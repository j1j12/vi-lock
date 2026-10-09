#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <opencv2/core.hpp>
#include <csignal>
#include "mainwindow.h"
#include "person_store.h"
#include "access_rules.h"
#include <QCoreApplication>
#include <QDir>
#include <cstring>
#include <cstdio>
#include <exception>
#include <stdexcept>

namespace {
volatile std::sig_atomic_t stopRequested = 0;
void requestStop(int)
{
    // Signal context: no Qt calls, logging, allocation or thread joins.
    stopRequested = 1;
}
}

int main(int argc, char *argv[])
{
    const bool initializeRules = argc == 2 && std::strcmp(argv[1], "--init-access-rules") == 0;
    const bool checkRule = argc == 3 && std::strcmp(argv[1], "--check-access-rule") == 0;
    if (initializeRules || checkRule) {
        QCoreApplication app(argc, argv); // No camera, display, RPMsg or PIN prompt.
        try {
            if (checkRule) {
                const QString id = QString::fromLocal8Bit(argv[2]);
                bool enabled = false, found = false;
                for (const auto &person : PersonStore("/home/root/faces").load())
                    if (person.id == id) { found = true; enabled = person.enabled; }
                if (!found) throw std::runtime_error("person missing");
                const auto rule = AccessRuleStore("/home/root/faces").get(id);
                AccessClock clock;
                const auto now = QDateTime::currentDateTimeUtc();
                const bool clockOk = clock.consistent(now);
                auto verdict = evaluateAccess(rule, now, clockOk);
                if (!enabled) verdict = {false, "person disabled"};
                std::printf("%s: rule-only check, no motion; clock_consistent=%s; UTC+08:00=%s\n%s\n%s\n",
                    verdict.allowed ? "ALLOW" : "DENY", clockOk ? "yes" : "no",
                    now.toOffsetFromUtc(28800).toString(Qt::ISODate).toUtf8().constData(),
                    rule.summary().toUtf8().constData(), verdict.reason.toUtf8().constData());
                return verdict.allowed ? 0 : 2;
            }
            if (!QDir().mkpath("/home/root/faces")) throw std::runtime_error("cannot create faces directory");
            QStringList ids;
            for (const auto &person : PersonStore("/home/root/faces").load()) ids.append(person.id);
            AccessRuleStore("/home/root/faces").initialize(ids);
            std::puts("PASS: access rules initialized or existing rules validated; no existing rule replaced");
            return 0;
        } catch (const std::exception &e) { std::fprintf(stderr, "STOP: %s\n", e.what()); return 1; }
    }
    QApplication app(argc, argv);
    QApplication::setApplicationName("Access Control");
    // Configure once before workers start. Keep Qt workers independent, but
    // avoid nested OpenCV parallel regions competing for the two A7 cores.
    cv::setNumThreads(0);
    qInfo() << "Preview optimization v1: OpenCV sequential; preview interval 66ms";
    MainWindow window;
    window.showFullScreen();
    // Let the GUI event loop initiate shutdown so MainWindow's destructor
    // stops and joins workers before remoteproc is stopped by systemd.
    std::signal(SIGTERM, requestStop);
    std::signal(SIGINT, requestStop);
    QTimer stopTimer;
    QObject::connect(&stopTimer, &QTimer::timeout, &app, [&app]() {
        if (stopRequested)
            app.quit();
    });
    stopTimer.start(100);
    return app.exec();
}
