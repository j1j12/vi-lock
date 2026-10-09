#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QJsonDocument>
#include <QProcess>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QVariant>
#include <QUrl>
#include <QUuid>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif
#include "../outbox/store.h"
#include "../sender/tls_options.h"
#include "retry_policy.h"

static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
static QString endpoint(const QString &text) {
    QUrl u(text,QUrl::StrictMode);const auto parts=u.host().split('.');bool ipv4=parts.size()==4;
    for(const auto &part:parts){bool ok=false;int n=part.toInt(&ok);if(!ok||n<0||n>255||QString::number(n)!=part)ipv4=false;}
    bool allowed=ipv4&&(parts[0]=="10"||(parts[0]=="192"&&parts[1]=="168")||
                 (parts[0]=="172"&&parts[1].toInt()>=16&&parts[1].toInt()<=31));
    bool port=u.port()==18767;
#ifdef EVENT_SENDER_TEST
    if(u.host()=="127.0.0.1"){allowed=true;port=u.port()>0;}
#endif
    require(u.isValid()&&u.scheme()=="https"&&allowed&&port&&u.path()=="/events-test"&&
        !u.hasQuery()&&!u.hasFragment()&&u.userInfo().isEmpty()&&!text.contains('@'),
        "require https://<private IPv4>:18767/events-test");
    return u.toString(QUrl::FullyEncoded);
}

static void validateTestDatabase(const QString &path) {
    // Distinct SQLite application_id: refuse GUI/outbox databases, even if renamed.
    const QString name="synthetic-validation";
    bool ok=false;
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(path);
        if(db.open()) {
            QSqlQuery q(db);
            if(q.exec("PRAGMA application_id")&&q.next()) {
                int appId=q.value(0).toInt();q.finish();
                if(appId==1094927699)ok=true;
                else if(appId==0&&q.exec("SELECT count(*) FROM sqlite_master WHERE type='table'")&&q.next()&&q.value(0).toInt()==0) {
                    q.finish();ok=q.exec("PRAGMA application_id=1094927699");
                }
            }
            q.finish();db.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    require(ok,"not a synthetic sender database; refusing import or relabel");
    require(QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner),"private database permissions failed");
}

int main(int argc,char **argv) {
    // Plain stdout for provisioning checks, independent of Qt platform messaging.
    if(argc==2&&std::strcmp(argv[1],"--version")==0){std::puts("access_event_test_sender 2");return 0;}
    QCoreApplication app(argc,argv);app.setApplicationName("access_event_test_sender");app.setApplicationVersion("2");
    QCommandLineParser p;p.addHelpOption();p.addVersionOption();
    p.addOptions({{"enqueue","Generate one fictional startup event"},{"status","Read queue state"},
        {"send","Send at most one event; no daemon or automatic retry"},
        {"endpoint","Required HTTPS event-test endpoint","url"},
        {"cacert","CA PEM absolute path","path"},{"cert","Client PEM absolute path","path"},
        {"key","Client private PEM absolute path","path"}});
#ifdef EVENT_SENDER_TEST
    p.addOption({"test-directory","Host test only","path"});
#endif
    p.process(app);
    try {
        require(int(p.isSet("enqueue"))+int(p.isSet("status"))+int(p.isSet("send"))==1,"choose enqueue/status/send");
        QString url;QStringList tls;QString curlPath;
        if(p.isSet("send")) {
            url=endpoint(p.value("endpoint"));tls=tlsOptions(p.value("cacert"),p.value("cert"),p.value("key"));
            curlPath=QStandardPaths::findExecutable("curl");require(!curlPath.isEmpty(),"curl missing");
#ifdef EVENT_SENDER_TEST
            if(!qgetenv("EVENT_TEST_CURL").isEmpty())curlPath=QString::fromLocal8Bit(qgetenv("EVENT_TEST_CURL"));
#endif
        }
        QString directory="/home/root/access-control-event-test";
#ifdef EVENT_SENDER_TEST
        directory=p.value("test-directory");require(!directory.isEmpty(),"test directory required");
#endif
#ifdef Q_OS_UNIX
        umask(0077);
#endif
        require(QFileInfo(directory).isAbsolute()&&!QFileInfo(directory).isSymLink(),"invalid test directory");
        require(QDir().mkpath(directory)&&QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),"private test directory unavailable");
        QLockFile lock(QDir(directory).filePath("sender.lock"));lock.setStaleLockTime(0);
        if(!lock.tryLock(0)) {
            std::fprintf(stderr,"BUSY: sender locked; retained, try later\n");return 4;
        }
        const QString path=QDir(directory).filePath("synthetic-events.sqlite");
        for(const QString &suffix:{QString(),QString("-journal"),QString("-wal"),QString("-shm")})
            require(!QFileInfo(path+suffix).isSymLink(),"database symlink rejected");
        validateTestDatabase(path);EventOutbox store(path);
        if(p.isSet("enqueue")) {
            const auto id="evt-"+QUuid::createUuid().toString(QUuid::Id128);
            store.append(id,QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),"startup");
            std::printf("QUEUED synthetic %s\n",qPrintable(id));
        }
        int result=0;
        if(p.isSet("send")&&!store.next().isEmpty()) {
            auto payload=store.next();payload["synthetic"]=true;
            QStringList args={"-q","--noproxy","*","--silent","--show-error","--connect-timeout","3","--max-time","8",
                "--max-filesize","2048","-H","Content-Type: application/json","--data-binary",QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)),
                "--write-out","\n%{http_code}"};args+=tls;args+=url;
            QProcess curl;curl.start(curlPath,args);QByteArray output;bool guarded=false;
            bool started=curl.waitForStarted(2000);
            if(started) {
                QElapsedTimer clock;clock.start();
                while(curl.state()!=QProcess::NotRunning) {
                    curl.waitForFinished(100);output+=curl.readAllStandardOutput();curl.readAllStandardError();
                    if(output.size()>4096||clock.elapsed()>12000){guarded=true;curl.kill();curl.waitForFinished(2000);break;}
                }
                output+=curl.readAllStandardOutput();guarded=guarded||output.size()>4096;
            }
            const int split=output.lastIndexOf('\n');const int status=split<0?0:output.mid(split+1).trimmed().toInt();
            const bool transportOK=started&&!guarded&&curl.exitStatus()==QProcess::NormalExit&&curl.exitCode()==0;
#ifdef EVENT_SENDER_TEST
            if(transportOK&&status==200&&qgetenv("EVENT_TEST_DROP_ACK")=="1")std::_Exit(78);
#endif
            if(transportOK&&split>=0&&store.acknowledge(status,output.left(split)))std::puts("ACK committed; removed from pending");
            else {
                std::printf("RETAINED: curl_exit=%d http=%d guard=%d; no fallback\n",started?curl.exitCode():-1,status,int(guarded));
                if(status==409)std::puts("CONFLICT: inspect data; do not generate a new ID to bypass");
                result=sendFailureExit(started,guarded,started?curl.exitCode():-1,status);
                if(result==3)std::puts("PAUSE REQUIRED: check endpoint, credentials, clock or protocol; event retained");
            }
        }
        const auto head=store.next();
        std::printf("pending=%d deferred=%d next=%s\n",store.pendingCount(),store.deferredCount(),head.isEmpty()?"none":qPrintable(head["event_id"].toString()));
        return result;
    }catch(const std::exception &e){std::fprintf(stderr,"STOP: %s\n",e.what());return 1;}
}
