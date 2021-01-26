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
	
noinst_PROGRAMS= xr_rdriver.elf

xr_rdriver_elf_SOURCES = main.c export.exp

TARGET = xr_rdriver

PSP_FW_VERSION = 635
BUILD_PRX=1

# Use the kernel's small inbuilt libc
USE_KERNEL_LIBC = 1
# Use only kernel libraries
USE_KERNEL_LIBS = 1

PRX_EXPORTS = exports.exp

PSPSDK=$(shell psp-config --pspsdk-path)
LIBS=-lpspkernel -lpsploadexec_kernel -lpspsystemctrl_kernel 

include $(PSPSDK)/lib/build.mak

all: xr_RDLib.S
xr_RDLib.S: exports.exp
	psp-build-exports -s $^

clean-local:
	$(RM) xr_rdriver.prx xr_RDLib.S
