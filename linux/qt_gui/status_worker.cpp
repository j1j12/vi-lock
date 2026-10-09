#include "status_worker.h"
#include "access_rules.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QStorageInfo>
#include <QSysInfo>
namespace {
QString read(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return "UNKNOWN";
    const auto bytes=file.read(256).trimmed();
    return bytes.isEmpty() ? "UNKNOWN" : QString::fromUtf8(bytes);
}
}
QStringList StatusWorker::collect(const QString &root) {
    const auto now=QDateTime::currentDateTimeUtc();
    const QString rtc=root+"/sys/class/rtc/rtc0/";
    const QString date=read(rtc+"date"), time=read(rtc+"time"), boot=read(rtc+"hctosys");
    const bool clock= date==read(rtc+"date") && AccessClock::rtcAgrees(now,date,time,boot);
    QStorageInfo storage(root+"/home/root");
    QString disk="UNKNOWN";
    if(storage.isValid() && storage.isReady()) disk=QString("可用 %1 MiB / 总计 %2 MiB%3")
        .arg(storage.bytesAvailable()/1048576).arg(storage.bytesTotal()/1048576).arg(storage.isReadOnly() ? "（只读）" : "");
    const QFileInfo current(root+"/home/root/access-control-data/events.jsonl");
    const QFileInfo previous(root+"/home/root/access-control-data/events.jsonl.1");
    const QString logs=current.isFile() ? QString("当前 %1 KiB / 归档 %2 KiB").arg(current.size()/1024)
        .arg(previous.isFile() ? previous.size()/1024 : 0) : "UNKNOWN（未找到当前事件文件）";
    return {"GUI v18 · Qt " + QString(qVersion()) + " · " + QSysInfo::kernelVersion(),
        read(root+"/sys/class/remoteproc/remoteproc0/state"),
        read(root+"/sys/class/remoteproc/remoteproc0/firmware"),
        read(root+"/sys/module/access_control/version"),
        now.toOffsetFromUtc(28800).toString("yyyy-MM-dd HH:mm:ss 'UTC+08:00'"),
        date+" "+time+" UTC；hctosys="+boot,
        clock ? "快照一致；不代表可信授时或授权已通过" : "未通过/UNKNOWN；受限人员可能被拒绝",
        read(root+"/proc/uptime").section(' ',0,0)+" 秒",
        disk, logs};
}
void StatusWorker::run() {
    while(!isInterruptionRequested()) {
        if(!busy_.load()) { msleep(50); continue; }
        const auto values=collect();
        busy_.store(false); emit sample(values);
    }
}
