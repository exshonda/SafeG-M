# SafeG-M 遷移テスト Non-secure(最小ベアメタル) ビルド規則 — ARM MPS2-AN505 / QEMU
#
# Secure ビルドが出力した CMSE import lib(secure_nsclib.o)をリンクする
# （必ず Secure を先にビルドすること）．NS を NS エイリアス 0x00200000/0x28200000 に配置．

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
COMMON  = ../../common
NSCLIB  = ../../../FreeRTOS/sample/mps2_an505_gcc/secure_nsclib.o

CFLAGS  = -mcpu=cortex-m33 -mthumb -mlittle-endian -mfloat-abi=softfp -mfpu=fpv5-sp-d16 \
          -O2 -Wall -ffreestanding -ffunction-sections -fdata-sections \
          -I. -I$(COMMON) $(EXTRA_CFLAGS)
LDFLAGS = -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=nstest.map -T ns_an505.ld

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
