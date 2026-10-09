#ifndef ACCESS_RULE_DIALOG_H
#define ACCESS_RULE_DIALOG_H
#include <QDialog>
#include "access_rules.h"
class QCheckBox;
class QDateEdit;
class QTimeEdit;
class AccessRuleDialog : public QDialog {
public:
    AccessRuleDialog(const QString &id, const AccessRule &rule, QWidget *parent = nullptr);
    AccessRule rule() const;
private:
    QCheckBox *dates_, *hours_;
    QDateEdit *first_, *last_;
    QTimeEdit *start_, *end_;
};
#endif
