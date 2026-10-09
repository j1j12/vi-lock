#ifndef ADMIN_PIN_WORKER_H
#define ADMIN_PIN_WORKER_H
#include <QThread>
#include <QMutex>
#include <QString>

class AdminPinWorker : public QThread {
    Q_OBJECT
public:
    explicit AdminPinWorker(const QString &directory) : directory_(directory) {}
    quint64 request(const QString &operation, const QString &pin = QString(), const QString &replacement = QString());
    void cancel(quint64 requestId);
signals:
    void reply(quint64 requestId, bool ok, const QString &message, int attempts = -1, qint64 lockedMs = 0);
    void audit(const QString &operation, bool ok, const QString &message);
protected:
    void run() override;
private:
    QString directory_;
    QMutex mutex_;
    bool busy_ = false, pending_ = false;
    quint64 serial_ = 0;
    quint64 cancelled_ = 0;
    QString operation_, pin_, replacement_;
};
#endif
