#include "access_rule_dialog.h"
#include <QCheckBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QCalendarWidget>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <stdexcept>
#include <QPainter>
#include <QPaintEvent>

namespace {
// Paint the mark ourselves: linuxfb themes/fonts need not supply checkbox icons.
class RuleToggle : public QCheckBox {
public:
    RuleToggle(const QString &label, const QString &off) : QCheckBox(label), off_(off) {
        setFixedHeight(44);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
    }
    QSize sizeHint() const override { return QSize(600,44); }
protected:
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), isChecked() ? QColor("#164e63") : QColor("#263445"));
        const QRect box(10,8,28,28);
        p.setPen(QPen(QColor("#e2e8f0"),2)); p.setBrush(QColor("#111922")); p.drawRoundedRect(box,3,3);
        if(isChecked()) {
            p.setPen(QPen(QColor("#5eead4"),3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p.drawLine(QPoint(16,22),QPoint(22,28)); p.drawLine(QPoint(22,28),QPoint(33,16));
        }
        QFont f=font(); f.setPixelSize(18); p.setFont(f); p.setPen(Qt::white);
        p.drawText(QRect(52,0,width()-64,height()),Qt::AlignLeft|Qt::AlignVCenter,
                   text() + (isChecked() ? "　已启用" : "　未启用 · " + off_));
        if(hasFocus()) { p.setPen(QPen(Qt::white,1,Qt::DashLine)); p.setBrush(Qt::NoBrush); p.drawRect(rect().adjusted(2,2,-3,-3)); }
    }
private:
    QString off_;
};

void touchValue(QDateTimeEdit *target, bool date, const QString &title, QWidget *parent) {
    QDialog pad(parent);
    pad.setObjectName("ruleTouchPad");
    pad.setWindowTitle(title);
    pad.setFixedSize(560, 460);
    auto *layout = new QVBoxLayout(&pad);
    layout->setContentsMargins(12,12,12,12); layout->setSpacing(6);
    layout->addWidget(new QLabel(title + (date ? "：输入8位日期，例如20260924" : "：输入4位时间，例如0830")));
    auto *input = new QLineEdit;
    input->setObjectName("ruleDigits");
    input->setReadOnly(true); // All input is through the explicit touch buttons.
    input->setAlignment(Qt::AlignCenter);
    input->setStyleSheet("background:white;color:black;font-size:28px;min-height:42px;");
    input->setMaxLength(date ? 8 : 4);
    input->setText(date ? target->date().toString("yyyyMMdd") : target->time().toString("HHmm"));
    layout->addWidget(input);
    auto *message = new QLabel("先点清空，再输入；取消不会改变原值。");
    message->setWordWrap(true); layout->addWidget(message);
    auto *keys = new QGridLayout;
    const QStringList captions{"1","2","3","4","5","6","7","8","9","清空","0","退格"};
    for (int i=0; i<captions.size(); ++i) {
        const QString key=captions[i];
        auto *button=new QPushButton(key);
        button->setObjectName("digit_"+key);
        keys->addWidget(button,i/3,i%3);
        QObject::connect(button,&QPushButton::clicked,&pad,[input,message,key]() {
            QString text=input->text();
            if(key=="清空") text.clear();
            else if(key=="退格") text.chop(1);
            else if(text.size()<input->maxLength()) text+=key;
            input->setText(text); message->setText("输入完成后点确认。");
        });
    }
    layout->addLayout(keys);
    auto *bottom=new QHBoxLayout;
    auto *ok=new QPushButton("确认"), *cancel=new QPushButton("取消");
    ok->setObjectName("applyTouchValue"); cancel->setObjectName("cancelTouchValue");
    bottom->addWidget(ok); bottom->addWidget(cancel); layout->addLayout(bottom);
    QObject::connect(cancel,&QPushButton::clicked,&pad,&QDialog::reject);
    QObject::connect(ok,&QPushButton::clicked,&pad,[&pad,input,message,date]() {
        const QString text=input->text();
        const QDate d=QDate::fromString(text,"yyyyMMdd");
        const QTime t=QTime::fromString(text,"HHmm");
        const bool valid=date ? text.size()==8 && d.isValid() && d>=QDate(2026,1,1) && d<=QDate(2099,12,31)
                              : text.size()==4 && t.isValid();
        if(!valid) { message->setText(date ? "日期无效：请输入2026–2099年的真实日期（8位）。" : "时间无效：小时00–23，分钟00–59（4位）。"); return; }
        pad.accept();
    });
    // Nested under the management dialog: AdminSession expiry rejects this too.
    if(pad.exec()!=QDialog::Accepted) return;
    if(date) target->setDate(QDate::fromString(input->text(),"yyyyMMdd"));
    else target->setTime(QTime::fromString(input->text(),"HHmm"));
}
}

AccessRuleDialog::AccessRuleDialog(const QString &id, const AccessRule &r, QWidget *parent) : QDialog(parent) {
    setWindowTitle("人员访问规则");
    setFixedSize(960, 580); // Verified 1024x600 panel; leave room around the modal.
    setStyleSheet("QDialog{background:#172330;} QLabel,QCheckBox{color:white;font-size:18px;} "
        "QDateTimeEdit{background:white;color:black;font-size:22px;min-height:40px;padding:0 8px;} "
        "QDateTimeEdit::up-button,QDateTimeEdit::down-button{width:48px;} "
        "QCheckBox::indicator{width:26px;height:26px;} "
        "QPushButton{min-height:40px;font-size:19px;padding:0 8px;} QCalendarWidget{background:white;color:black;}");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12,10,12,10); layout->setSpacing(6);
    auto *heading=new QLabel("人员：" + id + "　· UTC+08:00"); heading->setWordWrap(true);
    layout->addWidget(heading);
    dates_ = new RuleToggle("限制有效日期", "长期有效");
    hours_ = new RuleToggle("限制每日时段", "全天允许");
    dates_->setChecked(r.dates); hours_->setChecked(r.hours);
    dates_->setObjectName("restrictDates"); hours_->setObjectName("restrictHours");
    auto today = QDateTime::currentDateTimeUtc().toOffsetFromUtc(28800).date();
    if (today < QDate(2026,1,1) || today > QDate(2099,12,31)) today = QDate(2026,1,1);
    first_ = new QDateEdit(r.dates ? r.first : today);
    last_ = new QDateEdit(r.dates ? r.last : today);
    first_->setObjectName("firstDate"); last_->setObjectName("lastDate");
    for (auto *edit : {first_, last_}) {
        edit->setDisplayFormat("yyyy-MM-dd"); edit->setCalendarPopup(false);
        edit->setReadOnly(true); edit->setButtonSymbols(QAbstractSpinBox::NoButtons);
        edit->setDateRange(QDate(2026,1,1), QDate(2099,12,31));
        edit->setEnabled(r.dates);
        connect(dates_, &QCheckBox::toggled, edit, &QWidget::setEnabled);
    }
    start_ = new QTimeEdit(r.hours ? QTime(r.startMinute / 60, r.startMinute % 60) : QTime(8,0));
    end_ = new QTimeEdit(r.hours ? QTime(r.endMinute / 60, r.endMinute % 60) : QTime(18,0));
    start_->setObjectName("startTime"); end_->setObjectName("endTime");
    for (auto *edit : {start_, end_}) {
        edit->setDisplayFormat("HH:mm"); edit->setEnabled(r.hours);
        edit->setReadOnly(true); edit->setButtonSymbols(QAbstractSpinBox::NoButtons);
        connect(hours_, &QCheckBox::toggled, edit, &QWidget::setEnabled);
    }
    layout->addWidget(dates_);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16); grid->setVerticalSpacing(6);
    grid->addWidget(new QLabel("开始日期"),0,0); grid->addWidget(first_,0,1);
    grid->addWidget(new QLabel("结束日期"),0,2); grid->addWidget(last_,0,3);
    auto addInput = [this](QGridLayout *row, QDateTimeEdit *field, QCheckBox *enabled, bool date, const QString &caption, const QString &name, int column) {
        auto *button=new QPushButton(caption); button->setObjectName(name);
        button->setEnabled(enabled->isChecked()); row->addWidget(button,1,column,1,2);
        connect(enabled,&QCheckBox::toggled,button,&QWidget::setEnabled);
        connect(button,&QPushButton::clicked,this,[this,field,date,caption]() { touchValue(field,date,caption,this); });
    };
    addInput(grid,first_,dates_,true,"输入开始日期","editFirstDate",0);
    addInput(grid,last_,dates_,true,"输入结束日期","editLastDate",2);
    layout->addLayout(grid);
    layout->addWidget(hours_);
    auto *times = new QGridLayout;
    times->setHorizontalSpacing(16); times->setVerticalSpacing(6);
    times->addWidget(new QLabel("允许开始"),0,0); times->addWidget(start_,0,1);
    times->addWidget(new QLabel("允许结束"),0,2); times->addWidget(end_,0,3);
    addInput(times,start_,hours_,false,"输入开始时间","editStartTime",0);
    addInput(times,end_,hours_,false,"输入结束时间","editEndTime",2);
    layout->addLayout(times);
    auto *help = new QLabel("日期首末日包含；时段开始包含、结束不包含，22:00–06:00支持跨午夜。\n"
                           "跨午夜仍受有效日期限制；时间检查失败拒绝授权。保存不会触发动作。");
    help->setWordWrap(true); layout->addWidget(help);
    auto *message = new QLabel; message->setMinimumHeight(24); message->setWordWrap(true); layout->addWidget(message);
    for (auto *edit : {start_, end_}) connect(edit, &QTimeEdit::timeChanged, message, [message]() { message->clear(); });
    for (auto *edit : {first_, last_}) connect(edit, &QDateEdit::dateChanged, message, [message]() { message->clear(); });
    auto *buttons = new QHBoxLayout;
    auto *save = new QPushButton("保存规则"), *cancel = new QPushButton("取消");
    save->setObjectName("saveRule");
    buttons->addWidget(save); buttons->addWidget(cancel); layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, [this, message]() {
        try { rule().validate(); accept(); }
        catch (const std::exception &) { message->setText("规则无效：结束日期不能早于开始日期；时段起止不能相同。"); }
    });
}
AccessRule AccessRuleDialog::rule() const {
    AccessRule r; r.dates = dates_->isChecked(); r.hours = hours_->isChecked();
    r.first = first_->date(); r.last = last_->date();
    r.startMinute = start_->time().hour()*60 + start_->time().minute();
    r.endMinute = end_->time().hour()*60 + end_->time().minute();
    return r;
}
