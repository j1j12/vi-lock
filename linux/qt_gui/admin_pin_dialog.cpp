#include "admin_pin_dialog.h"
#include "admin_pin_store.h"
#include "event_journal.h"
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QRegularExpressionValidator>
#include <QApplication>

AdminPinDialog::AdminPinDialog(AdminPinWorker &worker, EventJournal &journal, bool change, QWidget *parent)
    : QDialog(parent), worker_(worker), journal_(journal), change_(change) {
    journal_.record("admin_prompt", change ? "修改PIN验证" : "人员管理入口验证");
    setWindowTitle(change ? "修改管理员PIN" : "管理员验证");
    setMinimumSize(560, 520);
    setStyleSheet("QDialog{background:#172330;} QLabel{color:white;font-size:17px;} "
                  "QLineEdit{background:white;color:black;min-height:34px;font-size:22px;} "
                  "QPushButton{min-height:42px;font-size:21px;}");
    auto *layout = new QVBoxLayout(this);
    message_ = new QLabel("读取PIN配置…");
    message_->setWordWrap(true);
    layout->addWidget(message_);
    auto *fields = new QGridLayout;
    currentLabel_ = new QLabel("管理员PIN"); nextLabel_ = new QLabel("新PIN（8–12位）"); confirmLabel_ = new QLabel("再输入新PIN");
    current_ = new QLineEdit; next_ = new QLineEdit; confirm_ = new QLineEdit;
    const QList<QLineEdit *> inputs{current_, next_, confirm_};
    const QList<QLabel *> labels{currentLabel_, nextLabel_, confirmLabel_};
    for (int i = 0; i < inputs.size(); ++i) {
        inputs[i]->setEchoMode(QLineEdit::Password);
        inputs[i]->setMaxLength(12);
        inputs[i]->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]{0,12}"), inputs[i]));
        inputs[i]->setContextMenuPolicy(Qt::NoContextMenu);
        inputs[i]->setInputMethodHints(Qt::ImhDigitsOnly | Qt::ImhHiddenText | Qt::ImhNoPredictiveText);
        fields->addWidget(labels[i], i, 0); fields->addWidget(inputs[i], i, 1);
    }
    layout->addLayout(fields);
    input_ = current_;
    connect(qApp, &QApplication::focusChanged, this, [this, inputs](QWidget *, QWidget *now) {
        for (auto *field : inputs) if (now == field) input_ = field;
    });
    auto *keys = new QGridLayout;
    const QStringList captions{"1","2","3","4","5","6","7","8","9","清空","0","退格"};
    for (int i = 0; i < captions.size(); ++i) {
        auto *button = new QPushButton(captions[i]);
        button->setFocusPolicy(Qt::NoFocus);
        keys->addWidget(button, i / 3, i % 3);
        const QString key = captions[i];
        connect(button, &QPushButton::clicked, this, [this, key]() {
            if (!input_->isEnabled() || !input_->isVisible()) return;
            if (key == "清空") input_->clear();
            else if (key == "退格") input_->backspace();
            else input_->insert(key);
        });
    }
    layout->addLayout(keys);
    auto *bottom = new QHBoxLayout;
    submit_ = new QPushButton("确认");
    submit_->setDefault(true);
    auto *cancel = new QPushButton("取消");
    bottom->addWidget(submit_); bottom->addWidget(cancel); layout->addLayout(bottom);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(submit_, &QPushButton::clicked, this, [this]() { submit(); });
    countdown_.setInterval(200);
    feedback_.setInterval(200);
    connect(&feedback_, &QTimer::timeout, this, [this]() {
        message_->setText(QString("正在安全校验，已等待 %1 秒…取消不会授予管理权限。")
                          .arg(workClock_.elapsed() / 1000));
    });
    connect(&countdown_, &QTimer::timeout, this, [this]() { updateLock(); });
    connect(&worker_, &AdminPinWorker::reply, this, [this](quint64 id, bool ok, const QString &message, int attempts, qint64 lockedMs) {
        if (!pending_ || id != pending_) return; // Never accept a stale dialog's login result.
        feedback_.stop();
        pending_ = 0;
        if (inspecting_) {
            inspecting_ = false;
            if (!ok) { message_->setText("管理入口已锁定：" + message); submit_->setEnabled(false); return; }
            setMode(message == "setup_required");
            if (!setup_) applyStatus(attempts, lockedMs);
            return;
        }
        for (auto *field : {current_, next_, confirm_}) { field->clear(); field->setEnabled(true); }
        submit_->setEnabled(true);
        if (ok) accept();
        else {
            applyStatus(attempts, lockedMs);
            if (lockedMs <= 0) message_->setText(message + "\n" + (attempts >= 0 ? message_->text() : QString()));
        }
    });
    next_->hide(); confirm_->hide(); nextLabel_->hide(); confirmLabel_->hide();
    submit_->setEnabled(false);
    pending_ = worker_.request("inspect");
    if (!pending_) message_->setText("PIN校验仍在执行，请取消后稍候重试。");
}
AdminPinDialog::~AdminPinDialog() {
    worker_.cancel(pending_);
    current_->clear(); next_->clear(); confirm_->clear();
}
void AdminPinDialog::setMode(bool setup) {
    setup_ = setup;
    if (change_ && setup) { message_->setText("PIN状态异常，需root处理"); return; }
    current_->setVisible(!setup); currentLabel_->setVisible(!setup);
    next_->setVisible(change_ || setup); confirm_->setVisible(change_ || setup);
    nextLabel_->setVisible(change_ || setup); confirmLabel_->setVisible(change_ || setup);
    input_ = setup ? next_ : current_;
    input_->setFocus();
    submit_->setEnabled(true);
    message_->setText(setup ? "首次设置：8–12位数字，不接受重复/连续数字。请记住PIN。" :
                      change_ ? "请输入当前PIN和两次新PIN，修改成功后退出管理。" : "请输入管理员PIN。5次错误锁定60秒。");
}
void AdminPinDialog::submit() {
    if (pending_ || inspecting_ || lockedMs_ > 0 || !submit_->isEnabled()) return;
    if (change_ || setup_) {
        if (next_->text() != confirm_->text() || !AdminPinStore::validNewPin(next_->text())) {
            message_->setText("新PIN须为8–12位非简单数字，且两次输入一致。"); return;
        }
    }
    const QString op = change_ ? "change" : setup_ ? "initialize" : "verify";
    pending_ = worker_.request(op, setup_ ? next_->text() : current_->text(), change_ ? next_->text() : QString());
    if (!pending_) { message_->setText("验证线程忙，请稍候重试。"); return; }
    for (auto *field : {current_, next_, confirm_}) { field->clear(); field->setEnabled(false); }
    submit_->setEnabled(false);
    message_->setText("正在安全校验，请稍候…取消不会授予管理权限。");
    workClock_.start(); feedback_.start();
}
void AdminPinDialog::applyStatus(int attempts, qint64 lockedMs) {
    if (attempts < 0) {
        submit_->setEnabled(false);
        for (auto *field : {current_, next_, confirm_}) field->setEnabled(false);
        return;
    }
    lockedMs_ = lockedMs;
    if (lockedMs_ > 0) {
        lockClock_.start();
        for (auto *field : {current_, next_, confirm_}) { field->clear(); field->setEnabled(false); }
        submit_->setEnabled(false);
        countdown_.start();
        updateLock();
    } else {
        countdown_.stop();
        message_->setText(QString("剩余 %1 次尝试；用完后锁定60秒。退出窗口不会清除次数或锁定。%2")
                          .arg(attempts).arg(change_ ? "请输入当前PIN和两次新PIN。" : "请输入管理员PIN。"));
    }
}
void AdminPinDialog::updateLock() {
    const qint64 remaining = qMax(qint64(0), lockedMs_ - lockClock_.elapsed());
    if (remaining > 0) {
        message_->setText(QString("PIN已锁定，剩余 %1 秒。退出后重新进入仍然锁定。").arg((remaining + 999) / 1000));
        return;
    }
    // Only the worker may authorize expiry. Never unlock using UI time alone.
    if (pending_) return;
    inspecting_ = true;
    pending_ = worker_.request("inspect");
    if (!pending_) return; // Retry on next tick if an old request is finishing.
    countdown_.stop();
    lockedMs_ = 0;
    for (auto *field : {current_, next_, confirm_}) field->setEnabled(true);
    message_->setText("正在确认锁定状态…");
}
