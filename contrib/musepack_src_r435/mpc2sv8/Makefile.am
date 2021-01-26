INCLUDES = -I$(top_srcdir)/include

common_sources = ../common/crc32.c

METASOURCES = AUTO
bin_PROGRAMS = mpc2sv8
mpc2sv8_SOURCES = mpc2sv8.c $(common_sources)
mpc2sv8_LDADD = -lm \
	$(top_builddir)/libmpcdec/libmpcdec.la \
	$(top_builddir)/libmpcenc/libmpcenc.a
