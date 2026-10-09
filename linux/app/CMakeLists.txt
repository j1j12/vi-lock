cmake_minimum_required(VERSION 3.10)
project(face_app CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenCV: manual paths (find_package fails because the SDK's OpenCVConfig.cmake
# references a missing 3rdparty/libcorrespondence.a)
set(SDK_SYSROOT "/opt/st/stm32mp1/3.1-snapshot/sysroots/cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi")
include_directories(${SDK_SYSROOT}/usr/include/opencv4)
link_directories(${SDK_SYSROOT}/usr/lib)

# NCNN cross-compiled (manual install path, default ~/ncnn/build-arm/install)
set(NCNN_DIR "$ENV{HOME}/ncnn/build-arm/install" CACHE PATH "NCNN install prefix")
include_directories(${NCNN_DIR}/include)
include_directories(${NCNN_DIR}/include/ncnn)

add_executable(face_app
    face_app.cpp
    face_engine.cpp
    v4l2_capture.cpp
    rpmsg_client.cpp
)

target_link_libraries(face_app
    opencv_imgproc
    opencv_core
    ${NCNN_DIR}/lib/libncnn.a
    pthread
)
