#include "diagnostic_views.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollBar>
EnrollmentView::EnrollmentView(const QString &operation,const QString &id,QWidget *parent) : QDialog(parent) {
    setStyleSheet("QDialog{background:#172330;} QLabel{color:white;font-size:17px;} QPushButton{font-size:19px;min-height:42px;padding:0 8px;}");
    setWindowTitle("人脸录入预览"); setFixedSize(980,560);
    auto *layout=new QVBoxLayout(this);
    auto *heading=new QLabel((operation=="new" ? "新增：" : "更新：")+id+" · 管理模式，不授权动作");
    heading->setWordWrap(true); layout->addWidget(heading);
    auto *grid=new QGridLayout;
    live=new QLabel("等待实时画面"); detected=new QLabel("等待检测");
    for(auto *view:{live,detected}) {
        view->setFixedSize(456,342); view->setAlignment(Qt::AlignCenter);
        view->setStyleSheet("background:black;color:#cbd5e1;border:1px solid #475569;");
    }
    grid->addWidget(new QLabel("实时画面（取景）"),0,0);
    grid->addWidget(new QLabel("同帧检测框（限频更新，非身份追踪）"),0,1);
    grid->addWidget(live,1,0); grid->addWidget(detected,1,1); layout->addLayout(grid);
    detectionText=new QLabel("预览检测不会保存。绿框=检测到单人，不代表身份或授权通过。");
    detectionText->setWordWrap(true); layout->addWidget(detectionText);
    message=new QLabel("保持单人正脸；点“采集并保存”后重新取新帧、检测和提取。");
    message->setWordWrap(true); layout->addWidget(message);
    auto *buttons=new QHBoxLayout;
    capture=new QPushButton("采集并保存"); closeButton=new QPushButton("返回 / 取消");
    buttons->addWidget(capture); buttons->addWidget(closeButton); layout->addLayout(buttons);
}
SystemStatusView::SystemStatusView(QWidget *parent) : QDialog(parent) {
    setWindowTitle("系统状态（只读）");
    setFixedSize(1000,580);
    setStyleSheet("QDialog{background:#172330;} QLabel{color:white;font-size:17px;} "
        "QTableWidget{background:white;color:black;font-size:16px;} QPushButton{font-size:20px;min-height:42px;}");
    auto *layout=new QVBoxLayout(this);
    auto *notice=new QLabel("只读采样约2秒刷新；不设置时间、不重启服务、不发控制命令。已开始的M4周期不会中断。");
    notice->setWordWrap(true); layout->addWidget(notice);
    const QStringList names{"GUI / Qt / 内核","M4运行状态","M4加载文件（非运行证明）","已加载驱动版本",
        "系统时间","RTC原始值","RTC/系统快照检查","系统运行时间","数据盘空间","事件文件大小",
        "摄像头状态","RPMsg状态","识别线程","当前门控","物理门到位"};
    table=new QTableWidget(names.size(),2);
    table->setHorizontalHeaderLabels({"项目","当前观察值"});
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    table->verticalHeader()->hide(); table->verticalHeader()->setDefaultSectionSize(40);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for(int i=0;i<names.size();++i) { table->setItem(i,0,new QTableWidgetItem(names[i])); table->setItem(i,1,new QTableWidgetItem("采样中 / UNKNOWN")); }
    layout->addWidget(table,1);
    auto *bottom=new QHBoxLayout;
    reload=new QPushButton("立即刷新"); back=new QPushButton("返回");
    auto *up=new QPushButton("向上翻页"), *down=new QPushButton("向下翻页");
    bottom->addWidget(up); bottom->addWidget(down); bottom->addWidget(reload); bottom->addWidget(back); layout->addLayout(bottom);
    connect(up,&QPushButton::clicked,this,[this]() { auto *s=table->verticalScrollBar(); s->setValue(s->value()-s->pageStep()); });
    connect(down,&QPushButton::clicked,this,[this]() { auto *s=table->verticalScrollBar(); s->setValue(s->value()+s->pageStep()); });
}
