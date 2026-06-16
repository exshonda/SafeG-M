#******************************************************************************
#
# Makefile - SafeG-M 遷移テスト Non-secure(FreeRTOS版) ビルド規則
#
# FreeRTOS/sample の makedefs と startup/標準ソースを流用し，main を本テスト用
# main_test.c に差し替えてビルドする．Secure ビルドが生成した CMSE import lib
# (secure_nsclib.o = テスト gate) をリンクする（必ず Secure を先にビルド）．
#
#******************************************************************************

include ../../../FreeRTOS/sample/makedefs

# 出力先をパース時に確保
$(shell mkdir -p $(COMPILER))

FREERTOS_BASE ?= ../../../freertos_base
RTOS_DIR  = $(FREERTOS_BASE)
FRTOS_SMP = ../../../FreeRTOS/sample
NS_TEMPL  = $(FRTOS_SMP)/mimxrt685evk_gcc
COMMON    = ../../common

CFLAGS += -I . -I $(NS_TEMPL) -I $(COMMON) \
          -I $(RTOS_DIR)/include \
          -I $(RTOS_DIR)/portable/GCC/ARM_CM33_NTZ/non_secure

VPATH = .:..:$(RTOS_DIR):$(RTOS_DIR)/portable/MemMang:$(RTOS_DIR)/portable/GCC/ARM_CM33_NTZ/non_secure:$(FRTOS_SMP):$(NS_TEMPL)

NSCLIB = $(NS_TEMPL)/secure_nsclib.o
LDFLAGS = -Xlinker -Map=$(COMPILER)/out.map -Xlinker --gc-sections

OBJS = $(COMPILER)/main_test.o    \
       $(COMPILER)/croutine.o     \
       $(COMPILER)/event_groups.o \
       $(COMPILER)/list.o         \
       $(COMPILER)/queue.o        \
       $(COMPILER)/stream_buffer.o\
       $(COMPILER)/tasks.o        \
       $(COMPILER)/timers.o       \
       $(COMPILER)/port.o         \
       $(COMPILER)/portasm.o      \
       $(COMPILER)/heap_4.o       \
       $(COMPILER)/syscalls.o     \
       $(COMPILER)/sysmem.o

INIT_OBJS = $(COMPILER)/startup.o

all: $(COMPILER) $(COMPILER)/nstest.axf

clean:
	@rm -rf $(COMPILER) $(wildcard *.bin) nstest.axf

$(COMPILER):
	@mkdir -p $(COMPILER)

$(COMPILER)/nstest.axf: $(INIT_OBJS) $(OBJS) $(NSCLIB)
SCATTER_nstest = $(NS_TEMPL)/standalone.ld
ENTRY_nstest   = ResetISR

-include $(wildcard $(COMPILER)/*.d) __dummy__
