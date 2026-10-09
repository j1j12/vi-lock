#include "../event_query.h"
#include "../event_query_view.h"
#include "../event_journal.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QFile>
#include <QComboBox>
#include <QFontDatabase>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <cstdio>
#include <stdexcept>
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
static QString row(QString type,QString person="test_user",QString time="2026-09-28T00:00:00+08:00"){
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"type",type},{"person",person},{"time",time},{"detail","fixture"}}).toJson(QJsonDocument::Compact));
}
int main(int argc,char **argv){
    QApplication app(argc,argv);
#ifdef Q_OS_WIN
    int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    if(font>=0)app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first()));
#endif
    try {
        QStringList lines{row("recognition_match"),row("access_denied"),row("cycle_requested"),row("cycle_finished",""),
            row("recognition_result"),row("recognition_match","test_user2","2026-09-27T15:59:59Z"),"bad"};
        EventFilter filter;auto all=queryEvents(lines,filter);
        check(all.lines.size()==6 && all.invalid==1,"invalid JSON separated");
        check(all.counts.value("recognition_match")==2,"explicit event counts only");
        filter.person="test_user"; check(queryEvents(lines,filter).lines.size()==4,"exact person; no anonymous completion attribution");
        filter.type="cycle_finished";check(queryEvents(lines,filter).lines.isEmpty(),"no inferred person");
        filter.person.clear();filter.type.clear();filter.dates=true;filter.first=filter.last=QDate(2026,9,28);
        check(queryEvents(lines,filter).lines.size()==5,"UTC8 inclusive date boundary");
        filter.first=QDate(2026,9,29);check(!queryEvents(lines,filter).valid,"reversed dates rejected");
        QTemporaryDir temp;check(temp.isValid(),"temp");
        EventJournal journal(temp.path());
        QStringList selected{lines.first()};check(journal.exportSelection(selected),"queue selection");
        selected.clear();check(!journal.exportSelection({}),"bounded pending queue");
        journal.record("later","not in exported snapshot");
        journal.start();journal.requestInterruption();check(journal.wait(5000),"drain on stop");
        QFile file(temp.filePath("events-filtered.jsonl"));check(file.open(QIODevice::ReadOnly),"export created");
        check(file.readAll()==lines.first().toUtf8()+'\n',"immutable exported snapshot");file.close();
        EventJournal empty(temp.path());check(empty.exportSelection({}),"empty export allowed");empty.start();empty.requestInterruption();check(empty.wait(5000),"empty drain");
        check(file.open(QIODevice::ReadOnly)&&file.readAll().isEmpty(),"empty file not blank JSON row");
        EventQueryView view;view.setSnapshot(lines);view.resize(1024,600);view.show();app.processEvents();
        check(view.selectedLines().size()==6,"view snapshot");
        auto *people=view.findChild<QComboBox*>("personFilter");people->setCurrentIndex(people->findData("test_user"));
        check(view.selectedLines().size()==4,"touch combo filter");
        view.setSnapshot({});check(view.selectedLines().isEmpty() && people->currentData().toString()=="test_user","refresh preserves missing selection");
        view.setSnapshot(lines);app.processEvents();
        for(auto *widget:view.findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))
            if(widget->isVisible())check(view.rect().contains(widget->geometry()),"view bounds");
        check(view.grab().save("events-view.png"),"layout screenshot");
        QPushButton *dates=nullptr,*first=nullptr;
        for(auto *button:view.findChildren<QPushButton*>()) {
            if(button->text().contains("限定日期"))dates=button;
            if(button->text().startsWith("起始"))first=button;
        }
        check(dates && first,"date controls");dates->click();check(first->isEnabled(),"date toggle enables picker");
        QTimer::singleShot(100,[&](){auto *dialog=qobject_cast<QDialog*>(app.activeModalWidget());
            check(dialog && dialog!=&view,"calendar modal");check(dialog->grab().save("events-calendar.png"),"calendar screenshot");dialog->reject();});
        first->click();dates->click();check(!first->isEnabled(),"date toggle off");
        std::puts("PASS: query/exact filters/UTC8/count semantics/export snapshot/shutdown/empty export/UI bounds");
    }catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
