cmake_minimum_required(VERSION 3.10)
project(access_control_gui CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets Sql)

set(SDK_SYSROOT "/opt/st/stm32mp1/3.1-snapshot/sysroots/cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi")
include_directories(${SDK_SYSROOT}/usr/include/opencv4 ../app)
link_directories(${SDK_SYSROOT}/usr/lib)
set(NCNN_DIR "$ENV{HOME}/ncnn/build-arm/install" CACHE PATH "NCNN install prefix")
if(NOT EXISTS "${NCNN_DIR}/lib/libncnn.a")
    message(FATAL_ERROR "NCNN library missing: ${NCNN_DIR}/lib/libncnn.a")
endif()
include_directories(${NCNN_DIR}/include ${NCNN_DIR}/include/ncnn)

add_executable(access_control_gui
    main.cpp
    mainwindow.cpp
    rpmsg_worker.cpp
    rpmsg_worker.h
    vision_worker.cpp
    vision_worker.h
    person_store.cpp
    access_rules.cpp
    access_rule_dialog.cpp
    face_overlay.cpp
    diagnostic_views.cpp
    enrollment_ui.cpp
    status_ui.cpp
    status_worker.cpp
    status_worker.h
    event_journal.cpp
    event_journal_db.cpp
    ../../telemetry/outbox/store.cpp
    event_query.cpp
    event_query_view.cpp
    event_journal.h
    management_ui.cpp
    admin_pin_store.cpp
    admin_pin_worker.cpp
    admin_pin_worker.h
    admin_pin_dialog.cpp
    admin_session.cpp
    ../app/face_engine.cpp
    ../app/v4l2_capture.cpp
)

target_link_libraries(access_control_gui
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Sql
    opencv_imgproc opencv_core ${NCNN_DIR}/lib/libncnn.a pthread
)
