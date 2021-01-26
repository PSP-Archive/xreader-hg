dnl
dnl AC_XREADER_VERSION()
dnl
dnl Determine the xReader package version.

AC_DEFUN([AC_XREADER_VERSION],
[
  AC_BEFORE([$0], [AM_INIT_AUTOMAKE])

  AC_MSG_CHECKING([for xReader version])
  AS_IF([test -r "${srcdir}/aclocal/version.m4"],
        [],
        [AC_MSG_ERROR([Unable to find aclocal/version.m4])])
  AS_IF([test -r "${srcdir}/VERSION"],
        [],
	[AC_MSG_ERROR([Unable to find VERSION])])
  xReader_version=`cat "${srcdir}/VERSION"`
  AC_MSG_RESULT($xReader_version)
])

dnl
dnl AC_XREADER_VERSIONNUM()
dnl
dnl Determine the xReader package version number.

AC_DEFUN([AC_XREADER_VERSIONNUM],
[
  AC_BEFORE([$0], [AM_INIT_AUTOMAKE])

  AC_MSG_CHECKING([for xReader version])
  AS_IF([test -r "${srcdir}/aclocal/version.m4"],
        [],
        [AC_MSG_ERROR([Unable to find aclocal/version.m4])])
  AS_IF([test -r "${srcdir}/VERNUM"],
        [],
	[AC_MSG_ERROR([Unable to find version number])])
  xReader_version_number=`cat "${srcdir}/VERNUM"`
  AC_MSG_RESULT($xReader_version_number)
])

