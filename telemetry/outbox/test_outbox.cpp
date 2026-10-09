#include "store.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QUuid>
#include <cstdio>
#include <stdexcept>
static void check(bool ok){if(!ok)throw std::runtime_error("assertion failed");}
static QString id(){return "evt-"+QUuid::createUuid().toString(QUuid::Id128);}
static QByteArray ack(const QString &id){return QJsonDocument(QJsonObject{{"event_id",id},{"result","duplicate"}}).toJson();}
static const QString utc="2026-10-05T03:00:00.000Z";
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        if(app.arguments().size()==4) {
            EventOutbox store(app.arguments()[1]);
            if(app.arguments()[3]=="ack")store.acknowledge(200,ack(app.arguments()[2]));
            else store.append(app.arguments()[2],utc,"access_allowed");
            return 0;
        }
        QTemporaryDir dir;check(dir.isValid());const QString path=dir.filePath("events.sqlite");
        QString first=id();
        { EventOutbox s(path);s.append(first,utc,"access_allowed");s.append(first,utc,"access_allowed");check(s.eventCount()==1&&s.pendingCount()==1);
          bool conflict=false;try{s.append(first,utc,"access_denied");}catch(...){conflict=true;}check(conflict);
          check(!s.acknowledge(200,ack(id()))&&!s.acknowledge(503,ack(first))); }
        { EventOutbox s(path);check(s.next()["event_id"]==first);check(s.acknowledge(200,ack(first)));s.append(first,utc,"access_allowed");check(s.pendingCount()==0&&s.eventCount()==1); }
        for(const QString &point: {QString("after_event"),QString("before_commit"),QString("after_commit"),QString("before_ack_commit"),QString("after_ack_commit")}) {
            std::printf("TEST crash point: %s\n",qPrintable(point));std::fflush(stdout);
            QString crashPath=dir.filePath(point+".sqlite"),eventId=id();bool isAck=point.contains("ack");
            if(isAck){EventOutbox s(crashPath);s.append(eventId,utc,"access_allowed");}
            QProcess child;auto env=QProcessEnvironment::systemEnvironment();env.insert("OUTBOX_TEST_CRASH",point);child.setProcessEnvironment(env);
            child.start(app.applicationFilePath(),{crashPath,eventId,isAck?"ack":"append"});
            bool finished=child.waitForFinished(10000);
            if(!finished||child.exitCode()!=77)std::fprintf(stderr,"child finished=%d exit=%d stderr=%s\n",int(finished),child.exitCode(),child.readAllStandardError().constData());
            check(finished&&child.exitCode()==77);
            EventOutbox recovered(crashPath);
            int expectedEvents=(isAck||point=="after_commit")?1:0;
            int expectedPending=(point=="after_commit"||point=="before_ack_commit")?1:0;
            check(recovered.eventCount()==expectedEvents&&recovered.pendingCount()==expectedPending);
            if(expectedPending)check(recovered.next()["event_id"]==eventId);
        }
        { EventOutbox s(dir.filePath("full.sqlite"));for(int i=0;i<128;++i)s.append(id(),utc,"startup");
          const auto deferred=id();s.append(deferred,utc,"startup");check(s.eventCount()==129&&s.pendingCount()==128&&s.deferredCount()==1);
          check(s.acknowledge(200,ack(s.next()["event_id"].toString())));check(s.pendingCount()==128&&s.deferredCount()==0); }
        { EventOutbox s(dir.filePath("local.sqlite"));const auto eventId=id();
          auto row=QJsonObject{{"event_id",eventId},{"time",utc},{"type","access_allowed"},{"person","private-person"},{"detail","local-only-detail"}};
          const auto line=QString::fromUtf8(QJsonDocument(row).toJson(QJsonDocument::Compact));
          s.appendLocal(line,false);check(s.pendingCount()==0&&s.eventCount()==1&&s.history().first()==line);
          row["event_id"]=id();s.appendLocal(QString::fromUtf8(QJsonDocument(row).toJson(QJsonDocument::Compact)),true);
          const auto payload=QJsonDocument(s.next()).toJson();check(!payload.contains("private-person")&&!payload.contains("local-only-detail")); }
        { EventOutbox s(dir.filePath("retention.sqlite"));for(int i=0;i<505;++i){auto eventId=id();s.append(eventId,utc,"startup");check(s.acknowledge(200,ack(eventId)));}check(s.eventCount()==500&&s.pendingCount()==0); }
        std::puts("PASS: atomic insert, stable ID, conflict, ACK identity, restart, 5 process-crash points, capacity, history retention");
    }catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
