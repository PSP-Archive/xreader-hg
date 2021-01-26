CC = @PSP_CC@
CCAS = $(CC)
AR = @PSP_AR@
RANLIB = @PSP_RANLIB@
CFLAGS = 

EXEEXT =
OBJEXT = o

AM_CFLAGS = -I $(top_srcdir)/src/include

if OPTIMIZE
AM_CFLAGS += -Os -G0 -Wall -fno-builtin-printf
else
AM_CFLAGS += -O0 -G0 -g -Wall
endif
CCASFLAGS = $(CFLAGS) $(INCLUDES)

LDFLAGS = -mno-crt0 -nostartfiles
	
noinst_PROGRAMS= xrPrx.elf

xrPrx_elf_SOURCES = main.c exception_asm.S xrPrx.h exports.exp

TARGET = xrPrx

PSP_FW_VERSION = 635
BUILD_PRX=1

# Use the kernel's small inbuilt libc
USE_KERNEL_LIBC = 1
# Use only kernel libraries
USE_KERNEL_LIBS = 1

PRX_EXPORTS = exports.exp

PSPSDK=$(shell psp-config --pspsdk-path)
libdir = $(top_srcdir)/lib
LIBS=-L$(libdir) -lpspkernel -lpspsystemctrl_kernel

include $(PSPSDK)/lib/build.mak

all: xrPrx.S
xrPrx.S: exports.exp
	psp-build-exports -s $^

clean-local:
	$(RM) xrPrx.prx xrPrx.S
