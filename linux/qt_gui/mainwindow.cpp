#include "mainwindow.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QDebug>
#include <QVBoxLayout>
#include <QWidget>
#include <QRegularExpression>
#include <opencv2/imgproc.hpp>
#include <exception>

namespace {
QLabel *makeValue(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumHeight(38);
    label->setStyleSheet("background:#263445;color:#f2f5f8;border-radius:6px;"
                         "font-size:17px;font-weight:600;padding:4px;");
    return label;
}

void addStatus(QGridLayout *layout, int row, const QString &name, QLabel *value)
{
    QLabel *title = new QLabel(name);
    title->setStyleSheet("color:#9fb1c5;font-size:16px;");
    layout->addWidget(title, row, 0);
    layout->addWidget(value, row, 1);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      cameraView_(new QLabel),
      cameraState_(makeValue("正在初始化")),
      distanceState_(makeValue("--")),
      recognitionState_(makeValue("等待触发")),
      userState_(makeValue("未识别")),
      authorizationState_(makeValue("待验证")),
      doorState_(makeValue("UNKNOWN")),
      rpmsgState_(makeValue("未连接"))
{
    monotonic_.start();
    oneShot_ = qgetenv("ACCESS_CONTROL_SERVO_MANUAL") == "1" &&
               qgetenv("ACCESS_CONTROL_ONESHOT") == "1";
    QWidget *root = new QWidget;
    root->setStyleSheet("background:#111922;");
    setCentralWidget(root);

    QLabel *heading = new QLabel("STM32MP157 智能门禁系统");
    heading->setStyleSheet("color:white;font-size:24px;font-weight:700;padding:4px;");

    cameraView_->setAlignment(Qt::AlignCenter);
    cameraView_->setMinimumSize(480, 300);
    cameraView_->setText("CAMERA");
    cameraView_->setStyleSheet("background:black;color:#667788;font-size:28px;"
                               "border:2px solid #2f4358;border-radius:8px;");

    QGridLayout *status = new QGridLayout;
    status->setHorizontalSpacing(12);
    status->setVerticalSpacing(6);
    addStatus(status, 0, "Camera", cameraState_);
    addStatus(status, 1, "Distance / Presence", distanceState_);
    addStatus(status, 2, "Recognition Result", recognitionState_);
    addStatus(status, 3, "User", userState_);
    addStatus(status, 4, "Authorization", authorizationState_);
    addStatus(status, 5, "Door State", doorState_);
    addStatus(status, 6, "RPMsg State", rpmsgState_);

    QHBoxLayout *content = new QHBoxLayout;
    auto *cameraColumn=new QVBoxLayout;
    previewNote_=new QLabel("实时画面；识别时展示同帧检测框");
    previewNote_->setStyleSheet("color:#cbd5e1;font-size:15px;"); previewNote_->setWordWrap(true);
    cameraColumn->addWidget(cameraView_,1); cameraColumn->addWidget(previewNote_);
    content->addLayout(cameraColumn, 3);
    content->addLayout(status, 2);

    QVBoxLayout *layout = new QVBoxLayout(root);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->addWidget(heading);
    auto *navigation = new QHBoxLayout;
    auto *peopleButton = new QPushButton("人员管理");
    auto *eventsButton = new QPushButton("事件记录");
    auto *systemButton = new QPushButton("系统状态");
    auto *storageStatus = new QLabel("v18 · 事件查询/统计");
    storageStatus->setWordWrap(true);
    storageStatus->setMaximumWidth(440);
    storageStatus->setStyleSheet("color:#cbd5e1;");
    for (auto *button : {peopleButton, eventsButton, systemButton}) {
        button->setMinimumHeight(38);
        button->setStyleSheet("background:#245580;color:white;font-size:18px;");
        navigation->addWidget(button);
    }
    navigation->addWidget(storageStatus, 2);
    layout->addLayout(navigation);
    connect(peopleButton, &QPushButton::clicked, this, &MainWindow::showPeople);
    connect(eventsButton, &QPushButton::clicked, this, &MainWindow::showEvents);
    connect(systemButton, &QPushButton::clicked, this, &MainWindow::showSystemStatus);
    statusWorker_.start();
    connect(&journal_, &EventJournal::notice, storageStatus, [storageStatus](const QString &text) {
        if(!storageStatus->property("storageFailed").toBool())storageStatus->setText(text);
    });
    connect(&journal_, &EventJournal::storageHealth, storageStatus, [storageStatus](const QString &text,bool failed) {
        if(failed)storageStatus->setProperty("storageFailed",true);
        if(failed||!storageStatus->property("storageFailed").toBool())storageStatus->setText(text);
        if(failed)storageStatus->setStyleSheet("color:#ff8a80;");
    });
    journal_.start();
    journal_.record("startup", qgetenv("ACCESS_CONTROL_EVENT_DB")=="1"
        ? "GUI v19; local transactional journal; real telemetry disabled"
        : "GUI v18-compatible journal; PIN + access rules; explicit one-shot");
    connect(&adminPin_, &AdminPinWorker::audit, this, [this](const QString &operation, bool ok, const QString &message) {
        journal_.record("admin_" + operation, (ok ? "成功：" : "拒绝：") + message);
    });
    adminPin_.start();
    qInfo() << "Admin PIN v15: personnel authentication required; idle timeout 120s; no default PIN";
    connect(&recognition_, &RecognitionWorker::managementResult, this,
            [this](const QString &operation, const QString &id, bool ok, const QString &detail) {
        if (operation != "list" || !ok)
            journal_.record("person_" + operation, (ok ? "成功：" : "失败：") + detail, id);
    });
    layout->addLayout(content, 1);
    if (qgetenv("ACCESS_CONTROL_SERVO_MANUAL") == "1") {
        qInfo() << (oneShot_ ? "Servo GUI bench v13: explicitly armed one-shot mode" :
                               "Servo GUI bench v11: manual only; no automatic authorization");
        doorState_->setWordWrap(true);
        auto *buttons = new QHBoxLayout;
        auto *closeServo = new QPushButton("模拟关闭 1500us");
        auto *openServo = new QPushButton("模拟打开 1600us");
        for (auto *button : {closeServo, openServo}) {
            button->setMinimumHeight(48);
            button->setStyleSheet("QPushButton{background:#245580;color:white;font-size:18px;} QPushButton:disabled{background:#333;color:#888;}");
            button->setEnabled(false);
            buttons->addWidget(button);
            connect(&rpmsgWorker_, &RPMsgWorker::servoReady, button, &QPushButton::setEnabled);
        }
        layout->addLayout(buttons);
        connect(&rpmsgWorker_, &RPMsgWorker::servoStatus, doorState_, &QLabel::setText);
        connect(&rpmsgWorker_, &RPMsgWorker::servoStatus, this, [this](const QString &text) {
            qInfo() << "Servo GUI:" << text;
            journal_.record("m4_servo_status", text);
        });
        auto command = [this, closeServo, openServo](quint32 pulse) {
            closeServo->setEnabled(false);
            openServo->setEnabled(false);
            if (!rpmsgWorker_.requestServo(pulse)) doorState_->setText("尚未就绪，请等待状态确认");
        };
        connect(closeServo, &QPushButton::clicked, this, [command]() { command(1500); });
        connect(openServo, &QPushButton::clicked, this, [command]() { command(1600); });
        distanceState_->setText("等待人员事件（需雷达固件）");
        if (oneShot_) {
            closeServo->hide();
            openServo->hide();
            auto *arm = new QPushButton("允许下一次识别动作");
            auto *cancel = new QPushButton("取消等待");
            for (auto *button : {arm, cancel}) {
                button->setMinimumHeight(48);
                button->setStyleSheet("QPushButton{background:#245580;color:white;font-size:18px;} QPushButton:disabled{background:#333;color:#888;}");
                button->setEnabled(false);
                buttons->addWidget(button);
            }
            connect(&rpmsgWorker_, &RPMsgWorker::servoReady, this, [this](bool ready) {
                cycleReady_ = ready;
                if (!ready && armGate_.active()) {
                    armGate_.cancel();
                    authorizationState_->setText("状态失效，已解除启用");
                    journal_.record("gate_invalidated", "M4状态失效，已解除启用");
                }
            });
            connect(arm, &QPushButton::clicked, this, [this, arm]() {
                if (armGate_.arm(monotonic_.elapsed(), cycleReady_ && recognition_.canTrigger())) {
                    authorizationState_->setText("已启用一次：30秒内重新触发雷达");
                    arm->setEnabled(false);
                    qInfo() << "One-shot armed; awaiting NEW radar trigger";
                    journal_.record("gate_armed", "30秒内等待新的雷达触发");
                }
            });
            connect(cancel, &QPushButton::clicked, this, [this]() {
                armGate_.cancel();
                authorizationState_->setText("已取消等待，不动作");
                journal_.record("gate_cancelled", "用户取消等待");
            });
            connect(&armTimer_, &QTimer::timeout, this, [this, arm, cancel]() {
                if (armGate_.expire(monotonic_.elapsed())) {
                    authorizationState_->setText("启用已超时，不动作");
                    journal_.record("gate_expired", "30秒超时，不动作");
                }
                arm->setEnabled(!managing_ && cycleReady_ && !armGate_.active() && recognition_.canTrigger());
                cancel->setEnabled(armGate_.active());
            });
            armTimer_.start(100);
            qInfo() << "One-shot v13: disabled until user arms; 30s lifetime";
        }
    }

    connect(&rpmsgWorker_, &RPMsgWorker::linkStatus, rpmsgState_, &QLabel::setText);
    connect(&rpmsgWorker_, &RPMsgWorker::linkStatus, this, [this](const QString &text) {
        // Do not fill disk with the heartbeat counter.
        QString state = text;
        state.replace(QRegularExpression("PONG\\s*\\d+"), "PONG");
        if (state != lastLinkEvent_) { journal_.record("rpmsg", state); lastLinkEvent_ = state; }
    });
    connect(&rpmsgWorker_, &RPMsgWorker::presenceDetected, this, [this]() {
        distanceState_->setText("检测到有人");
        if (managing_) return;
        quint64 ticket = oneShot_ ? armGate_.claim(monotonic_.elapsed()) : 0;
        if (recognition_.trigger(ticket)) {
            recognitionState_->setText("收到触发，等待新帧");
            userState_->setText("--");
            if (ticket) authorizationState_->setText("本次识别处理中；不再接受新触发");
            journal_.record("recognition_trigger", ticket ? "单次授权识别" : "仅显示识别");
        } else if (ticket) {
            armGate_.cancel();
            authorizationState_->setText("识别未就绪，已解除启用");
            journal_.record("gate_rejected", "识别未就绪");
        }
    });
    connect(&rpmsgWorker_, &RPMsgWorker::doorStateReceived, this, [this](quint32 state) {
        doorState_->setText(QString("M4逻辑状态 %1；未验证到位").arg(state));
        journal_.record("m4_door_state", QString("逻辑状态 %1；无物理到位反馈").arg(state));
    });
    connect(&rpmsgWorker_, &RPMsgWorker::cycleFinished, this, [this]() {
        journal_.record("cycle_finished", "M4接受动作后返回空闲；逻辑周期结束，不代表物理到位；未关联人员");
    });
    rpmsgWorker_.start();

    authorizationState_->setText(oneShot_ ? "未启用：识别仅显示" : "仅显示，不授权");
    authorizationState_->setWordWrap(true);
    connect(&recognition_, &RecognitionWorker::decision, this, [this](quint64 ticket, bool matched, const QString &personId) {
        AccessVerdict verdict{false, "未匹配到启用人员"};
        if (matched) {
            journal_.record("recognition_match", "身份特征匹配；尚不代表规则允许或开门", personId);
            try {
                const auto rule = AccessRuleStore("/home/root/faces").get(personId);
                const auto now = QDateTime::currentDateTimeUtc();
                verdict = evaluateAccess(rule, now, (!rule.dates && !rule.hours) || accessClock_.consistent(now));
            } catch (const std::exception &e) { verdict = {false, "访问规则错误，拒绝授权：" + QString::fromUtf8(e.what())}; }
            // Re-evaluate at GUI dispatch time, not at image capture time.
            journal_.record(verdict.allowed ? "access_allowed" : "access_denied", verdict.reason, personId);
            if (!armGate_.owns(ticket))
                authorizationState_->setText((verdict.allowed ? "规则允许；未启用，不动作" : "规则拒绝：" + verdict.reason));
        }
        if (!oneShot_ || !armGate_.owns(ticket)) return;
        bool permit = armGate_.finish(ticket, matched && verdict.allowed, monotonic_.elapsed());
        if (permit && !managing_ && cycleReady_ && rpmsgWorker_.requestCycle()) {
            cycleReady_ = false;
            authorizationState_->setText("一次性动作已请求；启用已解除");
            qInfo() << "One-shot matched: cycle requested; ticket consumed";
            journal_.record("cycle_requested", "匹配、访问规则与门控允许；已请求，不代表到位", personId);
        } else {
            authorizationState_->setText(!verdict.allowed ? verdict.reason + "；本轮不动作" : "门控过期/未就绪；本轮不动作");
            qInfo() << "One-shot consumed without motion";
            journal_.record("cycle_rejected", !verdict.allowed ? verdict.reason : "门控过期/未就绪；本轮不动作", personId);
        }
    });
    connect(&camera_, &CameraWorker::status, cameraState_, &QLabel::setText);
    connect(&camera_, &CameraWorker::status, this, [this](const QString &text) { journal_.record("camera", text); });
    connect(&recognition_, &RecognitionWorker::status, recognitionState_, &QLabel::setText);
    connect(&recognition_, &RecognitionWorker::result, this, [this](const QString &text, const QString &name) {
        recognitionState_->setText(text);
        userState_->setText(name);
        if (text != "识别中") journal_.record("recognition_result", text, name);
    });
    camera_.start();
    recognition_.start();
    connect(&frameTimer_, &QTimer::timeout, this, &MainWindow::updateFrame);
    frameTimer_.start(66);
}

MainWindow::~MainWindow()
{
    armTimer_.stop();
    armGate_.cancel();
    frameTimer_.stop();
    rpmsgWorker_.requestInterruption();
    recognition_.requestInterruption();
    camera_.requestInterruption();
    rpmsgWorker_.wait();
    recognition_.wait();
    camera_.wait();
    adminPin_.requestInterruption();
    adminPin_.wait();
    statusWorker_.requestInterruption(); statusWorker_.wait();
    journal_.record("shutdown", "GUI正常退出");
    journal_.requestInterruption();
    journal_.wait();
}

void MainWindow::updateFrame()
{
    if(managing_) return; // Modal enrollment has its own bounded live preview.
    cv::Mat bgr;
    quint64 seq = 0;
    if (!camera_.snapshot(bgr, seq)) {
        cameraView_->clear();
        previewNote_->setText("暂无新摄像头帧；不显示旧检测框");
        displayedSequence_ = 0;
        return;
    }
    QImage detected; QString caption; qint64 age=0;
    if(!managing_ && recognition_.detectionSnapshot(0,detected,caption,age)) {
        cameraView_->setPixmap(QPixmap::fromImage(detected).scaled(cameraView_->size(),Qt::KeepAspectRatio,Qt::FastTransformation));
        previewNote_->setText(caption+QString(" · 帧龄%1ms").arg(age));
        displayedSequence_=0; return;
    }
    previewNote_->setText("实时画面；检测框仅显示在对应检测帧上");
    // Repaint the same frame only if the target size changed.
    if (seq == displayedSequence_ && cameraView_->size() == displayedSize_)
        return;

    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                 QImage::Format_RGB888);
    // rgb stays alive until synchronous QPixmap conversion has completed.
    cameraView_->setPixmap(QPixmap::fromImage(image).scaled(
        cameraView_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    displayedSequence_ = seq;
    displayedSize_ = cameraView_->size();
}
