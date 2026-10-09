#include "store.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUuid>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QStringList>
#include <stdexcept>
#include <cstdlib>

namespace {
void require(bool ok, const char *message) { if(!ok) throw std::runtime_error(message); }
void fault(const char *point) {
#ifdef OUTBOX_TEST_FAULTS
    if(qgetenv("OUTBOX_TEST_CRASH")==point) std::_Exit(77);
#else
    Q_UNUSED(point);
#endif
}
}
void EventOutbox::exec(const QString &sql) {
    QSqlQuery query(db_); require(query.exec(sql),"SQLite statement failed");
}
int EventOutbox::scalar(const QString &sql) {
    QSqlQuery query(db_);require(query.exec(sql)&&query.next(),"SQLite count failed");return query.value(0).toInt();
}
EventOutbox::EventOutbox(const QString &path) : connection_(QUuid::createUuid().toString()) {
    require(QFileInfo(path).isAbsolute()&&!QFileInfo(path).isSymLink(),"require absolute non-symlink database path");
    db_=QSqlDatabase::addDatabase("QSQLITE",connection_);db_.setDatabaseName(path);
    try {
        require(db_.open(),"SQLite unavailable; no fallback to memory");
        require(QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner),"database permissions failed");
        exec("PRAGMA busy_timeout=1000");exec("PRAGMA foreign_keys=ON");
        require(scalar("PRAGMA foreign_keys")==1,"foreign keys unavailable");
        QSqlQuery mode(db_);require(mode.exec("PRAGMA journal_mode=DELETE")&&mode.next()&&mode.value(0).toString()=="delete","rollback journal required");mode.finish();
        exec("PRAGMA synchronous=FULL");require(scalar("PRAGMA synchronous")==2,"FULL synchronous required");
        const int version=scalar("PRAGMA user_version");
        require(version==0||version==2,"unsupported database version; use a new v2 database, do not overwrite v1");
        if(version==0) {
            require(scalar("SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")==0,"refuse unrelated database");
            exec("BEGIN IMMEDIATE");
            exec("CREATE TABLE events(seq INTEGER PRIMARY KEY AUTOINCREMENT, event_id TEXT NOT NULL UNIQUE, utc TEXT NOT NULL, kind TEXT NOT NULL, local_row TEXT NOT NULL)");
            exec("CREATE TABLE outbox(event_id TEXT PRIMARY KEY REFERENCES events(event_id) ON DELETE RESTRICT, ready INTEGER NOT NULL CHECK(ready IN (0,1)))");
            exec("PRAGMA user_version=2");exec("COMMIT");
        }
        // Prepare real columns even for an existing database; fail closed on schema damage.
        exec("SELECT e.seq,e.event_id,e.utc,e.kind,e.local_row,o.ready FROM events e JOIN outbox o ON e.event_id=o.event_id LIMIT 0");
        exec(QString("PRAGMA max_page_count=%1").arg(64*1024*1024/scalar("PRAGMA page_size")));
        QSqlQuery integrity(db_);require(integrity.exec("PRAGMA quick_check")&&integrity.next()&&integrity.value(0).toString()=="ok","database integrity failure");
    } catch(...) {
        db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(connection_);throw;
    }
}
EventOutbox::~EventOutbox() { db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(connection_); }
int EventOutbox::pendingCount(){return scalar("SELECT count(*) FROM outbox WHERE ready=1");}
int EventOutbox::deferredCount(){return scalar("SELECT count(*) FROM outbox WHERE ready=0");}
int EventOutbox::eventCount(){return scalar("SELECT count(*) FROM events");}
void EventOutbox::append(const QString &id,const QString &utc,const QString &kind) {
    require(QStringList{"startup","shutdown","access_allowed","access_denied","cycle_requested","cycle_finished"}.contains(kind),"event kind not approved for prototype");
    appendRow(id,utc,kind,QString(),true);
}
void EventOutbox::appendLocal(const QString &line,bool prepare) {
    const auto doc=QJsonDocument::fromJson(line.toUtf8());const auto row=doc.object();
    require(doc.isObject()&&line.toUtf8().size()<=8192,"invalid local event");
    const auto kind=row["type"].toString();
    require(!kind.isEmpty()&&kind.size()<=64,"invalid event type");
    const bool eligible=QStringList{"startup","shutdown","access_allowed","access_denied","cycle_requested","cycle_finished"}.contains(kind);
    appendRow(row["event_id"].toString(),row["time"].toString(),kind,line,prepare&&eligible);
}
QStringList EventOutbox::history() {
    QSqlQuery query(db_);require(query.exec("SELECT local_row FROM (SELECT seq,local_row FROM events WHERE local_row<>'' ORDER BY seq DESC LIMIT 500) ORDER BY seq"),"history read failed");
    QStringList result;while(query.next())result.append(query.value(0).toString());return result;
}
void EventOutbox::appendRow(const QString &id,const QString &utc,const QString &kind,const QString &local,bool prepare) {
    require(QRegularExpression("^evt-[a-f0-9]{32}$").match(id).hasMatch(),"invalid stable event ID");
    const auto time=QDateTime::fromString(utc,Qt::ISODateWithMs);
    require(utc.endsWith('Z')&&time.isValid()&&utc.size()<=32,"require UTC ISO timestamp");
    exec("BEGIN IMMEDIATE");
    try {
        QSqlQuery existing(db_);existing.prepare("SELECT utc,kind,local_row FROM events WHERE event_id=?");existing.addBindValue(id);
        require(existing.exec(),"ID lookup failed");
        if(existing.next()) {
            require(existing.value(0).toString()==utc&&existing.value(1).toString()==kind&&existing.value(2).toString()==local,"event ID payload conflict");
            exec("COMMIT");return; // never requeue an already acknowledged retained event
        }
        QSqlQuery insert(db_);insert.prepare("INSERT INTO events(event_id,utc,kind,local_row) VALUES(?,?,?,?)");
        insert.addBindValue(id);insert.addBindValue(utc);insert.addBindValue(kind);insert.addBindValue(local.isNull()?QStringLiteral(""):local);require(insert.exec(),"event insert failed");
        fault("after_event");
        if(prepare) {
            QSqlQuery pending(db_);pending.prepare("INSERT INTO outbox(event_id,ready) VALUES(?,?)");pending.addBindValue(id);pending.addBindValue(pendingCount()<128&&deferredCount()==0?1:0);require(pending.exec(),"outbox insert failed");
        }
        // Prune acknowledged history only, never an unsent event.
        exec("DELETE FROM events WHERE event_id NOT IN (SELECT event_id FROM outbox) AND seq NOT IN (SELECT seq FROM events ORDER BY seq DESC LIMIT 500)");
        fault("before_commit");exec("COMMIT");fault("after_commit");
    } catch(...) { QSqlQuery rollback(db_);rollback.exec("ROLLBACK");throw; }
}
QJsonObject EventOutbox::next() {
    QSqlQuery query(db_);
    require(query.exec("SELECT e.event_id,e.utc,e.kind FROM events e JOIN outbox o ON e.event_id=o.event_id WHERE o.ready=1 ORDER BY e.seq LIMIT 1"),"next read failed");
    if(!query.next())return {};
    return {{"schema_version",1},{"event_id",query.value(0).toString()},{"occurred_at",query.value(1).toString()},{"kind",query.value(2).toString()}};
}
bool EventOutbox::acknowledge(int status,const QByteArray &body) {
    if(status!=200||body.size()>2048)return false;
    const auto doc=QJsonDocument::fromJson(body);const auto ack=doc.object();
    if(!doc.isObject()||ack.size()!=2||!ack["event_id"].isString()||
       (ack["result"]!="stored"&&ack["result"]!="duplicate"))return false;
    exec("BEGIN IMMEDIATE");
    try {
        const auto head=next();
        if(head.isEmpty()||head["event_id"]!=ack["event_id"]){exec("ROLLBACK");return false;}
        QSqlQuery remove(db_);remove.prepare("DELETE FROM outbox WHERE event_id=?");remove.addBindValue(ack["event_id"].toString());
        require(remove.exec()&&remove.numRowsAffected()==1,"ACK delete failed");
        exec("UPDATE outbox SET ready=1 WHERE event_id IN (SELECT o.event_id FROM outbox o JOIN events e ON e.event_id=o.event_id WHERE o.ready=0 ORDER BY e.seq LIMIT max(0,128-(SELECT count(*) FROM outbox WHERE ready=1)))");
        fault("before_ack_commit");exec("COMMIT");fault("after_ack_commit");return true;
    }catch(...){QSqlQuery rollback(db_);rollback.exec("ROLLBACK");throw;}
}
