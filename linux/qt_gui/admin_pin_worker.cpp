#include "admin_pin_worker.h"
#include "admin_pin_store.h"
#include <QMutexLocker>
#include <exception>
#include <QElapsedTimer>
#include <QDebug>

quint64 AdminPinWorker::request(const QString &operation, const QString &pin, const QString &replacement) {
    QMutexLocker lock(&mutex_);
    if (busy_ || isInterruptionRequested()) return 0;
    busy_ = pending_ = true;
    operation_ = operation; pin_ = pin; replacement_ = replacement;
    return ++serial_;
}
void AdminPinWorker::cancel(quint64 requestId) {
    QMutexLocker lock(&mutex_);
    if (requestId && requestId == serial_) cancelled_ = requestId;
}
void AdminPinWorker::run() {
    AdminPinStore store(directory_);
    while (!isInterruptionRequested()) {
        QString op, pin, replacement;
        quint64 id = 0;
        {
            QMutexLocker lock(&mutex_);
            if (pending_) {
                pending_ = false;
                id = serial_; op = operation_; pin = pin_; replacement = replacement_;
                pin_.clear(); replacement_.clear();
            }
        }
        if (!id) { msleep(30); continue; }
        bool ok = false;
        QElapsedTimer workTime; workTime.start();
        QString message;
        auto cancelled = [this, id]() {
            QMutexLocker lock(&mutex_);
            return cancelled_ == id || isInterruptionRequested();
        };
        try {
            if (cancelled()) message = "PIN request cancelled";
            else if (op == "inspect") { message = store.inspect(); ok = true; }
            else if (op == "initialize") { store.initialize(pin, cancelled); ok = true; }
            else if (op == "verify") ok = store.verify(pin);
            else if (op == "change") ok = store.change(pin, replacement, cancelled);
            else message = "Unknown PIN operation";
            if (message.isEmpty()) message = ok ? "成功" : "PIN不正确。";
        } catch (const std::exception &e) { message = QString::fromUtf8(e.what()); }
        if (cancelled()) { ok = false; message = "PIN request cancelled; no session granted"; }
        int attempts = -1;
        qint64 lockedMs = 0;
        try { store.status(attempts, lockedMs); }
        catch (const std::exception &e) { attempts = -1; ok = false; message = QString::fromUtf8(e.what()); }
        // Best effort only: Qt copies and process memory are not a secure enclave.
        pin.fill(QChar(0)); replacement.fill(QChar(0));
        { QMutexLocker lock(&mutex_); busy_ = false; }
        if (op != "inspect") emit audit(op, ok, message);
        if (op != "inspect") qInfo() << "Admin PIN operation" << op << "elapsed_ms=" << workTime.elapsed();
        emit reply(id, ok, message, attempts, lockedMs);
    }
}
