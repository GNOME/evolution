INCLUDES =					\
	$(EXTRA_GNOME_CFLAGS)			\
	$(GNOME_INCLUDEDIR)			\
	-I$(top_srcdir)/widgets/e-text		\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)/widgets/e-table		\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets/e-minicard

noinst_PROGRAMS =	\
	evolution-addressbook

evolution_addressbook_SOURCES = 	\
	demo.c

evolution_addressbook_LDADD =				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/widgets/e-minicard/libeminicard.a	\
	$(top_builddir)/widgets/e-table/libetable.a	\
	$(top_builddir)/widgets/e-text/libetext.a	\
	$(top_builddir)/e-util/libeutil.la

evolution_addressbook_LDFLAGS = `gnome-config --libs gdk_pixbuf`
