ACLOCAL_AMFLAGS=-I m4

changelogs =				\
	ChangeLog			\
	ChangeLog.pre-1-4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 				\
	evolution-calendar.pc			\
	evolution-mail.pc			\
	evolution-plugin.pc			\
	evolution-shell.pc

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
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	evolution-zip.in	\
	gnome-doc-utils.make    \
	gtk-doc.make		\
	$(pkgconfig_DATA:.pc=.pc.in)

if ENABLE_SMIME
SMIME_SUBDIR=smime
endif

if WITH_HELP
HELP_SUBDIR=help
endif

SUBDIRS = 			\
	m4			\
	data			\
	smclient		\
	e-util			\
	a11y			\
	filter			\
	widgets			\
	shell			\
	$(SMIME_SUBDIR)		\
	em-format		\
	addressbook		\
	composer		\
	mail			\
	calendar		\
	capplet			\
	art			\
	plugins			\
	modules			\
	doc			\
	ui			\
	views			\
	po			\
	sounds			\
	$(HELP_SUBDIR)

DISTCLEANFILES = 			\
	$(builddir)/iconv-detect.h

MAINTAINERCLEANFILES =			\
	$(srcdir)/INSTALL		\
	$(srcdir)/aclocal.m4		\
	$(srcdir)/autoscan.log		\
	$(srcdir)/compile		\
	$(srcdir)/config.guess		\
	$(srcdir)/config.h.in		\
	$(srcdir)/config.sub		\
	$(srcdir)/configure.scan	\
	$(srcdir)/depcomp		\
	$(srcdir)/install-sh		\
	$(srcdir)/ltmain.sh		\
	$(srcdir)/missing		\
	$(srcdir)/mkinstalldirs		\
	$(srcdir)/gnome-doc-utils.make	\
	$(srcdir)/gtk-doc.make		\
	$(srcdir)/m4/gnome-doc-utils.m4	\
	$(srcdir)/m4/gtk-doc.m4		\
	$(srcdir)/m4/intltool.m4	\
	$(srcdir)/m4/libtool.m4		\
	$(srcdir)/m4/ltoptions.m4	\
	$(srcdir)/m4/ltsugar.m4		\
	$(srcdir)/m4/ltversion.m4	\
	$(srcdir)/m4/lt~obsolete.m4

# Scrollkeeper must be disabled for distcheck to work because
# distcheck uses a custom prefix.  See GNOME bug #354357.
DISTCHECK_CONFIGURE_FLAGS =		\
	--enable-gtk-doc		\
	--disable-scrollkeeper		\
	--enable-test-component		\
	--with-help

-include $(top_srcdir)/git.mk
