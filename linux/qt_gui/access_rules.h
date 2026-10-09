#ifndef ACCESS_RULES_H
#define ACCESS_RULES_H
#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QStringList>

struct AccessRule {
    bool dates = false;
    QDate first, last; // Inclusive calendar dates in fixed UTC+08:00.
    bool hours = false;
    int startMinute = 0, endMinute = 0; // Start inclusive, end exclusive; wrap supported.
    void validate() const;
    QJsonObject json() const;
    static AccessRule parse(const QJsonObject &object);
    QString summary() const;
};
struct AccessVerdict {
    bool allowed;
    QString reason;
};
AccessVerdict evaluateAccess(const AccessRule &rule, const QDateTime &utc, bool timeConsistent);

class AccessRuleStore {
public:
    explicit AccessRuleStore(const QString &facesDirectory) : directory_(facesDirectory) {}
    // Explicit deployment-only migration; never called automatically by GUI.
    void initialize(const QStringList &existingIds);
    AccessRule get(const QString &id) const;
    QStringList serialized(const QStringList &ids) const;
    void set(const QString &id, const AccessRule &rule);
    void remove(const QString &id);
private:
    QJsonObject read() const;
    void write(const QJsonObject &people);
    void checkDirectory() const;
    QString directory_;
};

// Consistency guard, NOT authenticated network time or protection against root.
class AccessClock {
public:
    AccessClock();
    bool consistent(const QDateTime &utc);
    static bool rtcAgrees(const QDateTime &utc, const QString &date,
                          const QString &time, const QString &hctosys);
private:
    QElapsedTimer elapsed_;
    qint64 anchor_;
    bool jumped_ = false;
};
#endif
