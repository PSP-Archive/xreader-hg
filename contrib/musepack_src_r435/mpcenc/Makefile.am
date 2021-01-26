bin_PROGRAMS = mpcenc

common_sources = ../common/crc32.c \
		../common/huffman-bcl.c \
		../common/fastmath.c \
		../common/tags.c

# set the include path found by configure
INCLUDES = -I$(top_srcdir)/libmpcenc -I$(top_srcdir)/libmpcpsy \
	-I$(top_srcdir)/include/ $(all_includes)

# the library search path.
mpcenc_LDFLAGS = $(all_libraries)
mpcenc_SOURCES = keyboard.c mpcenc.c pipeopen.c stderr.c wave_in.c winmsg.c \
	$(common_sources)
mpcenc_LDADD = -lm \
	$(EXTRALIBS) \
	$(top_builddir)/libmpcpsy/libmpcpsy.a \
	$(top_builddir)/libmpcenc/libmpcenc.a
AM_CFLAGS = -DTRUE=1 -DFALSE=0

noinst_HEADERS = mpcenc.h predict.h config.h
