INCLUDES =					\
	$(EXTRA_GNOME_CFLAGS)			\
	$(GNOME_INCLUDEDIR)			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook/backend/pas	\
	-I$(top_srcdir)/calendar/pcs		\
	-I$(top_srcdir)/calendar/cal-util \
	-DEVOLUTION_VERSION=\""$(VERSION)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	

bin_PROGRAMS =	\
	wombat

wombat_SOURCES = 	\
	wombat.c

wombat_LDADD =				\
	$(EXTRA_GNOME_LIBS)		\
	$(BONOBO_HTML_GNOME_LIBS)	\
	$(top_builddir)/e-util/libeutil.la     \
	-lpas				\
	-lpcs				\
	-lical				\
	-lcalutil			\
	$(top_builddir)/libversit/libversit.la \
	-lgnomevfs

wombat_LDFLAGS = `gnome-config --libs gnorba` 

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = wombat.gnorba
