ACLOCAL_AMFLAGS=-I m4

changelogs =				\
	ChangeLog			\
	ChangeLog.pre-1-4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 				\
	evolution-shell.pc	\
	evolution-plugin.pc

EXTRA_DIST = 			\
	AUTHORS			\
	$(changelogs)		\
	COPYING-DOCS		\
	COPYING.LGPL2		\
	COPYING.LGPL3		\
	COPYING.OPENLDAP	\
	README			\
	HACKING			\
	MAINTAINERS		\
	NEWS			\
	NEWS-1.0		\
	marshal.mk		\
	iconv-detect.c		\
	plugin.mk		\
	server.mk		\
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	evolution-zip.in	\
	gnome-doc-utils.make    \
	$(pkgconfig_DATA:.pc=.pc.in)

if ENABLE_SMIME
SMIME_DIR=smime
endif

SUBDIRS = 			\
	win32			\
        data                    \
        e-util                  \
	a11y			\
        widgets                 \
        shell                   \
	filter			\
	$(SMIME_DIR)		\
	addressbook		\
	calendar		\
        art                     \
	composer		\
	mail			\
	plugins			\
	ui			\
	views			\
	tools			\
	po			\
	sounds

if WITH_HELP
SUBDIRS += help
endif

DISTCLEANFILES = 			\
	$(builddir)/doltcompile		\
	$(builddir)/doltlibtool		\
	$(builddir)/iconv-detect.h

MAINTAINERCLEANFILES =			\
	$(srcdir)/INSTALL		\
	$(srcdir)/aclocal.m4		\
	$(srcdir)/autoscan.log		\
	$(srcdir)/config.guess		\
	$(srcdir)/config.h.in		\
	$(srcdir)/config.sub		\
	$(srcdir)/configure.scan	\
	$(srcdir)/depcomp		\
	$(srcdir)/install-sh		\
	$(srcdir)/ltmain.sh		\
	$(srcdir)/missing		\
	$(srcdir)/mkinstalldirs		\
	$(srcdir)/gnome-doc-utils.make

# Scrollkeeper must be disabled for distcheck to work because
# distcheck uses a custom prefix.  See GNOME bug #354357.
DISTCHECK_CONFIGURE_FLAGS =		\
	--disable-scrollkeeper		\
	--enable-test-component

-include $(top_srcdir)/git.mk
