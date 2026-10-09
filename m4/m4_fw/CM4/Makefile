# ============================================================================
# M4 firmware build for STM32MP157DAA1 (Access Control System)
#
# Toolchain: arm-none-eabi-gcc 13.3.1 (bundled with STM32CubeIDE 1.18.0)
#   gcc : .../gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin
#   make: .../externaltools.make.win32_2.2.0.202409170845/tools/bin
#
# Ensure the toolchain bin directory is on PATH before running `make`.
# ============================================================================

TARGET = m4_fw_CM4

# Use a Unix shell (Git Bash sh) for recipes; add Git bin to PATH when building.
SHELL := C:/Users/Acer/AppData/Local/Programs/Git/bin/sh.exe

PREFIX = arm-none-eabi-
CC     = $(PREFIX)gcc
AS     = $(PREFIX)gcc
LD     = $(PREFIX)gcc
SZ     = $(PREFIX)size
HEX    = $(PREFIX)objcopy

BUILD_DIR = build

# --- CPU / FPU ---------------------------------------------------------------
CPU  = -mcpu=cortex-m4
FPU  = -mfpu=fpv4-sp-d16
FABI = -mfloat-abi=hard
MCU  = $(CPU) -mthumb $(FPU) $(FABI)

# --- Defines / includes ------------------------------------------------------
DEFS = -DUSE_HAL_DRIVER -DSTM32MP157Dxx -DCORE_CM4 \
	-DMETAL_FREERTOS -DMETAL_INTERNAL -DMETAL_MAX_DEVICE_REGIONS=2 \
	-DVIRTIO_SLAVE_ONLY -DNO_ATOMIC_64_SUPPORT

# Diagnostic build: `make RPMSG_BRINGUP_ONLY=1` starts only FreeRTOS + OpenAMP
# and deliberately skips the application GPIO/UART/TIM tasks.
ifeq ($(RPMSG_BRINGUP_ONLY),1)
DEFS += -DRPMSG_BRINGUP_ONLY -D__LOG_TRACE_IO_
endif

C_INCLUDES = \
-I Core/Inc \
-I App \
-I App/openamp \
-I ../Drivers/STM32MP1xx_HAL_Driver/Inc \
-I ../Drivers/STM32MP1xx_HAL_Driver/Inc/Legacy \
-I ../Middlewares/Third_Party/FreeRTOS/Source/include \
-I ../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
-I ../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
-I ../Middlewares/Third_Party/OpenAMP/open-amp/lib/include \
-I ../Middlewares/Third_Party/OpenAMP/libmetal/lib/include \
-I ../Drivers/CMSIS/RTOS2/Include \
-I ../Drivers/CMSIS/Device/ST/STM32MP1xx/Include \
-I ../Drivers/CMSIS/Include

# --- Flags -------------------------------------------------------------------
CFLAGS  = $(MCU) $(DEFS) $(C_INCLUDES) -O0 -g3 -Wall -fdata-sections -ffunction-sections
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

LDSCRIPT = STM32MP157DAAX_RAM.ld

LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) \
	-Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# --- Sources -----------------------------------------------------------------
C_SOURCES = \
Core/Src/main.c \
Core/Src/freertos.c \
Core/Src/gpio.c \
Core/Src/usart.c \
Core/Src/tim.c \
Core/Src/ipcc.c \
Core/Src/stm32mp1xx_it.c \
Core/Src/stm32mp1xx_hal_msp.c \
Core/Src/stm32mp1xx_hal_timebase_tim.c \
Core/Src/syscalls.c \
Core/Src/sysmem.c \
../Common/System/system_stm32mp1xx.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_cortex.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_dma.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_dma_ex.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_exti.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_gpio.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_hsem.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_ipcc.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_mdma.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_pwr.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_pwr_ex.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_rcc.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_rcc_ex.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_tim.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_tim_ex.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_uart.c \
../Drivers/STM32MP1xx_HAL_Driver/Src/stm32mp1xx_hal_uart_ex.c \
../Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
../Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
../Middlewares/Third_Party/FreeRTOS/Source/list.c \
../Middlewares/Third_Party/FreeRTOS/Source/queue.c \
../Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
../Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
../Middlewares/Third_Party/FreeRTOS/Source/timers.c \
../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c \
../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c \
../Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c \
../Middlewares/Third_Party/FreeRTOS/Source/portable/Common/mpu_wrappers.c \
../Middlewares/Third_Party/FreeRTOS/Source/portable/Common/mpu_wrappers_v2.c \
../Middlewares/Third_Party/OpenAMP/open-amp/lib/remoteproc/remoteproc_virtio.c \
../Middlewares/Third_Party/OpenAMP/open-amp/lib/rpmsg/rpmsg.c \
../Middlewares/Third_Party/OpenAMP/open-amp/lib/rpmsg/rpmsg_virtio.c \
../Middlewares/Third_Party/OpenAMP/open-amp/lib/virtio/virtio.c \
../Middlewares/Third_Party/OpenAMP/open-amp/lib/virtio/virtqueue.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/device.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/init.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/io.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/log.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/shmem.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/freertos/condition.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/freertos/freertos_device.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/freertos/freertos_init.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/freertos/freertos_io.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/freertos/freertos_shmem.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/freertos/time.c \
../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/freertos/template/sys.c \
App/openamp/openamp.c \
App/openamp/rsc_table.c \
App/openamp/mbox_ipcc.c \
App/openamp/lock_resource.c \
App/openamp/openamp_log.c \
App/openamp/trace_format.c \
App/app_access.c \
App/hal_servo.c \
App/hal_presence.c \
App/hal_doorlock.c \
App/task_sensor.c \
App/task_door.c \
App/task_rpmsg.c

ASM_SOURCES = \
Core/Startup/startup_stm32mp157daax.s

# --- Objects -----------------------------------------------------------------
OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.s=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

DEPS = $(OBJECTS:.o=.d)

# --- Rules -------------------------------------------------------------------
.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(HEX) -O ihex $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
