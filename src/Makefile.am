CC = @PSP_CC@
CXX = @PSP_CXX@
CCAS = $(CC)
AR = @PSP_AR@
RANLIB = @PSP_RANLIB@
CFLAGS = 
CXXFLAGS = 

EXEEXT =
OBJEXT = o

AM_CPPFLAGS = \
-I$(srcdir) -I$(srcdir)/include -I$(srcdir)/include/freetype2 \
-I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/xrPrx -I$(top_srcdir)/xr_rdriver \
-I$(PSPSDK)/include

if APE
AM_CPPFLAGS += -I $(srcdir)/include/mac
endif

if OPTIMIZE
AM_CFLAGS = -Os -G0 -Wall
AM_CXXFLAGS = -Os -G0 -Wall -fno-exceptions -fno-rtti
else
AM_CFLAGS = -O0 -G0 -g -Wall
AM_CXXFLAGS = -O0 -G0 -g -Wall -fno-exceptions -fno-rtti
endif

AM_CFLAGS += -Werror -Wdeclaration-after-statement
AM_CXXFLAGS += -Werror
CCASFLAGS = $(CFLAGS) $(INCLUDES)

noinst_PROGRAMS = xReader.elf

xReader_elf_SOURCES = \
	bg.c \
	bg.h \
	bookmark.c \
	bookmark.h \
	buffer.c \
	buffer.h \
	charsets.c \
	charsets.h \
	conf.c \
	conf.h \
	copy.c \
	copy.h \
	ctrl.c \
	ctrl.h \
	dbg.c \
	dbg.h \
	dictionary.c \
	dictionary.h \
	display.c \
	display.h \
	fat.c \
	fat.h \
	fs.c \
	fs.h \
	html.c \
	html.h \
	image.c \
	image.h \
	iniparser.c \
	iniparser.h \
	location.c \
	location.h \
	pspscreen.h \
	scene.c \
	scene.h \
	scene_image.c \
	scene_impl.h \
	scene_music.c \
	scene_text.c \
	strsafe.c \
	strsafe.h \
	text.c \
	text.h \
	ttfont.c \
	ttfont.h \
	usb.c \
	usb.h \
	version.h \
	win.c \
	win.h \
	common/datatype.h \
	common/psp_utils.c \
	common/psp_utils.h \
	common/qsort.c \
	common/qsort.h \
	common/utils.c \
	common/utils.h \
	simple_gettext.h \
	osk.c \
	osk.h \
	exception.c \
	exception.h \
	m33boot.c \
	m33boot.h \
	passwdmgr.c \
	passwdmgr.h \
	rc4.c \
	rc4.h \
	archive.c \
	archive.h \
	xrPrx.S \
	xr_RDLib.S \
	depdb.c \
	depdb.h \
	unumd.c \
	unumd.h \
	ssv.c \
	ssv.h \
	main.c \
	power.c \
	power.h \
	freq_lock.c \
	freq_lock.h \
	image_queue.h \
	image_queue_server.c \
	fontconfig.c \
	fontconfig.h \
	thread_lock.c

xrPrx.o: $(top_builddir)/xrPrx/xrPrx.S
	$(CC) -I$(PSPSDK)/include -c -o $@ $^

xr_RDLib.o: $(top_builddir)/xr_rdriver/xr_RDLib.S
	$(CC) -I$(PSPSDK)/include -c -o $@ $^

if MUSIC
xReader_elf_SOURCES += \
	audiocore/musicdrv.c \
	audiocore/musicdrv.h \
	audiocore/musicmgr.c \
	audiocore/musicmgr.h \
	audiocore/xaudiolib.c \
	audiocore/xaudiolib.h \
	audiocore/genericplayer.c \
	audiocore/genericplayer.h \
	audiocore/buffered_reader.c \
	audiocore/buffered_reader.h \
	audiocore/mp3info.h \
	audiocore/mp3info.c \
	audiocore/musicinfo.c \
	audiocore/musicinfo.h \
	audiocore/mediaengine.c \
	audiocore/medaiengine.h \
	audiocore/scevaudio.S \
	audiocore/lyric.c \
	audiocore/lyric.h \
	audiocore/apetaglib/APETag.c \
	audiocore/apetaglib/APETag.h \
	audiocore/musiclist.c \
	audiocore/musiclist.h \
	audiocore/stack.h \
	audiocore/stack.c \
	clock.c

if MP3
xReader_elf_SOURCES += \
	audiocore/mp3player.c \
	audiocore/mp3player.h
endif

if MPC
xReader_elf_SOURCES += \
	audiocore/mpcplayer.c \
	audiocore/mpcplayer.h
endif

if OGG
xReader_elf_SOURCES += \
	audiocore/oggplayer.c \
	audiocore/oggplayer.h
endif

if WMA
xReader_elf_SOURCES += \
	audiocore/wmaplayer.c \
	audiocore/wmaplayer.h \
	$(top_builddir)/cooleyesBridge/cooleyesBridge.S
LIBS += -lpspasfparser
endif

if WAV
xReader_elf_SOURCES += \
	audiocore/wavplayer.c \
	audiocore/wavplayer.h
endif

if FLAC
xReader_elf_SOURCES += \
	audiocore/flacplayer.c \
	audiocore/flacplayer.h
endif

if TTA
xReader_elf_SOURCES += \
	audiocore/ttaplayer.c \
	audiocore/ttaplayer.h \
	audiocore/tta/filter.h \
	audiocore/tta/ttadec.h \
	audiocore/tta/ttadec.c \
	audiocore/tta/ttalib.h
endif

if WAVPACK
xReader_elf_SOURCES += \
	audiocore/wvplayer.c \
	audiocore/wvplayer.h
endif

if APE
xReader_elf_SOURCES += \
	audiocore/apeplayer.cpp \
	audiocore/apeplayer.h
endif

if AT3 
xReader_elf_SOURCES += \
	audiocore/at3player.c \
	audiocore/at3player.h
endif

if AA3 
xReader_elf_SOURCES += \
	audiocore/aa3player.c \
	audiocore/aa3player.h
endif

if M4A
xReader_elf_SOURCES += \
	audiocore/m4aplayer.c \
	audiocore/m4aplayer.h
endif

if AAC
xReader_elf_SOURCES += \
	audiocore/aacplayer.c \
	audiocore/aacplayer.h
endif

endif

if NLS
xReader_elf_SOURCES += \
	simple_gettext.c \
	sofile.c \
	sofile.h \
	hash.c \
	hash.h
endif

clean-local:
	$(RM) -rf __SCE__xReader %__SCE__xReader xReader xReader% xReader.prx \
		PARAM.SFO EBOOT.PBP

PSP_FW_VERSION = 635
BUILD_PRX = 1
PSP_LARGE_MEMORY = 1
PRX_EXPORTS = exports.exp

libdir = $(top_srcdir)/lib
if MUSIC

if MPC
LIBS += $(libdir)/libmpcdec.a
endif

if MP3
endif

if OGG
LIBS += $(libdir)/libvorbisidec.a
endif

if WMA
endif

if APE
LIBS += -lmac
endif

if FLAC
LIBS += $(libdir)/libFLAC.a
endif

if WAVPACK
LIBS += $(libdir)/libwavpack.a
endif

if M4A
LIBS += $(libdir)/libmp4v2.a $(libdir)/libfaad2.a
else 
if AAC
LIBS += $(libdir)/libmp4v2.a $(libdir)/libfaad2.a
endif
endif

endif

LIBS += \
	-L$(libdir) \
	$(libdir)/unrar.a \
	$(libdir)/unzip.a \
	$(libdir)/libchm.a \
	$(libdir)/libpng.a \
	$(libdir)/libgif.a \
	$(libdir)/libjpeg.a \
	$(libdir)/libbmp.a \
	$(libdir)/libtga.a \
	$(libdir)/libz.a \
	$(libdir)/libfreetype.a \
	-lm \
	-lpspkubridge \
	-lpspsystemctrl_user \
	-lpspaudio \
	-lpspaudiolib \
	-lpspgu \
	-lpspgum \
	-lpsphprm \
	-lpsppower \
	-lpsprtc \
	-lpspusb \
	-lpspusbstor \
	-lstdc++ \
	-lpspaudio \
	-lpspaudiocodec \
	-lpspaudio \
	-lpsputility

if DMALLOC
LIBS += -ldmallocth
endif

TARGET = xReader
EXTRA_TARGETS = EBOOT.PBP
EXTRA_CLEAN = -r __SCE__xReader %__SCE__xReader xReader xReader% xReader.prx

if LITE
PSP_EBOOT_TITLE = xReader lite $(xReader_version)
else
PSP_EBOOT_TITLE = xReader $(xReader_version)
endif

PSP_EBOOT_ICON = $(top_srcdir)/resource/ICON0.png

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
