#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([xReader],[1.2.0],[])
m4_include([aclocal/ac_doxygen.m4])
m4_include([aclocal/pspdev.m4])
m4_include([aclocal/version.m4])
AC_XREADER_VERSION
AC_XREADER_VERSIONNUM
AC_CONFIG_HEADERS([config.h])
AC_PSPDEV_PATH

CONFIGURE_CMDLINE=$@
AC_SUBST(CONFIGURE_CMDLINE)

${srcdir}/Revision.py ${srcdir}

AM_INIT_AUTOMAKE([foreign no-exeext])
AM_INIT_AUTOMAKE([xReader], [$xReader_version])

# Enable/Disable options

AC_ARG_ENABLE(debug, [  --enable-debug   Enable debug option	default is disabled],
[debug="$enableval"], debug=no)
AC_ARG_ENABLE(dmalloc, [  --enable-dmalloc   Enable dmalloc option	default is disabled],
[dmalloc="$enableval"], dmalloc=no)
AC_ARG_ENABLE(optimize, [  --enable-optimize   Enable compiler optimize option	default is enabled],
[optimize="$enableval"], optimize=yes)
AC_ARG_ENABLE(homebrew_2xx, [  --enable-homebrew_2xx   Enable PSP firmware 2.0+ support	default is enabled],
homebrew_2xx="$enableval", homebrew_2xx=yes)
AC_ARG_ENABLE(image, [  --enable-image   Enable image option	default is enabled],
image="$enableval", image=yes)
AC_ARG_ENABLE(bg, [  --enable-bg   Enable background image option	default is enabled],
bg="$enableval", bg=yes)
AC_ARG_ENABLE(usb, [  --enable-usb   Enable USB option	default is enabled],
usb="$enableval", usb=yes)
AC_ARG_ENABLE(hprm, [  --enable-hprm   Enable hprm option	default is enabled],
hprm="$enableval", hprm=yes)
AC_ARG_ENABLE(analog, [  --enable-analog   Enable analog input option	default is enabled],
analog="$enableval", analog=yes)
AC_ARG_ENABLE(music, [  --enable-music   Enable music option	default is enabled],
music="$enableval", music=yes)
AC_ARG_ENABLE(mp3, [  --enable-mp3   Enable mp3 option	default is enabled when music is enabled],
mp3="$enableval", mp3=$music)
AC_ARG_ENABLE(mpc, [  --enable-mpc   Enable mpc option	default is enabled when music is enabled],
mpc="$enableval", mpc=$music)
AC_ARG_ENABLE(ogg, [  --enable-ogg   Enable ogg option	default is enabled when music is enabled],
ogg="$enableval", ogg=$music)
AC_ARG_ENABLE(wma, [  --enable-wma   Enable wma option	default is enabled when music is enabled],
wma="$enableval", wma=$music)
AC_ARG_ENABLE(wav, [  --enable-wav   Enable wav option	default is enabled when music is enabled],
wav="$enableval", wav=$music)
AC_ARG_ENABLE(ape, [  --enable-ape   Enable ape option	default is enabled when music is enabled],
ape="$enableval", ape=$music)
AC_ARG_ENABLE(tta, [  --enable-tta   Enable tta option	default is enabled when music is enabled],
tta="$enableval", tta=$music)
AC_ARG_ENABLE(wv, [  --enable-wv   Enable wavpack option	default is enabled when music is enabled],
wv="$enableval", wv=$music)
AC_ARG_ENABLE(flac, [  --enable-flac   Enable flac option	default is enabled when music is enabled],
flac="$enableval", flac=$music)
AC_ARG_ENABLE(at3, [  --enable-at3   Enable at3 option	default is enabled when music is enabled],
at3="$enableval", at3=$music)
AC_ARG_ENABLE(aa3, [  --enable-aa3   Enable aa3 option	default is enabled when music is enabled],
aa3="$enableval", aa3=$music)
AC_ARG_ENABLE(m4a, [  --enable-m4a   Enable m4a option	default is enabled when music is enabled],
m4a="$enableval", m4a=$music)
AC_ARG_ENABLE(aac, [  --enable-aac   Enable aac option	default is enabled when music is enabled],
aac="$enableval", aac=$music)
AC_ARG_ENABLE(lyric, [  --enable-lyric   Enable lyric option	default is enabled],
lyric="$enableval", lyric=yes)
AC_ARG_ENABLE(ttf, [  --enable-ttf   Enable ttf option	default is enabled],
ttf="$enableval", ttf=yes)
AC_ARG_ENABLE(nls, [  --enable-nls   Enable internationtial option	default is enabled],
nls="$enableval", nls=yes)
AC_ARG_ENABLE(lite, [  --enable-lite   Enable lite version	default is
		disable],
lite="$enableval", lite=no)

if test $lite = "yes"; then
	AC_DEFINE([ENABLE_LITE], 1, [Enable lite version])
	usb=no
	music=no
	wma=no
	lyric=no
	ttf=no
	image=yes
	bg=no
fi
AM_CONDITIONAL([LITE], [test x$lite = "xyes"])

if test $debug = "yes"; then
	AC_DEFINE([_DEBUG], 1, [Debug option])
	usb=no
fi
AM_CONDITIONAL([DEBUG], [test x$debug = "xyes"])
if test $dmalloc = "yes"; then
	AC_DEFINE([DMALLOC], 1, [Dmalloc option])
fi
AM_CONDITIONAL([DMALLOC], [test x$dmalloc = "xyes"])
AM_CONDITIONAL([OPTIMIZE], [test x$optimize = "xyes"])
if test $homebrew_2xx = "yes"; then
	AC_DEFINE([HOMEBREW_2XX], 1, [Enable PSP firmware 2.0+ support])
fi
if test $image = "yes"; then
	AC_DEFINE([ENABLE_IMAGE], 1, [Enable image option])
fi
if test $bg = "yes"; then
	AC_DEFINE([ENABLE_BG], 1, [Enable background image option])
fi
if test $usb = "yes"; then
	AC_DEFINE([ENABLE_USB], 1, [Enable USB option])
fi
if test $hprm = "yes"; then
	AC_DEFINE([ENABLE_HPRM], 1, [Enable hprm option])
fi
if test $analog = "yes"; then
	AC_DEFINE([ENABLE_ANALOG], 1, [Enable analog option])
fi
if test $music = "yes"; then
	AC_DEFINE([ENABLE_MUSIC], 1, [Enable music option])
fi
AM_CONDITIONAL([MUSIC], [test x$music = "xyes"])
if test $mp3 = "yes"; then
	AC_DEFINE([ENABLE_MP3], 1, [Enable mp3 option])
fi
AM_CONDITIONAL([MP3], [test x$mp3 = "xyes"])
if test $mpc = "yes"; then
	AC_DEFINE([ENABLE_MPC], 1, [Enable mpc option])
fi
AM_CONDITIONAL([MPC], [test x$mpc = "xyes"])
if test $ogg = "yes"; then
	AC_DEFINE([ENABLE_OGG], 1, [Enable ogg option])
fi
AM_CONDITIONAL([OGG], [test x$ogg = "xyes"])
if test $wma = "yes"; then
	AC_DEFINE([ENABLE_WMA], 1, [Enable wma option])
fi
AM_CONDITIONAL([WMA], [test x$wma = "xyes"])
if test $tta = "yes"; then
	AC_DEFINE([ENABLE_TTA], 1, [Enable tta option])
fi
AM_CONDITIONAL([TTA], [test x$tta = "xyes"])
if test $wav = "yes"; then
	AC_DEFINE([ENABLE_WAV], 1, [Enable wav option])
fi
AM_CONDITIONAL([WAV], [test x$wav = "xyes"])
if test $wv = "yes"; then
	AC_DEFINE([ENABLE_WAVPACK], 1, [Enable wavpack option])
fi
AM_CONDITIONAL([WAVPACK], [test x$wv = "xyes"])
if test $flac = "yes"; then
	AC_DEFINE([ENABLE_FLAC], 1, [Enable flac option])
fi
AM_CONDITIONAL([FLAC], [test x$flac = "xyes"])
if test $ape = "yes"; then
	AC_DEFINE([ENABLE_APE], 1, [Enable ape option])
fi
AM_CONDITIONAL([APE], [test x$ape = "xyes"])
if test $at3 = "yes"; then
	AC_DEFINE([ENABLE_AT3], 1, [Enable at3 option])
fi
AM_CONDITIONAL([AT3], [test x$at3 = "xyes"])
if test $aa3 = "yes"; then
	AC_DEFINE([ENABLE_AA3], 1, [Enable aa3 option])
fi
AM_CONDITIONAL([AA3], [test x$aa3 = "xyes"])
if test $m4a = "yes"; then
	AC_DEFINE([ENABLE_M4A], 1, [Enable m4a option])
fi
AM_CONDITIONAL([M4A], [test x$m4a = "xyes"])
if test $aac = "yes"; then
	AC_DEFINE([ENABLE_AAC], 1, [Enable aac option])
fi
AM_CONDITIONAL([AAC], [test x$aac = "xyes"])
if test $lyric = "yes"; then
	AC_DEFINE([ENABLE_LYRIC], 1, [Enable lyric option])
fi
if test $ttf = "yes"; then
	AC_DEFINE([ENABLE_TTF], 1, [Enable ttf option])
fi
if test $nls = "yes"; then
	AC_DEFINE([ENABLE_NLS], 1, [Enable internationtial option])
fi
AM_CONDITIONAL([NLS], [test x$nls = "xyes"])

AH_TOP([#ifndef _XREADER_CONFIG_H
#define _XREADER_CONFIG_H])
AH_BOTTOM([#endif])

AC_DEFINE_UNQUOTED([XREADER_VERSION_NUM], [$xReader_version_number], [xReader version number])

# Checks for programs.
AC_PSPDEV_TOOLCHAIN
AC_PROG_CC
# These will actually be overridden by the psp-specific counterparts.  They are
# here mainly to shut automake up.
AC_PROG_CXX
AM_PROG_AS
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
# AC_HEADER_STDC
# AC_CHECK_HEADERS([fcntl.h malloc.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
# AC_C_CONST
# AC_C_BIGENDIAN

# Checks for library functions.
# AC_FUNC_MALLOC
# AC_FUNC_MEMCMP
# AC_CHECK_FUNCS([getcwd strchr])

# Doxygen.
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(xReader, doxygen.cfg, doc)

# CFLAGS and CXXFLAGS used to build pspsdk libraries.
CFLAGS=""
PSPSDK_CFLAGS="$CFLAGS -G0 -Wall"
PSPSDK_CXXFLAGS="$PSPSDK_CFLAGS -fno-exceptions -fno-rtti"
AC_SUBST([xReader_version])
AC_SUBST(PSPSDK_CFLAGS)
AC_SUBST(PSPSDK_CXXFLAGS)

# Turn on all warnings (for host programs).
if test x$ac_compiler_gnu = xyes; then
   CFLAGS="$CFLAGS -Wall"
fi

#AC_CONFIG_SUBDIRS([src/GenIndex])
AC_CONFIG_FILES([Makefile
                 cooleyesBridge/Makefile
                 src/Makefile
                 xrPrx/Makefile
                 xr_rdriver/Makefile
                 testsuite/Makefile
                 conf_cmdline.h
                 ])
AC_CONFIG_FILES([scripts/t.sh], [chmod +x scripts/t.sh])
AC_CONFIG_FILES([scripts/rel.sh], [chmod +x scripts/rel.sh])
AC_OUTPUT
