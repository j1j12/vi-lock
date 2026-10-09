#include "../admin_pin_store.h"
#include "../admin_pin_dialog.h"
#include "../admin_session.h"
#include "../event_journal.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "../pin_kdf.h"
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QFontDatabase>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <stdexcept>

static void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F action) {
    bool rejected = false;
    try { action(); } catch (const std::exception &) { rejected = true; }
    check(rejected, "expected rejection");
}
static void marker(const QString &directory) {
    check(QDir().mkpath(directory), "create directory");
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QFile file(directory + "/admin-pin.setup");
    check(file.open(QIODevice::WriteOnly), "create setup permission");
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
}
static QByteArray bytes(const QString &path) {
    QFile file(path); check(file.open(QIODevice::ReadOnly), "read fixture"); return file.readAll();
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    // The Windows offscreen plugin does not discover native fonts by itself.
    const int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    if (font >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first()));
#endif
    try {
        // RFC7914 section11: first 32 bytes of each 64-byte test vector.
        check(pinPbkdf2Sha256("passwd", "salt", 1).toHex() ==
              "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc", "PBKDF2 single-round vector");
        check(pinPbkdf2Sha256("Password", "NaCl", 80000).toHex() ==
              "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56", "PBKDF2 iterative vector");
        check(pinPbkdf2Sha256("passwd", "salt", 0).isEmpty(), "invalid rounds rejected");
        check(AdminPinStore::validNewPin("83052947"), "valid PIN");
        for (const QString &weak : {QString("12345678"), QString("11111111"), QString("87654321"), QString("1234"), QString("83052947\n")})
            check(!AdminPinStore::validNewPin(weak), "weak/malformed PIN rejected");
        QTemporaryDir temp;
        check(temp.isValid(), "temporary directory");
        const QString directory = temp.path() + "/pin";
        AdminPinStore unprepared(directory);
        rejects([&]() { unprepared.inspect(); });
        marker(directory);
        qint64 time = 0;
        AdminPinStore store(directory, [&]() { return time; });
        check(store.inspect() == "setup_required", "explicit first setup");
        rejects([&]() { store.initialize("83052947", []() { return true; }); });
        check(store.inspect() == "setup_required", "cancel before setup commit retains permission");
        store.initialize("83052947");
        check(!QFile::exists(directory + "/admin-pin.setup"), "setup permission consumed");
        check(store.inspect() == "ready", "ready after setup");
        rejects([&]() { store.initialize("72950483"); });
        check(!bytes(directory + "/admin-pin.json").contains("83052947"), "PIN not stored as plaintext");
        check(!store.verify("73052947"), "wrong PIN denied");
        check(store.verify("83052947"), "correct PIN accepted");
        rejects([&]() { store.change("83052947", "72950483", []() { return true; }); });
        check(store.verify("83052947"), "cancelled change preserves old PIN");
        check(store.change("83052947", "72950483"), "PIN change");
        check(!store.verify("83052947") && store.verify("72950483"), "only new PIN works");
        int attempts = -1; qint64 locked = -1;
        for (int i = 0; i < 5; ++i) {
            check(!store.verify("bad"), "failure counter");
            store.status(attempts, locked);
            check(attempts == 4-i && locked == (i == 4 ? 60000 : 0), "authoritative remaining attempts/lock");
        }
        rejects([&]() { store.verify("72950483"); });
        AdminPinStore restarted(directory, [&]() { return time; });
        rejects([&]() { restarted.verify("72950483"); });
        time = 59999;
        restarted.status(attempts, locked);
        check(attempts == 0 && locked == 1, "remaining lock uses monotonic time");
        rejects([&]() { restarted.verify("72950483"); });
        time = 60000;
        restarted.status(attempts, locked);
        check(attempts == 5 && locked == 0, "expired status permits fresh attempts");
        check(restarted.verify("72950483"), "monotonic cooldown expires and correct PIN works");
        const auto obj = QJsonDocument::fromJson(bytes(directory + "/admin-pin.json")).object();
        check(obj["failures"].toInt(-1) == 0, "success resets failures");
        QFile broken(directory + "/admin-pin.json");
        check(broken.open(QIODevice::WriteOnly | QIODevice::Truncate), "corrupt fixture");
        broken.write("broken"); broken.close();
        rejects([&]() { restarted.inspect(); });
        check(QFile::remove(directory + "/admin-pin.json"), "delete fixture");
        rejects([&]() { restarted.inspect(); }); // No automatic first-setup fallback.

        QDialog root;
        QLineEdit input(&root);
        AdminSession session(root, 30);
        QThread::msleep(40);
        QKeyEvent key(QEvent::KeyPress, Qt::Key_1, Qt::NoModifier, "1");
        QCoreApplication::sendEvent(&input, &key);
        check(session.expired() && !session.active() && input.text().isEmpty(), "expired input cannot revive session");
        QDialog root2;
        QDialog nested(&root2);
        AdminSession nestedSession(root2, 30);
        QTimer::singleShot(1000, &nested, &QDialog::accept); // Failure watchdog.
        check(nested.exec() == QDialog::Rejected && nestedSession.expired(), "nested dialog rejected on idle expiry");

        // Compile and exercise real asynchronous dialog/keypad without camera.
        const QString uiDir = temp.path() + "/ui";
        marker(uiDir);
        AdminPinWorker worker(uiDir);
        EventJournal journal(temp.path() + "/events");
        worker.start();
        {
            AdminPinDialog dialog(worker, journal, false, nullptr);
            dialog.show();
            QElapsedTimer wait;
            wait.start();
            while (wait.elapsed() < 300) { app.processEvents(); QThread::msleep(10); }
            int digits = 0;
            for (auto *button : dialog.findChildren<QPushButton *>())
                if (button->text().size() == 1 && button->text()[0].isDigit()) ++digits;
            check(digits == 10, "touch keypad contains all digits");
            worker.reply(999999, true, "成功");
            check(dialog.result() != QDialog::Accepted, "stale reply cannot authenticate dialog");
            check(dialog.grab().save("admin-pin-setup.png"), "save UI preview");
            dialog.reject();
        }
        worker.requestInterruption(); worker.wait();
        // Same worker across dialog close/reopen: backend remains authoritative.
        const QString lockedDir = temp.path() + "/locked-ui";
        marker(lockedDir);
        AdminPinStore fixture(lockedDir);
        fixture.initialize("83052947");
        for (int i=0; i<5; ++i) fixture.verify("bad");
        AdminPinWorker lockedWorker(lockedDir);
        lockedWorker.start();
        auto pump = [&](int ms) {
            QElapsedTimer timer; timer.start();
            while (timer.elapsed() < ms) { app.processEvents(); QThread::msleep(10); }
        };
        bool disabled1=false, disabled2=false, changed=false;
        {
            AdminPinDialog dialog(lockedWorker, journal, false, nullptr); dialog.show(); pump(400);
            auto labels = dialog.findChildren<QLabel *>();
            const QString before = labels.first()->text();
            for (auto *button : dialog.findChildren<QPushButton *>())
                if (button->text()=="确认") disabled1=!button->isEnabled();
            pump(1200);
            changed = before != labels.first()->text() && labels.first()->text().contains("PIN已锁定");
            dialog.reject();
        }
        {
            AdminPinDialog dialog(lockedWorker, journal, false, nullptr); dialog.show(); pump(400);
            for (auto *button : dialog.findChildren<QPushButton *>())
                if (button->text()=="确认") disabled2=!button->isEnabled();
            dialog.reject();
        }
        lockedWorker.requestInterruption(); lockedWorker.wait();
        check(disabled1 && disabled2 && changed, "live countdown and reopen remain locked");
        std::puts("PASS: KDF vector, PIN setup/verify/change/cancel, private hash, persisted failures/cooldown, fail-closed config, idle/nested dialog, touch keypad");
        return 0;
    } catch (const std::exception &e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
