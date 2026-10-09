#ifndef ADMIN_PIN_DIALOG_H
#define ADMIN_PIN_DIALOG_H
#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>
#include "admin_pin_worker.h"
class EventJournal;
class QLineEdit;
class QLabel;
class QPushButton;

class AdminPinDialog : public QDialog {
public:
    AdminPinDialog(AdminPinWorker &worker, EventJournal &journal, bool change, QWidget *parent);
    ~AdminPinDialog() override;
private:
    void submit();
    void setMode(bool setup);
    void updateLock();
    void applyStatus(int attempts, qint64 lockedMs);
    QTimer countdown_;
    QTimer feedback_;
    QElapsedTimer workClock_;
    QElapsedTimer lockClock_;
    qint64 lockedMs_ = 0;
    AdminPinWorker &worker_;
    EventJournal &journal_;
    bool change_, setup_ = false, inspecting_ = true;
    quint64 pending_ = 0;
    QLineEdit *current_, *next_, *confirm_, *input_;
    QLabel *currentLabel_, *nextLabel_, *confirmLabel_, *message_;
    QPushButton *submit_;
};
#endif
