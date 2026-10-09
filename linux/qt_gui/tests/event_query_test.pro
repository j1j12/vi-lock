QT = core gui widgets sql
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = event_query_test
SOURCES += event_query_test.cpp ../event_query.cpp ../event_query_view.cpp ../event_journal.cpp
HEADERS += ../event_journal.h
SOURCES += ../event_journal_db.cpp ../../../telemetry/outbox/store.cpp
