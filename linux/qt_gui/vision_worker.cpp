#include "vision_worker.h"
#include "../app/v4l2_capture.h"
#include "../app/face_engine.h"
#include "person_store.h"
#include "access_rules.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <opencv2/imgproc.hpp>
#include "face_overlay.h"

namespace {
QImage detectedImage(const cv::Mat &frame, const std::vector<FaceBox> &faces) {
    cv::Mat rgb; cv::cvtColor(frame,rgb,cv::COLOR_BGR2RGB);
    QImage image(rgb.data,rgb.cols,rgb.rows,int(rgb.step),QImage::Format_RGB888);
    QVector<QRectF> boxes;
    for(const auto &f:faces) boxes.append(QRectF(f.x1,f.y1,f.x2-f.x1,f.y2-f.y1));
    return faceOverlay(image,boxes); // Deep-owned output; never draws on inference input.
}
}

void RecognitionWorker::publishDetection(quint64 session, const QImage &image, const QString &caption, qint64 inputAge) {
    QMutexLocker lock(&detectionMutex_);
    detectionSession_=session; detectionImage_=image; detectionCaption_=caption;
    detectionInputAge_=inputAge; detectionAge_.start();
}
bool RecognitionWorker::detectionSnapshot(quint64 session, QImage &image, QString &caption, qint64 &ageMs) {
    QMutexLocker lock(&detectionMutex_);
    if(detectionImage_.isNull() || detectionSession_!=session || !detectionAge_.isValid()) return false;
    ageMs=detectionInputAge_+detectionAge_.elapsed();
    if(ageMs>3000 || (session && !enrollment_.owns(session))) return false;
    image=detectionImage_; caption=detectionCaption_; return true;
}

bool CameraWorker::snapshot(cv::Mat &frame, quint64 &sequence, qint64 *ageMs)
{
    QMutexLocker lock(&mutex_);
    if (latest_.empty() || !latestAge_.isValid() || latestAge_.elapsed()>1000) return false;
    frame = latest_.clone();
    sequence = sequence_;
    if(ageMs) *ageMs=latestAge_.elapsed();
    return true;
}

void CameraWorker::run()
{
    V4L2Capture camera;
    try {
        if (!camera.open("/dev/video0", 640, 480)) {
            emit status("摄像头打开失败");
            return;
        }
        bool online = false;
        bool failureReported = false;
        while (!isInterruptionRequested()) {
            cv::Mat frame;
            if (!camera.read(frame)) {
                { QMutexLocker lock(&mutex_); latest_.release(); }
                if (!failureReported) emit status("取帧失败/超时");
                failureReported = true;
                online = false;
                msleep(20);
                continue;
            }
            { QMutexLocker lock(&mutex_); latest_ = frame; ++sequence_; latestAge_.start(); }
            if (!online) emit status(QString("在线 %1×%2").arg(frame.cols).arg(frame.rows));
            online = true;
            failureReported = false;
        }
    } catch (const std::exception &e) {
        emit status(QString("采集异常：%1").arg(e.what()));
    }
    QMutexLocker lock(&mutex_);
    latest_.release();
}

bool RecognitionWorker::trigger(quint64 ticket)
{
    if (!ready_.load()) return false;
    bool idle = false;
    if (!busy_.compare_exchange_strong(idle, true)) return false;
    ticket_ = ticket;
    operation_.clear();
    personId_.clear();
    pending_.store(true);
    return true;
}

bool RecognitionWorker::manage(const QString &operation, const QString &id, const QString &ruleJson, quint64 session)
{
    if (!ready_.load()) return false;
    bool idle = false;
    if (!busy_.compare_exchange_strong(idle, true)) return false;
    operation_ = operation;
    personId_ = id;
    ruleJson_ = ruleJson;
    session_ = session;
    ticket_ = 0;
    pending_.store(true);
    return true;
}

void RecognitionWorker::run()
{
    quint64 activeTicket = 0;
    try {
        emit status("加载模型和人员库");
        FaceEngine engine;
        if (!engine.init("/home/root/models/RFB-320.param", "/home/root/models/RFB-320.bin",
                         "/home/root/models/w600k_mbf.ncnn.param", "/home/root/models/w600k_mbf.ncnn.bin"))
            throw std::runtime_error("model load failed");
        PersonStore store("/home/root/faces");
        AccessRuleStore rules("/home/root/faces");
        const bool gated = qgetenv("ACCESS_CONTROL_SERVO_MANUAL") == "1" &&
                           qgetenv("ACCESS_CONTROL_ONESHOT") == "1";
        qInfo() << "Recognition ready; access rules v16; threshold unchanged (0.55)"
                << (gated ? "v13 decisions require explicit one-shot GUI gate" : "display-only; no unlock");
        ready_.store(true);
        emit status("模型就绪，等待雷达触发");
        while (!isInterruptionRequested()) {
            if (!pending_.exchange(false)) { msleep(50); continue; }
            if(operation_ == "preview") {
                // Same model/thread as recognition. No parallel ncnn or unbounded queue.
                try {
                    if(enrollment_.owns(session_)) {
                        cv::Mat frame; quint64 seq=0; qint64 captureAge=0;
                        QElapsedTimer age; age.start();
                        if(camera_.snapshot(frame,seq,&captureAge)) {
                            const auto faces=engine.detect(frame);
                            if(enrollment_.owns(session_)) publishDetection(session_,detectedImage(frame,faces),
                                QString("预览检测：%1张脸；不用于直接保存").arg(faces.size()),captureAge+age.elapsed());
                        }
                    }
                } catch(const std::exception &e) {
                    publishDetection(session_,QImage(),QString::fromUtf8(e.what()),0);
                }
                busy_.store(false); continue;
            }
            if (!operation_.isEmpty()) {
                bool ok = true;
                QString detail = "完成";
                try {
                    if (operation_ == "new" || operation_ == "update") {
                        if(!enrollment_.owns(session_)) throw std::runtime_error("enrollment cancelled or expired");
                        if (!PersonStore::validId(personId_)) throw std::runtime_error("invalid person ID");
                        cv::Mat frame;
                        quint64 base = 0, seq = 0;
                        camera_.snapshot(frame, base);
                        QElapsedTimer timer;
                        timer.start();
                        bool fresh = false;
                        while (!isInterruptionRequested() && timer.elapsed() < 2000) {
                            if (camera_.snapshot(frame, seq) && seq > base) { fresh = true; break; }
                            msleep(20);
                        }
                        if (!fresh) throw std::runtime_error("no fresh camera frame");
                        QElapsedTimer inputAge; inputAge.start();
                        const auto faces = engine.detect(frame);
                        publishDetection(session_,detectedImage(frame,faces),
                            QString("本次录入帧：%1张脸；正在校验/提取").arg(faces.size()),inputAge.elapsed());
                        if (faces.size() != 1) throw std::runtime_error("enrollment requires exactly one face");
                        const auto feature = engine.extract(frame, faces.front());
                        if (isInterruptionRequested() || !enrollment_.owns(session_)) throw std::runtime_error("enrollment cancelled before save");
                        if (operation_ == "new") {
                            for (const auto &person : store.load())
                                if (person.id == personId_) throw std::runtime_error("ID already exists");
                            // Publish rule first. Failure/termination before template leaves no matchable identity.
                            rules.set(personId_, AccessRule());
                        }
                        if(!enrollment_.owns(session_)) throw std::runtime_error("enrollment cancelled before template commit");
                        store.save(personId_, feature, operation_ == "update",[this]() {
                            return isInterruptionRequested() || !enrollment_.owns(session_);
                        });
                    } else if (operation_ == "enable" || operation_ == "disable") {
                        store.setEnabled(personId_, operation_ == "enable");
                    } else if (operation_ == "delete") { store.remove(personId_); rules.remove(personId_); }
                    else if (operation_ == "rule") {
                        bool exists = false;
                        for (const auto &person : store.load()) if (person.id == personId_) exists = true;
                        if (!exists) throw std::runtime_error("person no longer exists");
                        QJsonParseError error;
                        const auto doc = QJsonDocument::fromJson(ruleJson_.toUtf8(), &error);
                        if (error.error != QJsonParseError::NoError || !doc.isObject()) throw std::runtime_error("invalid rule request");
                        const auto rule = AccessRule::parse(doc.object());
                        rules.set(personId_, rule); detail = rule.summary();
                    }
                    else if (operation_ != "list") throw std::runtime_error("unknown management operation");
                    const auto records = store.load();
                    QStringList ids, states, policy;
                    for (const auto &person : records) {
                        ids.append(person.id);
                        states.append(person.enabled ? "启用" : "停用");
                    }
                    try { policy = rules.serialized(ids); }
                    catch (const std::exception &e) { detail += "；访问规则异常：" + QString::fromUtf8(e.what()); }
                    emit people(ids, states, policy);
                } catch (const std::exception &e) { ok = false; detail = QString::fromUtf8(e.what()); }
                const QString completedOperation = operation_, completedId = personId_;
                const quint64 completedSession=session_;
                busy_.store(false);
                emit managementResult(completedOperation, completedId, ok, detail);
                if(completedOperation=="new" || completedOperation=="update") emit enrollmentResult(completedSession,ok,detail);
                continue;
            }
            activeTicket = ticket_;
            bool matchedForAction = false;
            emit result("识别中", "--");
            QElapsedTimer elapsed;
            elapsed.start();
            cv::Mat frame;
            quint64 base = 0, seq = 0;
            camera_.snapshot(frame, base);
            bool fresh = false;
            // Require a newly captured frame after this request, not cached preview.
            while (!isInterruptionRequested() && elapsed.elapsed() < 2000) {
                if (camera_.snapshot(frame, seq) && seq > base) { fresh = true; break; }
                msleep(20);
            }
            QElapsedTimer frameAge; frameAge.start();
            QString text, who = "未识别";
            std::vector<PersonRecord> people;
            QString databaseError;
            try { people = store.load(); }
            catch (const std::exception &e) { databaseError = QString::fromUtf8(e.what()); }
            if (!databaseError.isEmpty()) text = "人员库错误，拒绝匹配：" + databaseError;
            else if (!fresh) text = "无新摄像头帧，请重新触发";
            else {
                emit result("人脸检测中", "--");
                const auto faces = engine.detect(frame);
                publishDetection(0,detectedImage(frame,faces),QString("检测帧：%1张脸；非实时画面").arg(faces.size()),frameAge.elapsed());
                if (faces.empty()) text = "未检测到人脸，请重新触发";
                else if (faces.size() != 1) text = "检测到多人，本次不匹配";
                else {
                    emit result("检测到单人：提取特征并比对", "--");
                    auto feature = engine.extract(frame, faces.front());
                    if (feature.size() != 512) throw std::runtime_error("invalid embedding size");
                    float best = -2;
                    QString candidate;
                    for (const auto &person : people) {
                        if (!person.enabled) continue;
                        float score = cosine_similarity(feature, person.feature);
                        if (std::isfinite(score) && score > best) { best = score; candidate = person.id; }
                    }
                    const bool matched = best > 0.55f; // Existing uncalibrated experimental threshold.
                    matchedForAction = matched;
                    if (matched) who = candidate;
                    text = QString("%1 %2（%3ms）").arg(matched ? "匹配" : "不匹配")
                               .arg(best, 0, 'f', 3).arg(elapsed.elapsed());
                    if (best <= -2) text = "没有可匹配的启用人员";
                }
            }
            qInfo() << "Recognition:" << text << who;
            emit result(text, who);
            emit decision(activeTicket, matchedForAction, matchedForAction ? who : QString());
            activeTicket = 0;
            busy_.store(false);
        }
    } catch (const std::exception &e) {
        emit decision(activeTicket, false, QString());
        emit result(QString("识别错误：%1").arg(e.what()), "未识别");
        qWarning() << "Recognition disabled:" << e.what();
    }
    ready_.store(false);
    busy_.store(false);
}
