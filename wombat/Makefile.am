INCLUDES =						\
	$(EXTRA_GNOME_CFLAGS)				\
	$(GNOME_INCLUDEDIR)				\
	-DG_LOG_DOMAIN=\"wombat\"			\
	-I$(top_srcdir)/e-util				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend/pas		\
	-I$(top_builddir)/addressbook/backend/pas	\
	-I$(top_srcdir)/calendar			\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS =	\
	wombat

wombat_SOURCES = 	\
	wombat.c

wombat_LDADD =							\
	$(EXTRA_GNOME_LIBS)					\
	$(BONOBO_VFS_GNOME_LIBS)				\
	$(top_builddir)/addressbook/backend/pas/libpas.la	\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/calendar/pcs/libpcs.a			\
	$(top_builddir)/libical/src/libical/libical.la		\
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/e-util/libeutil.la			\
	-lgnomevfs						\
	$(LDAP_LIBS)

wombat_LDFLAGS = `gnome-config --libs gnorba` 

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = wombat.gnorba
