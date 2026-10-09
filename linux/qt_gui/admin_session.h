#ifndef ADMIN_SESSION_H
#define ADMIN_SESSION_H
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QDialog>
#include <QPointer>

// Watches only this management dialog and its child dialogs. Camera repaint,
// PONG and mouse movement do not extend an authenticated session.
class AdminSession : public QObject {
public:
    explicit AdminSession(QDialog &dialog, int timeoutMs = 120000);
    ~AdminSession() override;
    bool active();
    bool expired() const { return expired_; }
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void expire();
    QPointer<QDialog> dialog_;
    QElapsedTimer idle_;
    QTimer timer_;
    int timeoutMs_;
    bool expired_ = false;
};
#endif
