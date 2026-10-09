#pragma once
#include <QSqlDatabase>
#include <QJsonObject>
#include <QString>
#include <QStringList>

// One instance per owning worker thread. Not connected to GUI or network yet.
class EventOutbox {
public:
    explicit EventOutbox(const QString &path);
    ~EventOutbox();
    EventOutbox(const EventOutbox &) = delete;
    EventOutbox &operator=(const EventOutbox &) = delete;
    void append(const QString &eventId, const QString &utc, const QString &kind);
    void appendLocal(const QString &jsonLine, bool prepareUpload = false);
    QStringList history();
    QJsonObject next();
    bool acknowledge(int status, const QByteArray &body);
    int pendingCount();
    int eventCount();
    int deferredCount();
private:
    void appendRow(const QString &eventId,const QString &utc,const QString &kind,const QString &local,bool prepare);
    QString connection_;
    QSqlDatabase db_;
    void exec(const QString &sql);
    int scalar(const QString &sql);
};
