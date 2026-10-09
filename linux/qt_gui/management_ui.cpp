#include "mainwindow.h"
#include "admin_pin_dialog.h"
#include "admin_session.h"
#include "access_rule_dialog.h"
#include "event_query_view.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <exception>

namespace {
void styleDialog(QDialog &dialog) {
    dialog.setStyleSheet("QDialog{background:#172330;color:white;} QLabel{color:white;font-size:17px;} "
        "QPushButton{min-height:42px;font-size:17px;padding:4px 10px;} "
        "QTableWidget{background:white;color:black;font-size:17px;} QLineEdit{min-height:36px;}");
}
QTableWidget *table(const QStringList &headers, QWidget *parent) {
    auto *t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->verticalHeader()->setDefaultSectionSize(42);
    return t;
}
}

void MainWindow::showPeople() {
    if (!recognition_.canTrigger() || (oneShot_ && !cycleReady_)) {
        QMessageBox::information(this, "暂不可管理", "请等待模型就绪、识别和舵机动作结束后重试。");
        return;
    }
    armGate_.cancel();
    managing_ = true;
    authorizationState_->setText("管理员验证中：已取消授权，不响应雷达");
    AdminPinDialog login(adminPin_, journal_, false, this);
    if (login.exec() != QDialog::Accepted) {
        managing_ = false;
        authorizationState_->setText("未进入管理；未启用：识别仅显示");
        journal_.record("management_denied", "未完成管理员验证");
        return;
    }
    authorizationState_->setText("人员管理中：已取消授权，不响应雷达");
    journal_.record("management_open", "PIN验证通过；120秒无操作退出");
    QDialog dialog(this);
    AdminSession session(dialog);
    dialog.setWindowTitle("人员管理");
    styleDialog(dialog);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("人员库（最多200人）· 管理员已验证 · 120秒无操作自动退出"));
    auto *list = table({"人员ID", "状态", "访问规则（UTC+08:00）"}, &dialog);
    list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    list->verticalHeader()->setDefaultSectionSize(60);
    layout->addWidget(list, 1);
    auto *message = new QLabel("正在加载；新增/更新前请保持单人正脸对准摄像头。");
    message->setWordWrap(true);
    layout->addWidget(message);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton("新增录入");
    auto *update = new QPushButton("更新特征");
    auto *toggle = new QPushButton("启用/停用");
    auto *remove = new QPushButton("删除");
    auto *refresh = new QPushButton("刷新");
    auto *close = new QPushButton("返回");
    auto *changePin = new QPushButton("修改PIN");
    auto *ruleButton = new QPushButton("设置访问规则");
    const QList<QPushButton *> actions{add, update, toggle, remove, refresh, ruleButton};
    for (auto *b : {add, update, toggle, remove, refresh}) buttons->addWidget(b);
    buttons->addWidget(close);
    buttons->addWidget(changePin);
    layout->addLayout(buttons);
    auto *rulesBar = new QHBoxLayout;
    rulesBar->addWidget(ruleButton);
    auto *rulesHelp = new QLabel("选中人员后设置有效期/每日时段；不改变启用状态；保存不动作。");
    rulesHelp->setWordWrap(true); rulesBar->addWidget(rulesHelp, 1);
    layout->addLayout(rulesBar);
    auto enable = [actions](bool value) { for (auto *b : actions) b->setEnabled(value); };
    auto submit = [this, message, enable, &session](const QString &op, const QString &id) {
        if (!session.active()) return;
        if (recognition_.manage(op, id)) { enable(false); message->setText("处理中，请稍候；不会发出舵机指令。"); }
        else message->setText("识别线程忙或未就绪，请稍后刷新。");
    };
    connect(&recognition_, &RecognitionWorker::people, &dialog,
            [list](const QStringList &ids, const QStringList &states, const QStringList &rules) {
        list->setRowCount(ids.size());
        for (int row = 0; row < ids.size(); ++row) {
            list->setItem(row, 0, new QTableWidgetItem(ids.at(row)));
            list->setItem(row, 1, new QTableWidgetItem(states.value(row)));
            auto *cell = new QTableWidgetItem;
            const QString json = rules.value(row);
            try { cell->setText(AccessRule::parse(QJsonDocument::fromJson(json.toUtf8()).object()).summary()); }
            catch (const std::exception &) { cell->setText("规则异常/缺失：拒绝授权"); }
            cell->setData(Qt::UserRole, json);
            list->setItem(row, 2, cell);
        }
    });
    connect(&recognition_, &RecognitionWorker::managementResult, &dialog,
            [message, enable](const QString &, const QString &, bool ok, const QString &detail) {
        enable(true);
        message->setText((ok ? "成功：" : "失败：") + detail);
    });
    connect(add, &QPushButton::clicked, &dialog, [this, &session, &dialog]() {
        bool ok = false;
        const QString proposed = "person_" + QUuid::createUuid().toString(QUuid::Id128).left(12);
        QString id = QInputDialog::getText(&dialog, "新增人员", "人员ID（英文/数字/_/-）；无键盘可直接使用自动ID：",
                                         QLineEdit::Normal, proposed, &ok).trimmed();
        if (ok && session.active()) enrollPerson("new",id,dialog,session);
    });
    connect(update, &QPushButton::clicked, &dialog, [this, list, &session, &dialog]() {
        int row = list->currentRow();
        if (row < 0) return;
        const QString id = list->item(row, 0)->text();
        if (QMessageBox::question(&dialog, "更新特征", "用当前摄像头中的单人替换 " + id + " 的特征？") == QMessageBox::Yes)
            enrollPerson("update",id,dialog,session);
    });
    connect(toggle, &QPushButton::clicked, &dialog, [list, submit]() {
        int row = list->currentRow();
        if (row >= 0) submit(list->item(row, 1)->text() == "启用" ? "disable" : "enable", list->item(row, 0)->text());
    });
    connect(remove, &QPushButton::clicked, &dialog, [list, submit, &dialog]() {
        int row = list->currentRow();
        if (row < 0) return;
        const QString id = list->item(row, 0)->text();
        if (QMessageBox::question(&dialog, "确认删除", "永久删除 " + id + " 的模板？需要重新录入才能恢复。",
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            submit("delete", id);
    });
    connect(refresh, &QPushButton::clicked, &dialog, [submit]() { submit("list", QString()); });
    connect(ruleButton, &QPushButton::clicked, &dialog, [this, list, message, enable, &dialog, &session]() {
        if (!session.active() || list->currentRow() < 0) return;
        const int row = list->currentRow();
        const QString id = list->item(row, 0)->text();
        AccessRule rule;
        try { rule = AccessRule::parse(QJsonDocument::fromJson(list->item(row, 2)->data(Qt::UserRole).toString().toUtf8()).object()); }
        catch (const std::exception &) {
            message->setText("规则缺失或损坏，请由root检查；不会自动恢复为全天允许。"); return;
        }
        AccessRuleDialog editor(id, rule, &dialog);
        if (editor.exec() != QDialog::Accepted || !session.active()) return;
        const QString payload = QString::fromUtf8(QJsonDocument(editor.rule().json()).toJson(QJsonDocument::Compact));
        if (recognition_.manage("rule", id, payload)) { enable(false); message->setText("正在保存访问规则…"); }
        else message->setText("识别线程忙，请刷新后重试。");
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(changePin, &QPushButton::clicked, &dialog, [this, &dialog, &session]() {
        if (!session.active()) return;
        AdminPinDialog change(adminPin_, journal_, true, &dialog);
        if (change.exec() == QDialog::Accepted) dialog.accept();
    });
    submit("list", QString());
    dialog.showFullScreen();
    dialog.exec();
    managing_ = false;
    authorizationState_->setText("管理结束，未启用：识别仅显示");
    journal_.record("management_close", session.expired() ? "空闲超时；未自动恢复授权" : "已退出；未自动恢复授权");
}

void MainWindow::showEvents() {
    armGate_.cancel();
    managing_=true;
    authorizationState_->setText("查看事件；已取消授权");
    journal_.record("events_open","只读查询；返回不恢复授权");
    EventQueryView dialog(this);
    auto refresh=[this,&dialog](){dialog.setSnapshot(journal_.snapshot());};
    connect(dialog.reload,&QPushButton::clicked,&dialog,refresh);
    connect(&journal_,&EventJournal::notice,&dialog,[&dialog](const QString &text){dialog.message->setText(text);});
    connect(dialog.exportButton,&QPushButton::clicked,&dialog,[this,&dialog](){
        dialog.message->setText(journal_.exportSelection(dialog.selectedLines())?
            "筛选快照已排队导出（后续刷新不改变此次内容）":"导出繁忙或数据无效");
    });
    refresh();dialog.showFullScreen();dialog.exec();
    managing_=false;
    authorizationState_->setText("查询结束；未启用：识别仅显示");
}
