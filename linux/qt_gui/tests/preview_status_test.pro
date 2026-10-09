QT = core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = preview_status_test
SOURCES += preview_status_test.cpp ../face_overlay.cpp ../status_worker.cpp ../access_rules.cpp ../person_store.cpp ../diagnostic_views.cpp
HEADERS += ../status_worker.h ../face_overlay.h ../enrollment_session.h
