#ifndef EVENT_JOURNAL_H
#define EVENT_JOURNAL_H
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QStringList>
#include <QElapsedTimer>

class EventJournal : public QThread {
    Q_OBJECT
public:
    explicit EventJournal(const QString &directory);
    void record(const QString &type, const QString &detail, const QString &person = QString());
    QStringList snapshot();
    bool exportHistory(); // Fixed local destination; no network, no face images.
    bool exportSelection(const QStringList &lines); // Immutable displayed snapshot, bounded.
signals:
    void changed();
    void notice(const QString &text);
    void storageHealth(const QString &text, bool failed);
protected:
    void run() override;
private:
    void runTransactional();
    bool transactional_ = false;
    QString directory_, session_;
    QElapsedTimer elapsed_;
    QMutex mutex_;
    QQueue<QString> pending_;
    QStringList history_;
    bool exportPending_ = false;
    bool overflow_ = false;
    bool selectionPending_ = false;
    QStringList selection_;
};
#endif
