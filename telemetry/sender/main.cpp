#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLockFile>
#include <QProcess>
#include <QThread>
#include <QSet>
#include <QElapsedTimer>
#include <cstdio>
#include "queue.h"
#include "endpoint.h"
#include "tls_options.h"
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QCommandLineParser parser;parser.addHelpOption();
    parser.addOptions({{{"d","directory"},"Persistent queue directory","path","/home/root/access-control-probe-queue"},
        {"enqueue","Add one synthetic probe"},{"status","Show queue count and next ID"},
        {"send","Bounded send session; no daemon/autostart"},{"attempts","Maximum HTTP attempts (1..20)","n","3"},
        {"endpoint","Required for --send: https://<computer IPv4>:18766/probe","url"},
        {"cacert","Trusted CA PEM (absolute path)","path"},
        {"cert","Enrolled device certificate PEM (absolute path)","path"},
        {"key","Device private key PEM (absolute path, mode 600)","path"}});
    parser.process(app);
    try {
        if(int(parser.isSet("enqueue"))+int(parser.isSet("status"))+int(parser.isSet("send"))!=1) throw std::runtime_error("choose one of --enqueue/--status/--send");
        bool ok=false;int attempts=parser.value("attempts").toInt(&ok);
        if(!ok || attempts<1 || attempts>20) throw std::runtime_error("attempts must be 1..20");
        // Validate before opening/mutating the queue; never silently reuse a stale address.
        const QString endpoint=parser.isSet("send")?probeEndpoint(parser.value("endpoint")):QString();
        const QStringList tls=parser.isSet("send")?tlsOptions(parser.value("cacert"),parser.value("cert"),parser.value("key")):QStringList();
        QString dir=parser.value("directory");
        if(QFileInfo(dir).isSymLink() || !QDir().mkpath(dir) || !QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)) throw std::runtime_error("private queue directory unavailable");
        QLockFile lock(QDir(dir).filePath("sender.lock"));lock.setStaleLockTime(0);
        if(!lock.tryLock(0)) throw std::runtime_error("queue in use; do not remove active lock");
        ProbeQueue queue(dir);
        if(parser.isSet("enqueue")){std::printf("QUEUED %s\n",qPrintable(queue.enqueue()));return 0;}
        if(parser.isSet("status")){std::printf("pending=%d next=%s\n",queue.entries.size(),queue.entries.isEmpty()?"none":qPrintable(queue.entries.first().toObject()["event_id"].toString()));return 0;}
        for(int i=0;i<attempts && !queue.entries.isEmpty();++i) {
            int delay=queue.delaySeconds();std::printf("attempt=%d pending=%d wait=%ds\n",i+1,queue.entries.size(),delay);std::fflush(stdout);
            QThread::sleep(delay); // standalone sender, never blocks GUI/M4
            // -q must be the first curl option: ignore user curlrc (which could disable verification).
            QStringList args={"-q","--noproxy","*","--silent","--show-error","--connect-timeout","3","--max-time","8",
                "--max-filesize","2048","-H","Content-Type: application/json","--data-binary",QString::fromUtf8(queue.payload()),
                "--write-out","\n%{http_code}"};
            args+=tls;args+=endpoint;
            QProcess curl;curl.start("curl",args);
            QByteArray output;bool exceeded=false;
            if(curl.waitForStarted(2000)) {
                QElapsedTimer deadline;deadline.start();
                while(curl.state()!=QProcess::NotRunning) {
                    curl.waitForFinished(100);output+=curl.readAllStandardOutput();curl.readAllStandardError();
                    if(output.size()>4096 || deadline.elapsed()>12000){exceeded=true;curl.kill();curl.waitForFinished(2000);break;}
                }
                output+=curl.readAllStandardOutput();
            }
            const int split=output.lastIndexOf('\n');int status=split<0?0:output.mid(split+1).trimmed().toInt();
            bool accepted=!exceeded && curl.exitStatus()==QProcess::NormalExit && curl.exitCode()==0 &&
                queue.acknowledge(status,output.left(split));
            if(accepted)std::puts("ACK committed; removed from pending");
            else {queue.failed();std::printf("NO valid ACK; retained for retry (curl_exit=%d http=%d guard=%d)\n",curl.exitCode(),status,int(exceeded));}
        }
        std::printf("pending=%d\n",queue.entries.size());return queue.entries.isEmpty()?0:2;
    }catch(const std::exception &e){std::fprintf(stderr,"STOP: %s\n",e.what());return 1;}
}
