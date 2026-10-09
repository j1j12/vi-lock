#ifndef EVENT_QUERY_VIEW_H
#define EVENT_QUERY_VIEW_H
#include <QDialog>
#include "event_query.h"
class QPushButton;
class QComboBox;
class QLabel;
class QTableWidget;
class EventQueryView : public QDialog {
public:
    explicit EventQueryView(QWidget *parent=nullptr);
    void setSnapshot(const QStringList &lines);
    QStringList selectedLines() const { return result_.lines; }
    QPushButton *reload, *exportButton;
    QLabel *message;
private:
    void apply();
    void chooseDate(bool first);
    QStringList source_;
    EventQueryResult result_;
    QDate first_,last_;
    QPushButton *dates_,*firstButton_,*lastButton_;
    QComboBox *person_,*type_;
    QLabel *summary_;
    QTableWidget *table_;
};
#endif
