#ifndef DIAGNOSTIC_VIEWS_H
#define DIAGNOSTIC_VIEWS_H
#include <QDialog>
class QLabel;
class QPushButton;
class QTableWidget;
class EnrollmentView : public QDialog {
public:
    EnrollmentView(const QString &operation,const QString &id,QWidget *parent=nullptr);
    QLabel *live,*detected,*detectionText,*message;
    QPushButton *capture,*closeButton;
};
class SystemStatusView : public QDialog {
public:
    explicit SystemStatusView(QWidget *parent=nullptr);
    QTableWidget *table;
    QPushButton *reload,*back;
};
#endif
