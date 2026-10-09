#ifndef ACCESS_CONTROL_RPMSG_WORKER_H
#define ACCESS_CONTROL_RPMSG_WORKER_H
#include <QThread>
#include <QString>
#include <atomic>

// Only this worker consumes the driver's receive queue while the GUI runs.
// Default mode sends PING only. Explicit manual bench mode also sends bounded
// servo commands/status queries; recognition never authorizes motion.
class RPMsgWorker : public QThread {
    Q_OBJECT
public:
    explicit RPMsgWorker(QObject *parent = nullptr) : QThread(parent) {}
    bool requestServo(quint32 pulse) {
        if (pulse != 1500 && pulse != 1600) return false;
        bool ready = true;
        if (!servoReady_.compare_exchange_strong(ready, false)) return false;
        servoPending_.store(static_cast<int>(pulse));
        return true;
    }
    bool requestCycle() {
        if (qgetenv("ACCESS_CONTROL_ONESHOT") != "1") return false;
        bool ready = true;
        if (!servoReady_.compare_exchange_strong(ready, false)) return false;
        servoPending_.store(2000); // Internal token, never a 2000us PWM request.
        return true;
    }
signals:
    void servoReady(bool ready);
    void servoStatus(const QString &text);
    void linkStatus(const QString &text);
    void presenceDetected();
    void doorStateReceived(quint32 state);
    void cycleFinished(); // Logical completion feedback, not physical position.
protected:
    void run() override;
private:
    std::atomic<bool> servoReady_{false};
    std::atomic<int> servoPending_{-1};
};
#endif
