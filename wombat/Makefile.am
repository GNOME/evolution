INCLUDES =						\
	-DG_LOG_DOMAIN=\"wombat\"			\
	-I$(top_srcdir)/e-util				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/calendar/pcs			\
	-I$(top_builddir)/calendar/pcs			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	$(GNOME_INCLUDEDIR)				\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"

bin_PROGRAMS =	\
	wombat

wombat_SOURCES = 		\
	wombat-moniker.c	\
	wombat-moniker.h	\
	wombat.c

wombat_LDADD =							\
	$(top_builddir)/addressbook/backend/pas/libpas.a	\
	$(DB3_LDADD)						\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/e-util/ename/libename.la		\
	$(top_builddir)/calendar/pcs/libpcs.a			\
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/libical/src/libical/libical.la		\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/e-util/libeutil.la			\
	$(LDAP_LIBS)						\
	$(EXTRA_GNOME_LIBS)					\
	$(BONOBO_VFS_GNOME_LIBS)

oafdir = $(datadir)/oaf

oaf_in_files = GNOME_Evolution_Wombat.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@


EXTRA_DIST = $(oaf_DATA) $(oaf_in_files)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: wombat.pure

wombat.pure: wombat
	@rm -f wombat.pure
	$(PLINK) $(wombat_LDFLAGS) $(wombat_OBJECTS) $(wombat_LDADD) $(LIBS)

endif
