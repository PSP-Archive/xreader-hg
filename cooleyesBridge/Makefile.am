CC = @PSP_CC@
CCAS = $(CC)
AR = @PSP_AR@
RANLIB = @PSP_RANLIB@
CFLAGS = 

EXEEXT =
OBJEXT = o

AM_CFLAGS = -I $(top_srcdir)/src/include

if OPTIMIZE
AM_CFLAGS += -Os -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division 
else
AM_CFLAGS += -O0 -G0 -g -Wall
endif
CCASFLAGS = $(CFLAGS) $(INCLUDES)

LDFLAGS = -mno-crt0 -nostartfiles -L $(top_srcdir)/lib

noinst_PROGRAMS= cooleyesBridge.elf

cooleyesBridge_elf_SOURCES = main.c sceAudio_driver3xx.S sceMeCore_driver3xx.S

TARGET = cooleyesBridge

PSP_FW_VERSION = 635
BUILD_PRX=1

# Use the kernel's small inbuilt libc
USE_KERNEL_LIBC = 1
# Use only kernel libraries
USE_KERNEL_LIBS = 1
PRX_EXPORTS = cooleyesBridge.exp

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

all: cooleyesBridge.S
cooleyesBridge.S: cooleyesBridge.exp
	psp-build-exports -s $^
cooleyesBridge.o: cooleyesBridge.exp
	psp-build-exports -b $^ > cooleyesBridge.c
	$(CC) $(CFLAGS) -c -o $@ cooleyesBridge.c

clean-local:
	$(RM) cooleyesBridge.prx cooleyesBridge.S cooleyesBridge.c
