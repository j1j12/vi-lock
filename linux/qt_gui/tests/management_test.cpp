#include "../person_store.h"
#include "../event_journal.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <limits>
#include <stdexcept>

static void check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> static void rejects(F action) {
    bool rejected = false;
    try { action(); } catch (const std::exception &) { rejected = true; }
    check(rejected, "expected rejection");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir temporary;
        check(temporary.isValid(), "temporary directory");
        QString faces = temporary.path() + "/faces";
        PersonStore store(faces);
        check(store.load().empty(), "empty database supported");
        std::vector<float> feature(512, 1.0f);
        store.save("test_user", feature, false);
        check(store.load().size() == 1 && store.load()[0].enabled, "legacy bin loaded");
        rejects([&] { store.save("cancelled",feature,false,[](){return true;}); });
        check(store.load().size()==1,"cancelled enrollment leaves no template");
        rejects([&] { store.save("test_user",feature,true,[](){return true;}); });
        check(store.load().size()==1 && store.load()[0].enabled,"cancelled update preserves old template");
        rejects([&] { store.save("test_user", feature, false); });
        rejects([&] { store.save("../escape", feature, false); });
        rejects([&] { store.save("bad/name", feature, false); });
        rejects([&] { store.save("bad\n", feature, false); });
        rejects([&] { store.save("missing", feature, true); });
        auto bad = feature;
        bad[0] = std::numeric_limits<float>::quiet_NaN();
        rejects([&] { store.save("test_user", bad, true); });
        check(store.load().size() == 1, "bad update preserves template");
        store.setEnabled("test_user", false);
        check(!store.load()[0].enabled, "disable persistent");
        check(QDir(faces).entryList({"*.bin"}, QDir::Files).isEmpty(), "legacy matcher excludes disabled");
        store.save("test_user", feature, true);
        check(!store.load()[0].enabled, "update preserves disabled state");
        store.setEnabled("test_user", true);
        check(store.load()[0].enabled, "enable persistent");
        store.remove("test_user");
        check(store.load().empty(), "delete last record supported");
        QFile corrupt(faces + "/broken.bin");
        check(corrupt.open(QIODevice::WriteOnly), "create corrupt fixture");
        corrupt.write("bad"); corrupt.close();
        rejects([&] { store.load(); });
        store.remove("broken");
        store.save("duplicate", feature, false);
        check(QFile::copy(faces + "/duplicate.bin", faces + "/duplicate.bin.disabled"), "duplicate fixture");
        rejects([&] { store.load(); });
        check(QFile::remove(faces + "/duplicate.bin.disabled"), "remove fixture");
        store.remove("duplicate");
        QString events = temporary.path() + "/events";
        EventJournal journal(events);
        for (int i = 0; i < 520; ++i) journal.record("test", QString::number(i), "test_user");
        journal.start();
        journal.requestInterruption();
        check(journal.wait(10000), "journal drain");
        check(journal.snapshot().size() == 500, "bounded history");
        QFile saved(events + "/events.jsonl");
        check(saved.open(QIODevice::ReadOnly), "journal persisted");
        int rows = 0;
        while (!saved.atEnd()) {
            auto obj = QJsonDocument::fromJson(saved.readLine()).object();
            check(obj["type"].toString() == "test" && obj.contains("session") && obj.contains("elapsed_ms"), "event schema");
            ++rows;
        }
        check(rows == 512, "bounded pending queue");
        saved.close();
        EventJournal restored(events);
        check(restored.exportHistory(), "export accepted");
        restored.start(); restored.requestInterruption();
        check(restored.wait(10000), "restore/export finished");
        check(restored.snapshot().size() == 500, "history restored");
        check(QFile::exists(events + "/events-export.jsonl"), "export created");
        QFile large(events + "/events.jsonl");
        check(large.open(QIODevice::WriteOnly), "rotation fixture");
        large.write(QByteArray(2 * 1024 * 1024, 'x')); large.close();
        EventJournal rotation(events);
        rotation.record("rotation", "bounded archive");
        rotation.start(); rotation.requestInterruption();
        check(rotation.wait(10000), "rotation finished");
        check(QFileInfo(events + "/events.jsonl.1").size() == 2 * 1024 * 1024 + 1, "archive rotated with separated partial tail");
        check(QFileInfo(events + "/events.jsonl").size() < 8192, "new current file");
        std::puts("PASS: person CRUD/validation/disabled compatibility; event queue/history/persistence/export/rotation");
        return 0;
    } catch (const std::exception &e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
