INCLUDES = -I$(top_srcdir)/include

common_sources = ../common/crc32.c \
		../common/huffman-bcl.c

METASOURCES = AUTO
lib_LTLIBRARIES = libmpcdec.la
libmpcdec_la_SOURCES = huffman.c mpc_decoder.c mpc_reader.c \
	requant.c	streaminfo.c synth_filter.c mpc_bits_reader.c mpc_demux.c \
	$(common_sources)

libmpcdec_la_LDFLAGS = -no-undefined -version-info 7:0:1

noinst_HEADERS = mpc_bits_reader.h huffman.h decoder.h internal.h requant.h mpcdec_math.h
AM_CFLAGS = -fpic -fvisibility=hidden
