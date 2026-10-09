#include "../event_journal.h"
#include "../../../telemetry/outbox/store.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <stdexcept>
static void check(bool ok){if(!ok)throw std::runtime_error("event DB assertion failed");}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);qputenv("ACCESS_CONTROL_EVENT_DB","1");
    try {
        QTemporaryDir dir;check(dir.isValid());
        const auto legacy=dir.filePath("events.jsonl");QFile file(legacy);check(file.open(QIODevice::WriteOnly));
        const QByteArray old="{\"type\":\"legacy\",\"detail\":\"preserve\"}\n";file.write(old);file.close();
        {
            EventJournal journal(dir.path());journal.record("access_allowed","private detail","test-person");journal.start();journal.requestInterruption();check(journal.wait(10000));
            check(journal.snapshot().size()==2);auto row=QJsonDocument::fromJson(journal.snapshot().last().toUtf8()).object();check(row.contains("event_id")&&row["person"]=="test-person");
        }
        check(file.open(QIODevice::ReadOnly)&&file.readAll()==old);file.close();
        {EventOutbox store(dir.filePath("events-v19.sqlite"));check(store.eventCount()==1&&store.pendingCount()==0&&store.deferredCount()==0);}
        {EventJournal restored(dir.path());restored.exportHistory();restored.start();restored.requestInterruption();check(restored.wait(10000)&&restored.snapshot().size()==2);}
        check(QFile::exists(dir.filePath("events-export.jsonl")));
        const auto blocked=dir.filePath("blocked");QFile bad(blocked);check(bad.open(QIODevice::WriteOnly));bad.write("not a directory");bad.close();
        {EventJournal failed(blocked);bool warned=false;
         QObject::connect(&failed,&EventJournal::storageHealth,&failed,[&](const QString &,bool error){if(error)warned=true;},Qt::DirectConnection);
         failed.record("startup","test");failed.start();failed.requestInterruption();check(failed.wait(10000)&&warned&&failed.snapshot().isEmpty());}
        std::puts("PASS: async DB write/restart/local-only/legacy preserved/export/failure notice/no false persisted history");
    }catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
