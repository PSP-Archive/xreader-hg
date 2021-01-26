ACLOCAL_AMFLAGS = -I aclocal
ACLOCAL_FILES = aclocal/version.m4 aclocal/pspdev.m4 aclocal/ac_doxygen.m4

SUBDIRS = xrPrx xr_rdriver cooleyesBridge src 

include aminclude.am

MOSTLYCLEANFILES = $(DX_CLEANFILES)

EXTRA_DIST =			\
	$(ACLOCAL_FILES)	\
	LICENSE			\
	VERSION			\
	VERNUM \
	Readme.txt \
	Changelog.txt \
	msg \
	src/include \
	Revision.py \
	lib \
	resource \
	scripts \
	$(DX_CONFIG)

dist-hook:
	rm -rf `find $(distdir)/* -name .svn -o -name CVS -o -name .git`
