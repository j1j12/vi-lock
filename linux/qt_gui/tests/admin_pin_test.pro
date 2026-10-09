QT = core gui widgets sql
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = admin_pin_test
SOURCES += admin_pin_test.cpp ../admin_pin_store.cpp ../admin_pin_worker.cpp ../admin_pin_dialog.cpp ../admin_session.cpp ../event_journal.cpp
HEADERS += ../admin_pin_store.h ../admin_pin_worker.h ../admin_pin_dialog.h ../admin_session.h ../event_journal.h
SOURCES += ../event_journal_db.cpp ../../../telemetry/outbox/store.cpp
