#include "admin_session.h"
#include <QApplication>
#include <QEvent>

AdminSession::AdminSession(QDialog &dialog, int timeoutMs)
    : dialog_(&dialog), timeoutMs_(timeoutMs) {
    idle_.start();
    qApp->installEventFilter(this);
    connect(&timer_, &QTimer::timeout, this, [this]() { active(); });
    timer_.start(250);
}
AdminSession::~AdminSession() { qApp->removeEventFilter(this); }
bool AdminSession::active() {
    if (!expired_ && idle_.elapsed() >= timeoutMs_) expire();
    return !expired_ && !dialog_.isNull();
}
void AdminSession::expire() {
    expired_ = true;
    timer_.stop();
    if (!dialog_) return;
    // Close nested confirmations too: returning from a stale QMessageBox or
    // input dialog must not resume an authorized mutation.
    const auto children = dialog_->findChildren<QDialog *>();
    for (auto *child : children) child->reject();
    dialog_->reject();
}
bool AdminSession::eventFilter(QObject *object, QEvent *event) {
    QWidget *widget = qobject_cast<QWidget *>(object);
    if (!dialog_ || !widget || (widget != dialog_ && !dialog_->isAncestorOf(widget))) return false;
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
    case QEvent::TouchBegin:
    case QEvent::Wheel:
        if (!active()) return true; // Expired input never revives the session.
        idle_.restart();
        break;
    default: break;
    }
    return false;
}
