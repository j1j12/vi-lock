#include "event_query_view.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QCalendarWidget>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSet>
EventQueryView::EventQueryView(QWidget *parent):QDialog(parent) {
    setWindowTitle("事件查询与统计"); resize(1000,580);
    first_=last_=QDateTime::currentDateTimeUtc().toOffsetFromUtc(8*3600).date();
    setStyleSheet("QDialog{background:#172330;color:white;} QLabel{color:white;font-size:16px;} "
        "QPushButton{min-height:40px;font-size:17px;padding:3px 8px;} "
        "QComboBox{min-height:40px;font-size:16px;} QTableWidget{background:white;color:black;font-size:16px;}");
    auto *layout=new QVBoxLayout(this);
    layout->addWidget(new QLabel("最近最多500条快照 · 日期按UTC+08 · 记录数不是通行人数/物理开门次数"));
    auto *dateRow=new QHBoxLayout;
    dates_=new QPushButton("□ 限定日期"); dates_->setCheckable(true);
    firstButton_=new QPushButton; lastButton_=new QPushButton;
    dateRow->addWidget(dates_); dateRow->addWidget(firstButton_); dateRow->addWidget(lastButton_);
    layout->addLayout(dateRow);
    auto *selectors=new QHBoxLayout;
    person_=new QComboBox; type_=new QComboBox;
    person_->setObjectName("personFilter"); type_->setObjectName("typeFilter");
    selectors->addWidget(new QLabel("人员")); selectors->addWidget(person_,1);
    selectors->addWidget(new QLabel("类型")); selectors->addWidget(type_,1);
    layout->addLayout(selectors);
    summary_=new QLabel; summary_->setWordWrap(true); summary_->setMinimumHeight(44);
    layout->addWidget(summary_);
    table_=new QTableWidget(0,4); table_->setHorizontalHeaderLabels({"时间","类型","人员","详情"});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setDefaultSectionSize(40);
    layout->addWidget(table_,1);
    auto *buttons=new QHBoxLayout;
    reload=new QPushButton("刷新快照"); exportButton=new QPushButton("导出筛选结果");
    auto *up=new QPushButton("上翻"), *down=new QPushButton("下翻"),*close=new QPushButton("返回");
    for(auto *button:{up,down,reload,exportButton,close}) buttons->addWidget(button);
    layout->addLayout(buttons);
    message=new QLabel("导出当前快照到events-filtered.jsonl，覆盖上次筛选导出；不包含人脸照片。");
    message->setWordWrap(true); message->setFixedHeight(52); layout->addWidget(message);
    connect(up,&QPushButton::clicked,this,[this](){auto *s=table_->verticalScrollBar();s->setValue(s->value()-s->pageStep());});
    connect(down,&QPushButton::clicked,this,[this](){auto *s=table_->verticalScrollBar();s->setValue(s->value()+s->pageStep());});
    connect(close,&QPushButton::clicked,this,&QDialog::accept);
    connect(dates_,&QPushButton::toggled,this,[this](){apply();});
    connect(firstButton_,&QPushButton::clicked,this,[this](){chooseDate(true);});
    connect(lastButton_,&QPushButton::clicked,this,[this](){chooseDate(false);});
    connect(person_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](){apply();});
    connect(type_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](){apply();});
    connect(table_,&QTableWidget::cellClicked,this,[this](int row,int){
        message->setText(table_->item(row,1)->text()+"："+table_->item(row,3)->text());
    });
    setSnapshot({});
}
void EventQueryView::setSnapshot(const QStringList &lines) {
    source_=lines.mid(qMax(0,lines.size()-500));
    const auto oldPerson=person_->currentData().toString(),oldType=type_->currentData().toString();
    QSignalBlocker personBlock(person_),typeBlock(type_);
    QSet<QString> persons,types;
    for(const auto &line:source_) {
        auto obj=QJsonDocument::fromJson(line.toUtf8()).object();
        if(!obj["person"].toString().isEmpty()) persons.insert(obj["person"].toString());
        if(!obj["type"].toString().isEmpty()) types.insert(obj["type"].toString());
    }
    if(!oldPerson.isEmpty()) persons.insert(oldPerson);
    if(!oldType.isEmpty()) types.insert(oldType);
    QStringList p=persons.values(),t=types.values(); p.sort();t.sort();
    person_->clear(); person_->addItem("全部人员",QString());
    type_->clear(); type_->addItem("全部类型",QString());
    for(const auto &value:p) person_->addItem(value,value);
    const QMap<QString,QString> labels{{"recognition_match","识别匹配"},{"access_denied","规则拒绝"},
        {"cycle_requested","动作请求"},{"cycle_finished","周期结束反馈"}};
    for(const auto &value:t) type_->addItem(labels.value(value,value),value);
    person_->setCurrentIndex(qMax(0,person_->findData(oldPerson)));
    type_->setCurrentIndex(qMax(0,type_->findData(oldType)));
    apply();
}
void EventQueryView::apply() {
    dates_->setText(dates_->isChecked()?"✓ 限定日期":"□ 限定日期");
    firstButton_->setText("起始 "+first_.toString("yyyy-MM-dd")); lastButton_->setText("截止 "+last_.toString("yyyy-MM-dd"));
    firstButton_->setEnabled(dates_->isChecked());lastButton_->setEnabled(dates_->isChecked());
    EventFilter filter; filter.dates=dates_->isChecked();filter.first=first_;filter.last=last_;
    filter.person=person_->currentData().toString();filter.type=type_->currentData().toString();
    result_=queryEvents(source_,filter);
    exportButton->setEnabled(result_.valid);
    summary_->setText(result_.valid ? QString("筛选%1 / 快照%2条；异常%3条。匹配%4 · 规则拒绝%5 · 请求%6 · 周期反馈%7\n统计仅针对筛选结果；旧版未记录的匹配/周期事件不补算，周期反馈无人员归属。")
        .arg(result_.lines.size()).arg(source_.size()).arg(result_.invalid).arg(result_.counts.value("recognition_match"))
        .arg(result_.counts.value("access_denied")).arg(result_.counts.value("cycle_requested")).arg(result_.counts.value("cycle_finished"))
        : "日期范围无效：起始不能晚于截止；禁止导出。");
    table_->setRowCount(result_.lines.size());
    int row=0;
    for(int i=result_.lines.size()-1;i>=0;--i,++row) {
        const auto obj=QJsonDocument::fromJson(result_.lines[i].toUtf8()).object();
        const auto stamp=QDateTime::fromString(obj["time"].toString(),Qt::ISODateWithMs);
        QStringList cells{stamp.isValid()?stamp.toOffsetFromUtc(8*3600).toString("MM-dd HH:mm:ss"):"时间异常",
            obj["type"].toString(),obj["person"].toString(),obj["detail"].toString()};
        for(int col=0;col<4;++col) table_->setItem(row,col,new QTableWidgetItem(cells[col]));
    }
}
void EventQueryView::chooseDate(bool first) {
    QDialog dialog(this); dialog.setWindowTitle("选择日期（UTC+08）");dialog.resize(720,480);
    auto *layout=new QVBoxLayout(&dialog);auto *calendar=new QCalendarWidget;
    calendar->setMinimumDate(QDate(1970,1,1));calendar->setMaximumDate(QDate(2099,12,31));
    calendar->setSelectedDate(first?first_:last_);layout->addWidget(calendar);
    auto *row=new QHBoxLayout;auto *previous=new QPushButton("上月"),*next=new QPushButton("下月");
    auto *ok=new QPushButton("确定"),*cancel=new QPushButton("取消");
    for(auto *b:{previous,next,ok,cancel}) row->addWidget(b);
    layout->addLayout(row);
    connect(previous,&QPushButton::clicked,calendar,&QCalendarWidget::showPreviousMonth);
    connect(next,&QPushButton::clicked,calendar,&QCalendarWidget::showNextMonth);
    connect(ok,&QPushButton::clicked,&dialog,&QDialog::accept);connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){(first?first_:last_)=calendar->selectedDate();apply();}
}
