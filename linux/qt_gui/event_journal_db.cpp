#include "event_journal.h"
#include "../../telemetry/outbox/store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QMutexLocker>
#include <memory>
#include <stdexcept>

void EventJournal::runTransactional() {
    std::unique_ptr<EventOutbox> store;
    bool failed=false;
    QStringList loaded;
    // Read old files only. Never replay them into outbox or rewrite their IDs.
    for(const QString &name:{QString("events.jsonl.1"),QString("events.jsonl")}) {
        QFile old(QDir(directory_).filePath(name));
        if(QFileInfo(old).isSymLink()||old.size()>2*1024*1024+8192||!old.open(QIODevice::ReadOnly))continue;
        while(!old.atEnd()) {
            auto line=old.readLine(8192);
            if(!line.endsWith('\n')){while(!old.atEnd()&&!line.endsWith('\n'))line=old.readLine(8192);continue;}
            if(!QJsonDocument::fromJson(line).isObject())continue;
            loaded.append(QString::fromUtf8(line.trimmed()));if(loaded.size()>500)loaded.removeFirst();
        }
    }
    try {
        if(QFileInfo(directory_).isSymLink()||!QDir().mkpath(directory_)||
           !QFile::setPermissions(directory_,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))
            throw std::runtime_error("event directory unavailable");
        store.reset(new EventOutbox(QDir(directory_).absoluteFilePath("events-v19.sqlite")));
        loaded.append(store->history());while(loaded.size()>500)loaded.removeFirst();
    }catch(const std::exception &) { failed=true; }
    {QMutexLocker lock(&mutex_);history_=loaded;}
    emit changed();
    emit storageHealth(failed?"事务日志不可用；新记录未持久化":"v19 本地事务日志就绪；真实上传关闭",failed);
    while(true) {
        QQueue<QString> batch;QStringList selected;bool overflow,doExport,doSelection;
        {QMutexLocker lock(&mutex_);batch.swap(pending_);overflow=overflow_;overflow_=false;
         doExport=exportPending_;exportPending_=false;doSelection=selectionPending_;selectionPending_=false;selected.swap(selection_);}
        if(overflow) {failed=true;emit storageHealth("日志内存队列溢出：部分事件未保存",true);}
        const bool changedBatch=!batch.isEmpty();
        while(!batch.isEmpty()) {
            const auto line=batch.dequeue();
            bool committed=false;
            if(store) {
                try {
                    // Local persistence ONLY. No capture opt-in and no network client in this path.
                    store->appendLocal(line,false);committed=true;
                }catch(const std::exception &) {failed=true;emit storageHealth("事务日志写入失败：部分事件未保存；请检查存储",true);}
            }
            if(committed){QMutexLocker lock(&mutex_);history_.append(line);if(history_.size()>500)history_.removeFirst();}
        }
        if(changedBatch){emit changed();if(!failed)emit storageHealth("v19 本批日志已持久化；真实上传关闭",false);}
        auto exportRows=[&](const QString &name,const QStringList &rows) {
            QSaveFile file(QDir(directory_).filePath(name));
            const auto bytes=(rows.isEmpty()?QString():rows.join('\n')+'\n').toUtf8();
            const bool ok=file.open(QIODevice::WriteOnly)&&file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)&&file.write(bytes)==bytes.size()&&file.commit();
            emit notice(ok?"已导出本地记录快照："+name:"本地记录导出失败");
        };
        if(doExport)exportRows("events-export.jsonl",snapshot());
        if(doSelection)exportRows("events-filtered.jsonl",selected);
        if(isInterruptionRequested()) {
            QMutexLocker lock(&mutex_);if(pending_.isEmpty()&&!exportPending_&&!selectionPending_)break;
        }
        msleep(100);
    }
    // SQLite connection is closed in this worker thread, never in the UI thread.
}
