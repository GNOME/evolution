INCLUDES =						\
	-DG_LOG_DOMAIN=\"wombat\"			\
	-I$(top_srcdir)/e-util				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/calendar/pcs			\
	$(GNOME_INCLUDEDIR)				\
	$(BONOBO_VFS_GNOME_CFLAGS)				\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS =	\
	wombat

wombat_SOURCES = 	\
	wombat.c

wombat_LDADD =							\
	$(EXTRA_GNOME_LIBS)					\
	$(BONOBO_VFS_GNOME_LIBS)				\
	$(top_builddir)/addressbook/backend/pas/libpas.a	\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/addressbook/ename/libename.la		\
	$(top_builddir)/calendar/pcs/libpcs.a			\
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/libical/src/libical/libical.a		\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/e-util/libeutil.la			\
	$(LDAP_LIBS)

wombat_LDFLAGS = `gnome-config --libs gnorba` 

oafdir = $(datadir)/oaf
oaf_DATA = wombat.oafinfo

EXTRA_DIST = wombat.oafinfo

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: wombat.pure

wombat.pure: wombat
	@rm -f wombat.pure
	$(PLINK) $(wombat_LDFLAGS) $(wombat_OBJECTS) $(wombat_LDADD) $(LIBS)

endif

