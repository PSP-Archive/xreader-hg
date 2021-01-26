CC = @PSP_CC@
CXX = @PSP_CXX@
CCAS = $(CC)
AR = @PSP_AR@
RANLIB = @PSP_RANLIB@
CFLAGS = 

EXEEXT =
OBJEXT = o

xrdir = $(top_srcdir)/src

AM_CPPFLAGS = \
-I$(srcdir) -I$(xrdir) -I$(xrdir)/include -I$(xrdir)/include/freetype2 \
-I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/xrPrx -I$(top_srcdir)/xr_rdriver \
-I$(PSPSDK)/include

if MUSIC
AM_CPPFLAGS += -I $(xrdir)/include/mac
endif

if OPTIMIZE
AM_CFLAGS = -O3 -G0 -fomit-frame-pointer -ffast-math -frename-registers -Wall
AM_CFLAGS += \
-funsigned-char -ffast-math -fforce-addr -fmerge-all-constants \
-floop-optimize -funsafe-loop-optimizations -ftree-loop-linear
else
AM_CFLAGS = -O0 -G0 -g -Wall
endif
AM_CXXFLAGS = $(AM_CFLAGS) -fno-exceptions -fno-rtti
CCASFLAGS = $(CFLAGS) $(INCLUDES)

noinst_PROGRAMS = xTest.elf

xTest_elf_SOURCES = \
dmalloc_test.c \
bookmark_test.c bookmark_test.h \
rar_speed_test.c rar_speed_test.h hprm_test.h hprm_test.c \
music_test.c jpeg_speed_test.c jpeg_speed_test.h \
main.c \
$(xrdir)/array.c $(xrdir)/array.h $(xrdir)/bg.c $(xrdir)/bg.h $(xrdir)/bookmark.c $(xrdir)/bookmark.h \
$(xrdir)/buffer.c $(xrdir)/buffer.h $(xrdir)/charsets.c $(xrdir)/charsets.h $(xrdir)/conf.c $(xrdir)/conf.h $(xrdir)/copy.c \
$(xrdir)/copy.h $(xrdir)/ctrl.c $(xrdir)/ctrl.h $(xrdir)/dbg.c $(xrdir)/dbg.h $(xrdir)/dictionary.c $(xrdir)/dictionary.h \
$(xrdir)/display.c $(xrdir)/display.h $(xrdir)/fat.c $(xrdir)/fat.h $(xrdir)/fs.c $(xrdir)/fs.h $(xrdir)/html.c $(xrdir)/html.h $(xrdir)/image.c \
$(xrdir)/image.h $(xrdir)/iniparser.c $(xrdir)/iniparser.h $(xrdir)/location.c $(xrdir)/location.h \
$(xrdir)/pspscreen.h $(xrdir)/scene.c $(xrdir)/scene.h $(xrdir)/scene_image.c $(xrdir)/scene_impl.h \
$(xrdir)/scene_music.c $(xrdir)/scene_text.c $(xrdir)/strsafe.c $(xrdir)/strsafe.h $(xrdir)/text.c $(xrdir)/text.h \
$(xrdir)/ttfont.c $(xrdir)/ttfont.h $(xrdir)/usb.c $(xrdir)/usb.h $(xrdir)/version.h $(xrdir)/win.c $(xrdir)/win.h $(xrdir)/common/datatype.h \
$(xrdir)/common/psp_utils.c $(xrdir)/common/psp_utils.h $(xrdir)/common/qsort.c $(xrdir)/common/qsort.h \
$(xrdir)/common/utils.c $(xrdir)/common/utils.h $(xrdir)/apetaglib/APETag.c $(xrdir)/apetaglib/APETag.h \
$(xrdir)/simple_gettext.h $(xrdir)/osk.c \
$(xrdir)/osk.h $(xrdir)/exception.c $(xrdir)/exception.h $(xrdir)/m33boot.c $(xrdir)/m33boot.h $(xrdir)/passwdmgr.c $(xrdir)/passwdmgr.h \
$(xrdir)/archive.c $(xrdir)/archive.h \
$(xrdir)/depdb.c $(xrdir)/depdb.h $(xrdir)/unumd.c $(xrdir)/unumd.h $(xrdir)/ssv.c $(xrdir)/ssv.h $(xrdir)/crc16.c $(xrdir)/crc16.h \
$(xrdir)/power.c $(xrdir)/power.h $(xrdir)/freq_lock.c $(xrdir)/freq_lock.h \
$(xrdir)/xrPrx.S $(xrdir)/xr_RDLib.S $(xrdir)/sceAsfParser.S $(xrdir)/image_queue.h $(xrdir)/image_queue_server.c

xTest_elf_SOURCES += \
$(xrdir)/tta/filter.h $(xrdir)/tta/ttadec.h $(xrdir)/tta/ttadec.c $(xrdir)/tta/ttalib.h

if MUSIC
xTest_elf_SOURCES += \
$(xrdir)/musicdrv.c $(xrdir)/musicdrv.h $(xrdir)/musicmgr.c $(xrdir)/musicmgr.h $(xrdir)/xmp3audiolib.c $(xrdir)/xmp3audiolib.h \
$(xrdir)/genericplayer.c $(xrdir)/genericplayer.h $(xrdir)/buffered_reader.c $(xrdir)/buffered_reader.h $(xrdir)/mp3info.h $(xrdir)/mp3info.c $(xrdir)/mp3info_buffered.c $(xrdir)/lyric.c $(xrdir)/lyric.h $(xrdir)/clock.c \
$(xrdir)/musicinfo.c $(xrdir)/musicinfo.h $(xrdir)/mediaengine.c $(xrdir)/medaiengine.h

if MP3
xTest_elf_SOURCES += \
$(xrdir)/mp3player.c $(xrdir)/mp3player.h
endif

if MPC
xTest_elf_SOURCES += \
$(xrdir)/mpcplayer.c $(xrdir)/mpcplayer.h
endif

if OGG
xTest_elf_SOURCES += \
$(xrdir)/oggplayer.c $(xrdir)/oggplayer.h
endif

if WMA
xTest_elf_SOURCES += \
$(xrdir)/wmaplayer.c $(xrdir)/wmaplayer.h $(xrdir)/sceAsfParser.S $(top_builddir)/cooleyesBridge/cooleyesBridge.S
endif

if WAV
xTest_elf_SOURCES += \
$(xrdir)/wavplayer.c $(xrdir)/wavplayer.h
endif

if FLAC
xTest_elf_SOURCES += \
$(xrdir)/flacplayer.c $(xrdir)/flacplayer.h
endif

if TTA
xTest_elf_SOURCES += \
$(xrdir)/ttaplayer.c $(xrdir)/ttaplayer.h \
$(xrdir)/tta/filter.h $(xrdir)/tta/ttadec.h $(xrdir)/tta/ttadec.c $(xrdir)/tta/ttalib.h
endif

if WAVPACK
xTest_elf_SOURCES += \
$(xrdir)/wvplayer.c $(xrdir)/wvplayer.h
endif

if APE
xTest_elf_SOURCES += \
$(xrdir)/apeplayer.cpp $(xrdir)/apeplayer.h
endif

if AT3 
xTest_elf_SOURCES += \
$(xrdir)/at3player.c $(xrdir)/at3player.h
endif

if AA3 
xTest_elf_SOURCES += \
$(xrdir)/aa3player.c $(xrdir)/aa3player.h
endif

if M4A
xTest_elf_SOURCES += \
$(xrdir)/m4aplayer.c $(xrdir)/m4aplayer.h
endif

if AAC
xTest_elf_SOURCES += \
$(xrdir)/aacplayer.c $(xrdir)/aacplayer.h
endif

endif

if NLS
xTest_elf_SOURCES += $(xrdir)/simple_gettext.c $(xrdir)/sofile.c $(xrdir)/sofile.h $(xrdir)/hash.c $(xrdir)/hash.h
endif


xrPrx.o: $(top_builddir)/xrPrx/xrPrx.S
	$(CC) -I$(PSPSDK)/include -c -o $@ $^

xr_RDLib.o: $(top_builddir)/xr_rdriver/xr_RDLib.S
	$(CC) -I$(PSPSDK)/include -c -o $@ $^

clean-local:
	$(RM) -rf __SCE__xTest %__SCE__xTest xTest xTest% xTest.prx \
		PARAM.SFO EBOOT.PBP

PSP_FW_VERSION = 401
BUILD_PRX = 1
PSP_LARGE_MEMORY = 1

libdir = $(top_srcdir)/lib
if MUSIC
if MPC
LIBS += $(libdir)/libmpcdec.a
endif

if MP3
LIBS += $(libdir)/libmad.a
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
endif
endif
LIBS += \
-L$(libdir) $(libdir)/unrar.a $(libdir)/unzip.a $(libdir)/libchm.a \
$(libdir)/libpng.a $(libdir)/libgif.a $(libdir)/libjpeg.a $(libdir)/libbmp.a \
$(libdir)/libtga.a $(libdir)/libmad.a $(libdir)/libz.a \
$(libdir)/libfreetype.a -lexif -lm -lpspkubridge -lpspsystemctrl_user \
-lpspaudio -lpspaudiolib -lpspgu -lpspgum -lpsphprm -lpsppower -lpsprtc -lpspusb \
-lpspusbstor -lstdc++ -lpspaudio -lpspaudiocodec -lpspaudio -lpspmpeg -lpsputility 

LIBS += -ldmallocth

TARGET = xTest
EXTRA_TARGETS = EBOOT.PBP
EXTRA_CLEAN = -r __SCE__xTest %__SCE__xTest xTest xTest% xTest.prx
PSP_EBOOT_TITLE = xTest
PSP_EBOOT_ICON = $(top_srcdir)/resource/ICON0.png

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
