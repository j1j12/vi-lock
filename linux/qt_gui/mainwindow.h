#ifndef ACCESS_CONTROL_MAINWINDOW_H
#define ACCESS_CONTROL_MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include "oneshot_gate.h"
#include "../app/v4l2_capture.h"
#include "rpmsg_worker.h"
#include "vision_worker.h"
#include "event_journal.h"
#include "admin_pin_worker.h"
#include "access_rules.h"
#include "status_worker.h"

class QLabel;
class AdminSession;
class QDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void updateFrame();
    void showPeople();
    void showEvents();
    void showSystemStatus();

private:
    void enrollPerson(const QString &operation, const QString &id, QDialog &parent, AdminSession &session);
    QLabel *cameraView_;
    QLabel *previewNote_;
    QLabel *cameraState_;
    QLabel *distanceState_;
    QLabel *recognitionState_;
    QLabel *userState_;
    QLabel *authorizationState_;
    QLabel *doorState_;
    QLabel *rpmsgState_;
    QTimer frameTimer_;
    QTimer armTimer_;
    QElapsedTimer monotonic_;
    OneShotGate armGate_;
    bool cycleReady_ = false;
    bool oneShot_ = false;
    bool managing_ = false;
    QString lastLinkEvent_;
    AccessClock accessClock_;
    EventJournal journal_{"/home/root/access-control-data"};
    AdminPinWorker adminPin_{"/home/root/access-control-data"};
    quint64 displayedSequence_ = 0;
    QSize displayedSize_;
    CameraWorker camera_;
    RecognitionWorker recognition_{camera_};
    RPMsgWorker rpmsgWorker_;
    StatusWorker statusWorker_;
};

#endif
