# ARM cross-compilation toolchain file for STM32MP157 Cortex-A7
# Toolchain: arm-ostl-linux-gnueabi (OpenSTLinux SDK)
# Flags match the SDK target tune (cortexa7t2hf-neon-vfpv4) exactly.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(SDK_SYSROOT "/opt/st/stm32mp1/3.1-snapshot/sysroots/cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi")

set(CMAKE_C_COMPILER arm-ostl-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-ostl-linux-gnueabi-g++)

set(CMAKE_SYSROOT "${SDK_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${SDK_SYSROOT}")

set(ARM_TUNE_FLAGS "-mthumb -mfpu=neon-vfpv4 -mfloat-abi=hard -mcpu=cortex-a7")
set(CMAKE_C_FLAGS "${ARM_TUNE_FLAGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${ARM_TUNE_FLAGS} ${CMAKE_CXX_FLAGS}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
