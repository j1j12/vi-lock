#include "../face_overlay.h"
#include "../enrollment_session.h"
#include "../status_worker.h"
#include <QApplication>
#include "../diagnostic_views.h"
#include <QLabel>
#include <QTableWidget>
#include <QFontDatabase>
#include <QPixmap>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <cstdio>
#include <stdexcept>
#include <limits>
static void check(bool ok,const char *s) { if(!ok) throw std::runtime_error(s); }
static void put(const QString &path,const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); check(f.open(QIODevice::WriteOnly),"fixture write"); f.write(data);
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
#ifdef Q_OS_WIN
    const int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    if(font>=0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first()));
#endif
    try {
        EnrollmentSession session;
        const auto first=session.begin(); check(session.owns(first),"new session active");
        session.cancel(first); check(!session.owns(first),"cancel denies commit");
        const auto second=session.begin(); session.cancel(first);
        check(session.owns(second) && !session.owns(first),"old cancel cannot cancel replacement");
        const auto third=session.begin(); check(!session.owns(second) && session.owns(third),"replacement invalidates old result");
        QImage source(100,80,QImage::Format_RGB32); source.fill(Qt::black);
        auto out=faceOverlay(source,{QRectF(10,10,40,40)});
        check(source.pixelColor(10,10)==QColor(Qt::black),"inference input not painted");
        check(out.pixelColor(10,10).green()>100,"single face green box");
        out=faceOverlay(source,{QRectF(-10,-10,30,30),QRectF(60,40,30,30)});
        check(out.pixelColor(60,40).red()>100,"multiple faces amber");
        check(faceOverlay(source,{} )==source,"no faces has no stale boxes");
        check(faceOverlay(source,{QRectF(std::numeric_limits<double>::quiet_NaN(),0,20,20)})==source,"invalid boxes discarded");
        check(faceOverlay(QImage(),{}).isNull(),"missing frame clears display");
        QTemporaryDir temp; check(temp.isValid(),"temporary fixture");
        auto missing=StatusWorker::collect(temp.path());
        check(missing.size()==10 && missing[1]=="UNKNOWN" && missing[3]=="UNKNOWN","missing sysfs not fabricated");
        put(temp.path()+"/sys/class/remoteproc/remoteproc0/state","running\n");
        put(temp.path()+"/sys/class/remoteproc/remoteproc0/firmware","fixture.elf\n");
        put(temp.path()+"/sys/module/access_control/version","2.0\n");
        put(temp.path()+"/proc/uptime","123.45 100.00\n");
        const auto utc=QDateTime::currentDateTimeUtc();
        put(temp.path()+"/sys/class/rtc/rtc0/date",utc.date().toString(Qt::ISODate).toLatin1());
        put(temp.path()+"/sys/class/rtc/rtc0/time",utc.time().toString("HH:mm:ss").toLatin1());
        put(temp.path()+"/sys/class/rtc/rtc0/hctosys","1\n");
        put(temp.path()+"/home/root/access-control-data/events.jsonl",QByteArray(2048,'x'));
        auto values=StatusWorker::collect(temp.path());
        check(values[1]=="running" && values[2]=="fixture.elf" && values[3]=="2.0","actual fixture values");
        check(values[6].startsWith("快照一致") && values[7].startsWith("123.45"),"RTC snapshot and uptime");
        check(QFileInfo(temp.path()+"/home/root/access-control-data/events.jsonl").size()==2048,"status does not alter files");
        EnrollmentView enrollment("new",QString(48,'a')); enrollment.show(); app.processEvents();
        auto fits=[](QDialog &d) {
            const auto widgets=d.findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly);
            for(auto *a:widgets) if(a->isVisible()) {
                check(d.rect().contains(a->geometry()),"view control within panel");
                for(auto *b:widgets) if(a!=b && b->isVisible()) check(!a->geometry().intersects(b->geometry()),"view controls do not overlap");
            }
        };
        fits(enrollment); check(enrollment.grab().save("enrollment-view.png"),"save enrollment preview");
        SystemStatusView status; status.show(); app.processEvents();
        for(int i=0;i<values.size();++i) status.table->item(i,1)->setText(values[i]);
        fits(status); check(status.grab().save("status-view.png"),"save status preview");
        std::puts("PASS: overlays immutable/clipped/empty, enrollment cancellation generations, read-only status and UNKNOWN");
        return 0;
    } catch(const std::exception &e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
