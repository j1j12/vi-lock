#include "../access_rules.h"
#include "../access_rule_dialog.h"
#include "../person_store.h"
#include "../oneshot_gate.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QCheckBox>
#include <QTimeEdit>
#include <QDateEdit>
#include <QPushButton>
#include <QFontDatabase>
#include <QFont>
#include <QPixmap>
#include <QTimer>
#include <QLineEdit>
#include <cstdio>
#include <stdexcept>

static void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class F> static void rejects(F f) {
    bool caught = false; try { f(); } catch (const std::exception &) { caught=true; }
    check(caught,"expected fail-closed rejection");
}
static QDateTime at(const char *dateTime) { return QDateTime::fromString(QString::fromLatin1(dateTime), Qt::ISODate); }
int main(int argc, char **argv) {
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    const int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    if(font>=0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first()));
#endif
    try {
        AccessRule rule;
        check(evaluateAccess(rule,QDateTime(),false).allowed,"unrestricted legacy compatible even without clock");
        rule.dates=true; rule.first=QDate(2026,9,22); rule.last=QDate(2026,9,23);
        check(!evaluateAccess(rule,at("2026-09-21T23:59:59+08:00"),true).allowed,"before start denied");
        check(evaluateAccess(rule,at("2026-09-22T00:00:00+08:00"),true).allowed,"first midnight included");
        check(evaluateAccess(rule,at("2026-09-23T23:59:59+08:00"),true).allowed,"last day inclusive");
        check(!evaluateAccess(rule,at("2026-09-24T00:00:00+08:00"),true).allowed,"expired midnight denied");
        check(!evaluateAccess(rule,at("2026-09-22T12:00:00+08:00"),false).allowed,"clock unknown denied");
        rule.hours=true; rule.startMinute=8*60; rule.endMinute=18*60;
        check(!evaluateAccess(rule,at("2026-09-22T07:59:59+08:00"),true).allowed,"before hour denied");
        check(evaluateAccess(rule,at("2026-09-22T08:00:00+08:00"),true).allowed,"start included");
        check(evaluateAccess(rule,at("2026-09-22T17:59:59+08:00"),true).allowed,"last minute included");
        check(!evaluateAccess(rule,at("2026-09-22T18:00:00+08:00"),true).allowed,"end excluded");
        check(evaluateAccess(rule,at("2026-09-22T00:00:00Z"),true).allowed,"fixed UTC8 independent of local timezone");
        rule.startMinute=22*60; rule.endMinute=6*60;
        check(evaluateAccess(rule,at("2026-09-22T22:00:00+08:00"),true).allowed,"wrap starts included");
        check(evaluateAccess(rule,at("2026-09-23T05:59:59+08:00"),true).allowed,"wrap morning included");
        check(!evaluateAccess(rule,at("2026-09-23T06:00:00+08:00"),true).allowed,"wrap end excluded");
        check(!evaluateAccess(rule,at("2026-09-24T01:00:00+08:00"),true).allowed,"wrap cannot extend expiry");
        auto invalid=rule; invalid.endMinute=invalid.startMinute; rejects([&]{invalid.validate();});
        invalid=rule; invalid.last=QDate(2026,1,1); rejects([&]{invalid.validate();});
        auto json=rule.json(); json["start"]=1.5; rejects([&]{AccessRule::parse(json);});
        json=rule.json(); json["hours"]="false"; rejects([&]{AccessRule::parse(json);});
        json=rule.json(); json["unknown"]=1; rejects([&]{AccessRule::parse(json);});
        check(AccessRule::parse(rule.json()).json()==rule.json(),"rule round trip");
        check(AccessClock::rtcAgrees(at("2026-09-22T12:00:00+08:00"),"2026-09-22","04:00:00","1"),"RTC UTC agrees");
        check(!AccessClock::rtcAgrees(at("2026-09-22T12:00:00+08:00"),"2026-09-22","12:00:00","1"),"local RTC mismatched denied");
        check(!AccessClock::rtcAgrees(at("2026-09-22T12:00:00+08:00"),"2026-09-22","04:00:00","0"),"failed boot RTC denied");
        check(!AccessClock::rtcAgrees(at("2026-09-22T12:00:00+08:00"),"","","1"),"unreadable RTC denied");
        check(!AccessClock::rtcAgrees(at("2026-09-22T12:00:06+08:00"),"2026-09-22","04:00:00","1"),"drift denied");
        QTemporaryDir temp; check(temp.isValid(),"temp directory");
        const auto directory=temp.path()+"/faces";
        PersonStore people(directory); std::vector<float> f(512,1.0f); people.save("alice",f,false);
        AccessRuleStore rules(directory);
        rejects([&]{rules.get("alice");});
        rules.initialize({"alice"});
        check(!rules.get("alice").dates && !rules.get("alice").hours,"explicit legacy migration unrestricted");
        rules.set("alice",rule); rules.initialize({"alice","bob"});
        check(rules.get("alice").json()==rule.json(),"rerun initialization preserves restrictions");
        rejects([&]{rules.get("bob");});
        rejects([&]{rules.set("../escape",rule);});
        people.setEnabled("alice",false); people.save("alice",f,true);
        check(!people.load().front().enabled && rules.get("alice").hours,"feature update retains disabled/rules");
        AccessRuleStore reopened(directory); check(reopened.get("alice").json()==rule.json(),"restart rules persistence");
        // Raw face match alone cannot permit the one-shot gate.
        OneShotGate gate; check(gate.arm(0,true),"arm"); auto ticket=gate.claim(1);
        const auto denied=evaluateAccess(rule,at("2026-09-23T12:00:00+08:00"),true);
        check(!gate.finish(ticket, true && denied.allowed,2) && !gate.active(),"denial consumes ticket, no fallback or retry");
        check(gate.arm(10,true),"rearm"); ticket=gate.claim(11);
        check(gate.finish(ticket,evaluateAccess(rule,at("2026-09-23T23:00:00+08:00"),true).allowed,12),"explicit gate plus valid rule permits");
        QFile broken(directory+"/access-rules.json"); check(broken.open(QIODevice::WriteOnly),"corruption fixture");
        broken.write("bad"); broken.close(); rejects([&]{rules.get("alice");}); rejects([&]{rules.initialize({"alice"});});
        check(QFile::remove(broken.fileName()),"remove fixture"); rejects([&]{rules.get("alice");});
        AccessRuleDialog editor("alice",rule); editor.show(); app.processEvents();
        check(editor.rule().json()==rule.json(),"editor preserves rule");
        auto *dateToggle=editor.findChild<QCheckBox*>("restrictDates");
        auto *hourToggle=editor.findChild<QCheckBox*>("restrictHours");
        dateToggle->click(); hourToggle->click(); app.processEvents();
        check(!editor.rule().dates && !editor.rule().hours,"first click clears both toggles");
        check(!editor.findChild<QPushButton*>("editFirstDate")->isEnabled() &&
              !editor.findChild<QPushButton*>("editStartTime")->isEnabled(),"cleared toggles disable entry");
        check(editor.grab().save("access-rule-unchecked.png"),"unchecked screenshot");
        dateToggle->click(); hourToggle->click(); app.processEvents();
        check(editor.rule().dates && editor.rule().hours,"second click checks both toggles");
        const auto widgets=editor.findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly);
        for(auto *a:widgets) if(a->isVisible()) {
            check(editor.rect().contains(a->geometry()),"control fits panel");
            for(auto *b:widgets) if(a!=b && b->isVisible())
                check(!a->geometry().intersects(b->geometry()),"sibling controls do not overlap");
        }
        auto *start=editor.findChild<QTimeEdit*>("startTime"), *end=editor.findChild<QTimeEdit*>("endTime");
        auto *save=editor.findChild<QPushButton*>("saveRule");
        bool invalidStayed=false, padSaved=false;
        QTimer::singleShot(30,&editor,[&]() {
            auto *pad=editor.findChild<QDialog*>("ruleTouchPad");
            if(!pad) return;
            auto type=[pad](const QString &digits) {
                pad->findChild<QPushButton*>("digit_清空")->click();
                for(auto c:digits) pad->findChild<QPushButton*>("digit_"+QString(c))->click();
            };
            type("2460"); pad->findChild<QPushButton*>("applyTouchValue")->click();
            invalidStayed=pad->isVisible() && pad->result()!=QDialog::Accepted;
            type("0830");
            padSaved=pad->grab().save("access-rule-keypad.png");
            pad->findChild<QPushButton*>("applyTouchValue")->click();
        });
        QTimer::singleShot(2000,&editor,[&]() { auto *pad=editor.findChild<QDialog*>("ruleTouchPad"); if(pad) pad->reject(); });
        editor.findChild<QPushButton*>("editStartTime")->click();
        check(invalidStayed && padSaved && start->time()==QTime(8,30),"touch-only entry and invalid time rejection");
        QTimer::singleShot(30,&editor,[&]() {
            auto *pad=editor.findChild<QDialog*>("ruleTouchPad");
            pad->findChild<QPushButton*>("digit_清空")->click();
            pad->findChild<QPushButton*>("digit_9")->click();
            pad->findChild<QPushButton*>("cancelTouchValue")->click();
        });
        editor.findChild<QPushButton*>("editStartTime")->click();
        check(start->time()==QTime(8,30),"cancel preserves previous value");
        bool invalidDateStayed=false;
        QTimer::singleShot(30,&editor,[&]() {
            auto *pad=editor.findChild<QDialog*>("ruleTouchPad");
            auto type=[pad](const QString &digits) {
                pad->findChild<QPushButton*>("digit_清空")->click();
                for(auto c:digits) pad->findChild<QPushButton*>("digit_"+QString(c))->click();
            };
            type("20260230"); pad->findChild<QPushButton*>("applyTouchValue")->click();
            invalidDateStayed=pad->isVisible();
            type("20260922"); pad->findChild<QPushButton*>("applyTouchValue")->click();
        });
        editor.findChild<QPushButton*>("editFirstDate")->click();
        check(invalidDateStayed && editor.rule().first==QDate(2026,9,22),"touch date validation");
        editor.findChild<QCheckBox*>("restrictHours")->setChecked(false);
        check(!editor.findChild<QPushButton*>("editStartTime")->isEnabled(),"unrestricted time input disabled");
        editor.findChild<QCheckBox*>("restrictHours")->setChecked(true);
        start->setTime(QTime(22,0));
        end->setTime(start->time()); save->click(); check(editor.result()!=QDialog::Accepted,"invalid editor refuses save");
        end->setTime(QTime(6,0));
        check(editor.grab().save("access-rule-editor.png"),"editor screenshot");
        save->click(); check(editor.result()==QDialog::Accepted,"valid editor save");
        std::puts("PASS: rule schema/storage/migration, date/time/wrap/UTC8, RTC consistency, one-shot denial, editor");
        return 0;
    } catch(const std::exception &e) {std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1;}
}
