#******************************************************************************
#
# Makefile - SafeG-M 遷移テスト Non-secure(最小ベアメタル) ビルド規則
#
# FreeRTOS/sample の makedefs(コンパイル/リンク規則)を流用し，FreeRTOS 本体を
# リンクせず最小構成(startup + ns_test_main + syscalls + sysmem)で RTOSDemo 相当の
# nstest.axf を生成する．Secure ビルドが生成した CMSE import lib(secure_nsclib.o)を
# リンクする（必ず Secure を先にビルドすること）．
#
#******************************************************************************

include ../../../FreeRTOS/sample/makedefs

# 出力先ディレクトリをパース時に確保（.d 出力先のため）
$(shell mkdir -p $(COMPILER))

FRTOS_DIR = ../../../FreeRTOS/sample
NS_TEMPL  = $(FRTOS_DIR)/mimxrt685evk_gcc
COMMON    = ../../common

# テストソース(.)、共通ヘッダ、FreeRTOS sample 由来の startup/syscalls/sysmem
CFLAGS += -I . -I $(COMMON) $(EXTRA_CFLAGS)
VPATH = .:..:$(FRTOS_DIR):$(NS_TEMPL)

# Secure ビルドが出力する CMSE import lib（固定パス: Makefile.target で指定）
NSCLIB = $(NS_TEMPL)/secure_nsclib.o

# makedefs の LDFLAGS は cwd の secure_nsclib.o を参照するため上書きする
# （import lib は ${^} 経由で 1 度だけリンクする）
LDFLAGS = -Xlinker -Map=$(COMPILER)/out.map -Xlinker --gc-sections

OBJS = $(COMPILER)/startup.o      \
       $(COMPILER)/ns_test_main.o \
       $(COMPILER)/syscalls.o     \
       $(COMPILER)/sysmem.o

all: $(COMPILER) $(COMPILER)/nstest.axf

clean:
	@rm -rf $(COMPILER) $(wildcard *.bin) nstest.axf

$(COMPILER):
	@mkdir $(COMPILER)

# オブジェクトは出力先ディレクトリ作成後にビルドする（.d 出力先の確保）
$(OBJS): | $(COMPILER)

$(COMPILER)/nstest.axf: $(OBJS) $(NSCLIB)
SCATTER_nstest = $(NS_TEMPL)/standalone.ld
ENTRY_nstest   = ResetISR

-include $(wildcard $(COMPILER)/*.d) __dummy__
