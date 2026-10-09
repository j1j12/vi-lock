QT = core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = rules_test
SOURCES += rules_test.cpp ../access_rules.cpp ../access_rule_dialog.cpp ../person_store.cpp
HEADERS += ../access_rules.h ../access_rule_dialog.h ../person_store.h ../oneshot_gate.h
