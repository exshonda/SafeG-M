# SafeG-M 遷移テスト Non-secure(FreeRTOS版) ビルド規則 — RP2350 / Raspberry Pi Pico2
#
# FreeRTOS カーネル本体は freertos_base(import_freertos.sh で取得)から、
# port は ARM_CM33_NTZ/non_secure を用いる。main は ns_freertos/main_test.c。
# Secure ビルド(asp3_core, ENABLE_SAFEG_M=ON/ENABLE_SAFEG_IMPLIB=ON)が出力した
# CMSE import lib(secure_nsclib.o)をリンクする（必ず Secure を先にビルド）。
#
# 使い方: make NSCLIB=<Secureビルドの secure_nsclib.o の絶対パス>

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

COMMON  = ../../common
NS_SRC  = ..
FRTOS   = ../../../freertos_base
PORT    = $(FRTOS)/portable/GCC/ARM_CM33_NTZ/non_secure
MEMMANG = $(FRTOS)/portable/MemMang
TEMPL   = ../../../FreeRTOS/sample/pico2_arm_gcc

NSCLIB ?= /tmp/run_sec/secure_nsclib.o

CFLAGS  = -mcpu=cortex-m33 -mthumb -mlittle-endian -mfloat-abi=softfp -mfpu=fpv5-sp-d16 \
          -O2 -Wall -ffunction-sections -fdata-sections \
          -I$(TEMPL) -I$(COMMON) -I$(FRTOS)/include -I$(PORT) $(EXTRA_CFLAGS)
LDFLAGS = -nostartfiles --specs=nano.specs --specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=out.map -T $(TEMPL)/standalone.ld

OBJS = startup.o main_test.o \
       tasks.o queue.o list.o timers.o event_groups.o stream_buffer.o croutine.o \
       port.o portasm.o heap_4.o

all: nstest.axf nstest.bin

startup.o:   $(TEMPL)/startup.c   ; $(CC) $(CFLAGS) -c -o $@ $<
main_test.o: $(NS_SRC)/main_test.c; $(CC) $(CFLAGS) -c -o $@ $<
port.o:      $(PORT)/port.c       ; $(CC) $(CFLAGS) -c -o $@ $<
portasm.o:   $(PORT)/portasm.c    ; $(CC) $(CFLAGS) -c -o $@ $<
heap_4.o:    $(MEMMANG)/heap_4.c  ; $(CC) $(CFLAGS) -c -o $@ $<
%.o:         $(FRTOS)/%.c         ; $(CC) $(CFLAGS) -c -o $@ $<

nstest.axf: $(OBJS) $(NSCLIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(NSCLIB) -lgcc

nstest.bin: nstest.axf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f *.o nstest.axf nstest.bin out.map
