QT = core sql
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = event_db_test
SOURCES += event_db_test.cpp ../event_journal.cpp ../event_journal_db.cpp ../../../telemetry/outbox/store.cpp
HEADERS += ../event_journal.h
