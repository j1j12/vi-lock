#include <QCoreApplication>
#include <QTemporaryDir>
#include <QLockFile>
#include <cstdio>
#include "queue.h"
#include "endpoint.h"
#include "tls_options.h"
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    try {
        check(probeEndpoint("https://192.168.101.100:18766/probe")=="https://192.168.101.100:18766/probe","TLS endpoint");
        check(!probeEndpoint("https://10.0.0.2:18766/probe").isEmpty(),"private endpoint");
        for(const QString &bad:{QString(),QString("http://example.com:18765/probe"),QString("http://192.168.101.100:18765/unlock"),
            QString("http://user@192.168.101.100:18765/probe"),QString("http://192.168.101.100:18765/probe?x=1"),
            QString("http://192.168.101.100:18765/probe#x"),QString("http://8.8.8.8:18765/probe"),
            QString("http://192.168.101.100:18765/probe"),QString("https://8.8.8.8:18766/probe"),
            QString("https://192.168.101.100:18765/probe"),QString("https://user@192.168.101.100:18766/probe"),
            QString("https://192.168.101.100:18766/probe?x=1")}) {
            bool rejected=false;try{probeEndpoint(bad);}catch(...){rejected=true;}check(rejected,"reject unsafe/missing endpoint");
        }
        QTemporaryDir dir;check(dir.isValid(),"temp");
        for(const QString &missing:{QString(),QString("relative.pem"),dir.filePath("absent.pem")}) {
            bool rejected=false;try{tlsFile(missing,true);}catch(...){rejected=true;}check(rejected,"missing TLS file rejected");
        }
        QFile fixture(dir.filePath("fixture.pem"));check(fixture.open(QIODevice::WriteOnly),"fixture");
        fixture.write("test-only-not-a-certificate");fixture.close();fixture.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
        const auto options=tlsOptions(fixture.fileName(),fixture.fileName(),fixture.fileName());
        check(options.contains("--cacert")&&options.contains("--cert")&&options.contains("--key")&&
              options.contains("=https")&&!options.contains("-k"),"TLS verification options");
        QString id;{ProbeQueue q(dir.path());id=q.enqueue();q.failed();check(q.delaySeconds()==2,"backoff");}
        ProbeQueue q(dir.path());check(q.entries.size()==1,"restart retained");
        check(!q.acknowledge(200,"{}"),"missing ACK");
        check(!q.acknowledge(200,"{\"event_id\":\"wrong\",\"result\":\"stored\"}"),"wrong ID");
        auto ack=QJsonDocument(QJsonObject{{"event_id",id},{"result","duplicate"}}).toJson();
        check(!q.acknowledge(503,ack),"HTTP failure");check(q.entries.size()==1,"failures retain");
        check(q.acknowledge(200,ack),"lost ACK duplicate accepted");check(ProbeQueue(dir.path()).entries.isEmpty(),"removal persisted");
        for(int i=0;i<ProbeQueue::capacity;++i)q.enqueue();
        bool full=false;try{q.enqueue();}catch(...){full=true;}check(full&&ProbeQueue(dir.path()).entries.size()==128,"capacity preserves old records");
        for(int i=0;i<10;++i)q.failed();check(q.delaySeconds()==60,"backoff bounded");
        QLockFile a(dir.filePath("lock")),b(dir.filePath("lock"));check(a.tryLock(0)&&!b.tryLock(0),"exclusive lock");
        QFile bad(q.path);check(bad.open(QIODevice::WriteOnly|QIODevice::Truncate),"corrupt fixture");bad.write("broken");bad.close();
        bool refused=false;try{ProbeQueue broken(dir.path());}catch(...){refused=true;}check(refused,"corruption fails closed");
        std::puts("PASS: persistence/ACK identity/duplicate/capacity/backoff/lock/corruption");
    }catch(const std::exception &e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
