#include "event_journal.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QUuid>

EventJournal::EventJournal(const QString &directory) : directory_(directory),
    session_(QUuid::createUuid().toString()) {
    transactional_ = qgetenv("ACCESS_CONTROL_EVENT_DB") == "1";
    elapsed_.start();
}

void EventJournal::record(const QString &type, const QString &detail, const QString &person) {
    QJsonObject obj;
    obj["time"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    if(transactional_) {
        obj["event_id"] = "evt-" + QUuid::createUuid().toString(QUuid::Id128);
        obj["time"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }
    obj["session"] = session_;
    obj["elapsed_ms"] = double(elapsed_.elapsed());
    obj["type"] = type.left(64);
    obj["person"] = person.left(48);
    obj["detail"] = detail.left(512);
    const QString line = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    QMutexLocker lock(&mutex_);
    if (pending_.size() >= 512) { overflow_ = true; return; }
    pending_.enqueue(line);
}
QStringList EventJournal::snapshot() { QMutexLocker lock(&mutex_); return history_; }
bool EventJournal::exportHistory() {
    QMutexLocker lock(&mutex_);
    if (exportPending_) return false;
    exportPending_ = true;
    return true;
}
void EventJournal::run() {
    if(transactional_) { runTransactional(); return; }
    const QString current = QDir(directory_).filePath("events.jsonl");
    const QString previous = current + ".1";
    bool writable = QDir().mkpath(directory_);
    if (writable) writable = QFile::setPermissions(directory_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    // Retain at most 500 valid rows in memory; bounded line reads even if a file
    // has been manually corrupted. Archives >2MiB are not loaded.
    QStringList loaded;
    for (const QString &path : {previous, current}) {
        QFile file(path);
        if (file.size() > 2 * 1024 * 1024 + 8192 || !file.open(QIODevice::ReadOnly)) continue;
        while (!file.atEnd()) {
            QByteArray line = file.readLine(8192);
            if (!line.endsWith('\n')) { while (!file.atEnd() && !line.endsWith('\n')) line = file.readLine(8192); continue; }
            if (!QJsonDocument::fromJson(line).isObject()) continue;
            loaded.append(QString::fromUtf8(line.trimmed()));
            if (loaded.size() > 500) loaded.removeFirst();
        }
    }
    { QMutexLocker lock(&mutex_); history_ = loaded; }
    emit changed();
    // Separate a crash-truncated last row from future complete JSON rows. This
    // does not claim recovery of the truncated event or power-loss durability.
    if (writable && QFileInfo(current).size() > 0) {
        QFile tail(current);
        if (!tail.open(QIODevice::ReadWrite) || !tail.seek(tail.size() - 1)) writable = false;
        else if (tail.read(1) != "\n") {
            writable = tail.write("\n", 1) == 1 && tail.flush();
            emit notice("检测到不完整日志尾行；已尝试隔离，缺失事件不可恢复");
        }
    }
    if (!writable) emit notice("事件目录不可写：仅保留内存记录");
    while (true) {
        QQueue<QString> batch;
        bool doExport, overflow, doSelection;
        QStringList selected;
        { QMutexLocker lock(&mutex_); batch.swap(pending_); doExport = exportPending_;
          exportPending_ = false; overflow = overflow_; overflow_ = false;
          doSelection=selectionPending_; selectionPending_=false; selected.swap(selection_); }
        if (overflow) emit notice("事件队列已满，部分记录丢失");
        const bool hadEvents = !batch.isEmpty();
        while (!batch.isEmpty()) {
            const QString line = batch.dequeue();
            { QMutexLocker lock(&mutex_); history_.append(line); if (history_.size() > 500) history_.removeFirst(); }
            if (!writable) continue;
            if (QFileInfo(current).size() >= 2 * 1024 * 1024) {
                if ((QFileInfo::exists(previous) && !QFile::remove(previous)) || !QFile::rename(current, previous)) {
                    writable = false;
                    emit notice("事件轮换失败：仅保留内存记录");
                    continue;
                }
            }
            QFile file(current);
            QByteArray data = line.toUtf8() + '\n';
            if (!file.open(QIODevice::WriteOnly | QIODevice::Append) ||
                !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
                file.write(data) != data.size() || !file.flush()) {
                writable = false;
                emit notice("事件写入失败：仅保留内存记录，请检查磁盘");
            }
        }
        if (hadEvents) emit changed();
        if (doExport) {
            const QString destination = QDir(directory_).filePath("events-export.jsonl");
            QSaveFile file(destination);
            const QByteArray data = (snapshot().join('\n') + '\n').toUtf8();
            bool ok = file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) &&
                      file.write(data) == data.size() && file.commit();
            emit notice(ok ? "已导出最近500条以内记录：" + destination : "事件导出失败");
        }
        if (isInterruptionRequested()) {
            QMutexLocker lock(&mutex_);
            if (pending_.isEmpty() && !exportPending_ && !selectionPending_ && !doSelection) break;
        }
        if(doSelection) {
            const QString destination=QDir(directory_).filePath("events-filtered.jsonl");
            QSaveFile file(destination);
            const auto data=(selected.isEmpty()?QString():selected.join('\n')+'\n').toUtf8();
            const bool ok=file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) &&
                file.write(data)==data.size() && file.commit();
            emit notice(ok?QString("已导出筛选快照%1条：%2").arg(selected.size()).arg(destination):"筛选导出失败");
        }
        msleep(100);
    }
}

bool EventJournal::exportSelection(const QStringList &lines) {
    if(lines.size()>500) return false;
    for(const auto &line:lines) if(line.toUtf8().size()>8192 || !QJsonDocument::fromJson(line.toUtf8()).isObject()) return false;
    QMutexLocker lock(&mutex_);
    if(selectionPending_) return false;
    selection_=lines;selectionPending_=true;return true;
}
