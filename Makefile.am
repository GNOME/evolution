ACLOCAL_AMFLAGS=-I m4

pkgconfigdir = $(libdir)/pkgconfig
INTERFACE_VERSION=3.0

%-$(INTERFACE_VERSION).pc: %.pc
	cat $< | grep -x -v "Requires.private: " >$@

pkgconfig_in_files =					\
	evolution-calendar-$(INTERFACE_VERSION).pc	\
	evolution-mail-$(INTERFACE_VERSION).pc		\
	evolution-shell-$(INTERFACE_VERSION).pc
pkgconfig_DATA = $(pkgconfig_in_files:.pc.in=-$(INTERFACE_VERSION).pc)

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_in_files = evolution.appdata.xml.in
appdata_files = $(appdata_in_files:.xml.in=.xml)
dist_appdata_DATA = $(appdata_files)

EXTRA_DIST = 			\
	$(pkgconfig_DATA)	\
	$(appdata_in_files)	\
	AUTHORS			\
	COPYING-DOCS		\
	COPYING-DOCS.CCBYSA	\
	COPYING-DOCS.GFDL	\
	COPYING.LGPL2		\
	COPYING.LGPL3		\
	COPYING.OPENLDAP	\
	README			\
	HACKING			\
	MAINTAINERS		\
	NEWS			\
	NEWS-1.0		\
	enumtypes.c.template	\
	enumtypes.h.template	\
	marshal.mk		\
	iconv-detect.c		\
	plugin.mk		\
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	gtk-doc.make		\
	tests

if ENABLE_SMIME
SMIME_SUBDIR=smime
endif

if WITH_HELP
HELP_SUBDIR=help
endif

# For maintainers only.
if GLADE_CATALOG
MAINT_SUBDIR=maint
endif

SUBDIRS = 			\
	m4			\
	data			\
	libgnomecanvas		\
	e-util			\
	libemail-engine		\
	shell			\
	$(SMIME_SUBDIR)		\
	em-format		\
	addressbook		\
	composer		\
	mail			\
	calendar		\
	art			\
	plugins			\
	modules			\
	$(MAINT_SUBDIR)		\
	doc			\
	ui			\
	views			\
	po			\
	sounds			\
	$(HELP_SUBDIR)

@GNOME_CODE_COVERAGE_RULES@

DISTCLEANFILES = 			\
	$(builddir)/iconv-detect.h	\
	$(pkgconfig_in_files)		\
	$(appdata_files)

MAINTAINERCLEANFILES =			\
	$(srcdir)/ABOUT-NLS		\
	$(srcdir)/INSTALL		\
	$(srcdir)/aclocal.m4		\
	$(srcdir)/autoscan.log		\
	$(srcdir)/compile		\
	$(srcdir)/config.guess		\
	$(srcdir)/config.h.in		\
	$(srcdir)/config.rpath		\
	$(srcdir)/config.sub		\
	$(srcdir)/configure.scan	\
	$(srcdir)/depcomp		\
	$(srcdir)/install-sh		\
	$(srcdir)/ltmain.sh		\
	$(srcdir)/missing		\
	$(srcdir)/mkinstalldirs		\
	$(srcdir)/gtk-doc.make		\
	$(srcdir)/m4/codeset.m4		\
	$(srcdir)/m4/gettext.m4		\
	$(srcdir)/m4/glibc2.m4		\
	$(srcdir)/m4/glibc21.m4		\
	$(srcdir)/m4/gtk-doc.m4		\
	$(srcdir)/m4/iconv.m4		\
	$(srcdir)/m4/intdiv0.m4		\
	$(srcdir)/m4/intl.m4		\
	$(srcdir)/m4/intldir.m4		\
	$(srcdir)/m4/intlmacosx.m4	\
	$(srcdir)/m4/intltool.m4	\
	$(srcdir)/m4/intmax.m4		\
	$(srcdir)/m4/inttypes-pri.m4	\
	$(srcdir)/m4/inttypes_h.m4	\
	$(srcdir)/m4/lcmessage.m4	\
	$(srcdir)/m4/lib-ld.m4		\
	$(srcdir)/m4/lib-link.m4	\
	$(srcdir)/m4/lib-prefix.m4	\
	$(srcdir)/m4/libtool.m4		\
	$(srcdir)/m4/lock.m4		\
	$(srcdir)/m4/longlong.m4	\
	$(srcdir)/m4/ltoptions.m4	\
	$(srcdir)/m4/ltsugar.m4		\
	$(srcdir)/m4/ltversion.m4	\
	$(srcdir)/m4/lt~obsolete.m4	\
	$(srcdir)/m4/nls.m4		\
	$(srcdir)/m4/po.m4		\
	$(srcdir)/m4/printf-posix.m4	\
	$(srcdir)/m4/progtest.m4	\
	$(srcdir)/m4/size_max.m4	\
	$(srcdir)/m4/stdint_h.m4	\
	$(srcdir)/m4/uintmax_t.m4	\
	$(srcdir)/m4/visibility.m4	\
	$(srcdir)/m4/wchar_t.m4		\
	$(srcdir)/m4/wint_t.m4		\
	$(srcdir)/m4/xsize.m4		\
	$(srcdir)/po/Makefile.in.in~	\
	$(srcdir)/po/Makevars.template	\
	$(srcdir)/po/Rules-quot		\
	$(srcdir)/po/boldquot.sed	\
	$(srcdir)/po/en@boldquot.header	\
	$(srcdir)/po/en@quot.header	\
	$(srcdir)/po/insert-header.sin	\
	$(srcdir)/po/quot.sed		\
	$(srcdir)/po/remove-potcdate.sin

DISTCHECK_CONFIGURE_FLAGS =		\
	--enable-gtk-doc		\
	--disable-goa			\
	--disable-image-inline		\
	--disable-pst-import		\
	--disable-weather		\
	--with-help

@BEHAVE_INSTALLED_TESTS_RULE@
INSTALLED_TESTS=general_shortcuts mail_shortcuts contacts_shortcuts	\
	memos_shortcuts view_shortcuts addressbook_contacts
INSTALLED_TESTS_TYPE=session-exclusive

-include $(top_srcdir)/git.mk
