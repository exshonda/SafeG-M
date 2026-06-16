# SafeG-M 遷移テスト Non-secure(最小ベアメタル) ビルド規則 — RP2350 / Raspberry Pi Pico2
#
# Secure ビルド(asp3_core, ENABLE_SAFEG_M=ON/ENABLE_SAFEG_IMPLIB=ON)が出力した
# CMSE import lib(secure_nsclib.o)をリンクする（必ず Secure を先にビルドすること）。
# NS を RP2350 の NS 領域 0x10200000(code)/0x20040000(RAM) に配置。
#
# 使い方: make NSCLIB=<Secureビルドの secure_nsclib.o の絶対パス>
#   例:   make NSCLIB=/tmp/m4_s1/secure_nsclib.o

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
COMMON  = ../../common
NSCLIB ?= /tmp/m4_s1/secure_nsclib.o

CFLAGS  = -mcpu=cortex-m33 -mthumb -mlittle-endian -mfloat-abi=softfp -mfpu=fpv5-sp-d16 \
          -O2 -Wall -ffreestanding -ffunction-sections -fdata-sections \
          -I. -I$(COMMON) $(EXTRA_CFLAGS)
LDFLAGS = -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=nstest.map -T ns_pico2.ld

OBJS = startup_ns.o ns_test_main.o

all: nstest.elf nstest.bin

startup_ns.o: startup_ns.c
	$(CC) $(CFLAGS) -c -o $@ $<

ns_test_main.o: ../ns_test_main.c
	$(CC) $(CFLAGS) -c -o $@ $<

nstest.elf: $(OBJS) $(NSCLIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(NSCLIB) -lgcc

nstest.bin: nstest.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) nstest.elf nstest.bin nstest.map
