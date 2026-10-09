#include "mainwindow.h"
#include "diagnostic_views.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>

void MainWindow::showSystemStatus() {
    armGate_.cancel(); managing_=true;
    authorizationState_->setText("查看系统状态，已取消待授权");
    journal_.record("status_open","只读状态页；取消待授权，返回不恢复");
    SystemStatusView dialog(this);
    auto *table=dialog.table;
    auto set=[table](int row,const QString &value) { table->item(row,1)->setText(value); table->item(row,1)->setToolTip(value); };
    connect(&statusWorker_,&StatusWorker::sample,&dialog,[set](const QStringList &values) {
        for(int i=0;i<values.size() && i<10;++i) set(i,values[i]);
    });
    auto refresh=[this,set]() {
        statusWorker_.request();
        set(10,cameraState_->text()); set(11,rpmsgState_->text());
        set(12,recognition_.canTrigger() ? "就绪且空闲" : "加载中 / 忙 / 异常（见主界面）");
        set(13,authorizationState_->text()); set(14,"UNKNOWN：尚无物理门磁/限位反馈");
    };
    auto *reload=dialog.reload, *back=dialog.back;
    connect(reload,&QPushButton::clicked,&dialog,refresh);
    connect(back,&QPushButton::clicked,&dialog,&QDialog::accept);
    QTimer timer; connect(&timer,&QTimer::timeout,&dialog,refresh); timer.start(2000); refresh();
    dialog.exec(); timer.stop(); managing_=false;
    authorizationState_->setText("状态页已退出；未启用：识别仅显示");
}
