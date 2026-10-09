#include "mainwindow.h"
#include "admin_session.h"
#include "diagnostic_views.h"
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPixmap>
#include <opencv2/imgproc.hpp>

void MainWindow::enrollPerson(const QString &operation, const QString &id, QDialog &parent, AdminSession &session) {
    if(!session.active()) return;
    const quint64 token=recognition_.beginEnrollment();
    if(!token) return;
    EnrollmentView dialog(operation,id,&parent);
    auto *live=dialog.live, *detected=dialog.detected;
    auto *detectionText=dialog.detectionText, *message=dialog.message;
    auto *capture=dialog.capture, *close=dialog.closeButton;
    bool waiting=false, inflight=false, previewInFlight=false;
    QElapsedTimer lastPreview; lastPreview.start();
    connect(capture,&QPushButton::clicked,&dialog,[&]() {
        if(!session.active() || waiting || inflight) return;
        waiting=true; capture->setEnabled(false); message->setText("等待本次预览结束，随后采集新帧…");
    });
    connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);
    connect(&recognition_,&RecognitionWorker::enrollmentResult,&dialog,[&](quint64 resultToken,bool ok,const QString &detail) {
        if(resultToken!=token) return;
        inflight=false;
        message->setText((ok ? "已保存，可返回人员管理：" : "未保存，可重试：")+detail);
        capture->setEnabled(!ok && session.active());
    });
    QTimer timer;
    connect(&timer,&QTimer::timeout,&dialog,[&]() {
        if(!session.active()) { dialog.reject(); return; }
        if(previewInFlight && recognition_.canTrigger()) { previewInFlight=false; lastPreview.restart(); }
        cv::Mat frame; quint64 seq=0;
        const bool cameraOk=camera_.snapshot(frame,seq);
        if(cameraOk) {
            cv::Mat rgb; cv::cvtColor(frame,rgb,cv::COLOR_BGR2RGB);
            QImage image(rgb.data,rgb.cols,rgb.rows,int(rgb.step),QImage::Format_RGB888);
            live->setPixmap(QPixmap::fromImage(image).scaled(live->size(),Qt::KeepAspectRatio,Qt::FastTransformation));
        } else { live->clear(); live->setText("暂无新画面 / 摄像头异常"); }
        QImage image; QString caption; qint64 age=0;
        if(cameraOk && recognition_.detectionSnapshot(token,image,caption,age)) {
            detected->setPixmap(QPixmap::fromImage(image).scaled(detected->size(),Qt::KeepAspectRatio,Qt::FastTransformation));
            detectionText->setText(caption+QString(" · 帧龄%1ms").arg(age));
        } else { detected->clear(); detected->setText("等待有效检测帧（旧框已清除）"); }
        if(waiting && recognition_.canTrigger()) {
            if(recognition_.manage(operation,id,QString(),token)) {
                waiting=false; inflight=true; message->setText("新帧检测 → 单人确认 → 特征提取 → 保存；可取消未提交操作。");
            }
        } else if(!waiting && !inflight && cameraOk && lastPreview.elapsed()>=1000 && recognition_.canTrigger()) {
            if(recognition_.manage("preview",QString(),QString(),token)) { lastPreview.restart(); previewInFlight=true; }
        }
    });
    timer.start(100);
    dialog.exec();
    timer.stop(); recognition_.cancelEnrollment(token);
    // A completed atomic commit cannot be undone by closing later.
}
