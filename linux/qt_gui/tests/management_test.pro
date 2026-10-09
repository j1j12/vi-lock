QT = core sql
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = management_test
SOURCES += management_test.cpp ../person_store.cpp ../event_journal.cpp
HEADERS += ../person_store.h ../event_journal.h
SOURCES += ../event_journal_db.cpp ../../../telemetry/outbox/store.cpp
