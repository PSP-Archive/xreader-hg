
METASOURCES = AUTO
bin_PROGRAMS = mpcgain
mpcgain_LDADD = -lm -lreplaygain \
	$(top_builddir)/libmpcdec/libmpcdec.la
mpcgain_SOURCES = mpcgain.c
INCLUDES = -I$(top_srcdir)/include
AM_CFLAGS = -ffast-math
