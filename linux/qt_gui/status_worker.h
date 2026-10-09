#ifndef STATUS_WORKER_H
#define STATUS_WORKER_H
#include <QThread>
#include <QStringList>
#include <atomic>
class StatusWorker : public QThread {
    Q_OBJECT
public:
    bool request() { bool idle=false; return busy_.compare_exchange_strong(idle,true); }
    static QStringList collect(const QString &root = QString()); // root used by fixture tests only.
signals:
    void sample(const QStringList &values);
protected:
    void run() override;
private:
    std::atomic<bool> busy_{false};
};
#endif
