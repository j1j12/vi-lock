#include "access_rules.h"
#include "person_store.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTime>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {
void need(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
bool dateInRange(const QDate &d) { return d.isValid() && d.year() >= 2026 && d.year() <= 2099; }
QString timeText(int minute) { return QTime(minute / 60, minute % 60).toString("HH:mm"); }
QString readText(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QString();
    return QString::fromLatin1(file.read(128)).trimmed();
}
}
void AccessRule::validate() const {
    if (dates) need(dateInRange(first) && dateInRange(last) && first <= last, "invalid validity dates (2026-2099)");
    if (hours) need(startMinute >= 0 && startMinute < 1440 && endMinute >= 0 && endMinute < 1440 &&
                    startMinute != endMinute, "invalid daily window; use unrestricted for all day");
}
QJsonObject AccessRule::json() const {
    validate();
    QJsonObject o;
    o["dates"] = dates; o["first"] = dates ? first.toString(Qt::ISODate) : QString();
    o["last"] = dates ? last.toString(Qt::ISODate) : QString();
    o["hours"] = hours; o["start"] = hours ? startMinute : 0; o["end"] = hours ? endMinute : 0;
    return o;
}
AccessRule AccessRule::parse(const QJsonObject &o) {
    need(o.size() == 6 && o["dates"].isBool() && o["hours"].isBool() && o["first"].isString() &&
         o["last"].isString() && o["start"].isDouble() && o["end"].isDouble(), "invalid access rule schema");
    AccessRule r; r.dates = o["dates"].toBool(); r.hours = o["hours"].toBool();
    r.first = QDate::fromString(o["first"].toString(), Qt::ISODate);
    r.last = QDate::fromString(o["last"].toString(), Qt::ISODate);
    r.startMinute = o["start"].toInt(-1); r.endMinute = o["end"].toInt(-1);
    need(o["start"].toDouble() == r.startMinute && o["end"].toDouble() == r.endMinute, "noninteger window");
    r.validate();
    need(r.json() == o, "noncanonical access rule");
    return r;
}
QString AccessRule::summary() const {
    validate();
    return (dates ? first.toString("yyyy-MM-dd") + " 至 " + last.toString("yyyy-MM-dd") : "长期有效") +
           " / " + (hours ? timeText(startMinute) + "–" + timeText(endMinute) +
                    (startMinute > endMinute ? "（跨午夜）" : "") : "全天允许");
}
AccessVerdict evaluateAccess(const AccessRule &r, const QDateTime &utc, bool consistent) {
    try { r.validate(); } catch (...) { return {false, "访问规则无效"}; }
    if (!r.dates && !r.hours) return {true, "长期有效、全天允许"};
    if (!consistent || !utc.isValid() || !dateInRange(utc.toUTC().date())) return {false, "设备时间未通过一致性检查"};
    const auto local = utc.toOffsetFromUtc(8 * 3600);
    if (r.dates && local.date() < r.first) return {false, "人员有效期尚未开始"};
    if (r.dates && local.date() > r.last) return {false, "人员有效期已结束"};
    const int m = local.time().hour() * 60 + local.time().minute();
    const bool inWindow = !r.hours || (r.startMinute < r.endMinute ?
        (m >= r.startMinute && m < r.endMinute) : (m >= r.startMinute || m < r.endMinute));
    if (!inWindow) return {false, "当前不在允许访问时段"};
    return {true, "访问规则允许（UTC+08:00）"};
}
void AccessRuleStore::checkDirectory() const {
    const QFileInfo dir(directory_);
    need(dir.isDir() && !dir.isSymLink(), "rules directory missing or symlink");
#ifdef Q_OS_UNIX
    need(dir.ownerId() == uint(geteuid()) && !(dir.permissions() & (QFile::WriteGroup | QFile::WriteOther)), "unsafe rules directory permissions");
#endif
}
QJsonObject AccessRuleStore::read() const {
    checkDirectory();
    const QString path = QDir(directory_).filePath("access-rules.json");
    const QFileInfo info(path);
    need(info.isFile() && !info.isSymLink(), "access rules missing or symlink; authorization denied");
#ifdef Q_OS_UNIX
    need(info.ownerId() == uint(geteuid()) && !(info.permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)), "unsafe rules file permissions");
#endif
    QFile file(path);
    need(file.open(QIODevice::ReadOnly) && file.size() <= 262144, "cannot read access rules");
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    need(error.error == QJsonParseError::NoError && doc.isObject(), "invalid rules JSON");
    const auto root = doc.object();
    need(root.size() == 3 && root["version"].toDouble(-1) == 1 && root["zone"].toString() == "UTC+08:00" &&
         root["people"].isObject(), "unsupported rules format");
    const auto people = root["people"].toObject();
    need(people.size() <= 400, "access rule count exceeded");
    for (auto it = people.begin(); it != people.end(); ++it) {
        need(PersonStore::validId(it.key()) && it.value().isObject(), "invalid rule person");
        AccessRule::parse(it.value().toObject());
    }
    return people;
}
void AccessRuleStore::write(const QJsonObject &people) {
    checkDirectory();
    need(people.size() <= 400, "access rule count exceeded");
    const QString path = QDir(directory_).filePath("access-rules.json");
    need(!QFileInfo(path).isSymLink(), "refusing rules symlink");
    QJsonObject root; root["version"] = 1; root["zone"] = "UTC+08:00"; root["people"] = people;
    const auto data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    need(file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) &&
         file.write(data) == data.size() && file.commit(), "access rules commit failed");
}
void AccessRuleStore::initialize(const QStringList &ids) {
    checkDirectory();
    const QFileInfo info(QDir(directory_).filePath("access-rules.json"));
    if (info.exists() || info.isSymLink()) { read(); return; } // Never replace existing restrictions.
    QJsonObject people;
    for (const auto &id : ids) { need(PersonStore::validId(id), "invalid migration ID"); people[id] = AccessRule().json(); }
    write(people);
}
AccessRule AccessRuleStore::get(const QString &id) const {
    const auto people = read();
    need(PersonStore::validId(id) && people.contains(id), "person has no access rule; authorization denied");
    return AccessRule::parse(people[id].toObject());
}
void AccessRuleStore::set(const QString &id, const AccessRule &rule) {
    need(PersonStore::validId(id), "invalid rule ID");
    auto people = read(); people[id] = rule.json(); write(people);
}
QStringList AccessRuleStore::serialized(const QStringList &ids) const {
    const auto people = read();
    QStringList result;
    for (const auto &id : ids)
        result.append(people.contains(id) ? QString::fromUtf8(QJsonDocument(people[id].toObject()).toJson(QJsonDocument::Compact)) : QString());
    return result;
}
void AccessRuleStore::remove(const QString &id) {
    auto people = read(); people.remove(id); write(people);
}
AccessClock::AccessClock() : anchor_(QDateTime::currentMSecsSinceEpoch()) { elapsed_.start(); }
bool AccessClock::rtcAgrees(const QDateTime &utc, const QString &date, const QString &time, const QString &hctosys) {
    const auto rtc = QDateTime::fromString(date + "T" + time + "Z", Qt::ISODate);
    return hctosys == "1" && utc.isValid() && dateInRange(utc.toUTC().date()) && rtc.isValid() &&
           qAbs(rtc.msecsTo(utc)) <= 5000;
}
bool AccessClock::consistent(const QDateTime &utc) {
    if (qAbs((utc.toMSecsSinceEpoch() - anchor_) - elapsed_.elapsed()) > 2000) jumped_ = true;
    if (jumped_) return false; // Repair time then restart GUI; no silent resynchronization.
    const QString root = "/sys/class/rtc/rtc0/";
    const QString date = readText(root + "date"), time = readText(root + "time");
    if (date != readText(root + "date")) return false; // Midnight rollover: retry on a new recognition.
    return rtcAgrees(utc, date, time, readText(root + "hctosys"));
}
