INCLUDES = -I$(top_srcdir)/include

common_sources = ../common/crc32.c

METASOURCES = AUTO
bin_PROGRAMS = mpccut
mpccut_SOURCES = mpccut.c $(common_sources)
mpccut_LDADD = $(top_builddir)/libmpcenc/libmpcenc.a \
	$(top_builddir)/libmpcdec/libmpcdec.la -lm
