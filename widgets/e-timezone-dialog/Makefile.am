privsolib_LTLIBRARIES =	libetimezonedialog.la

libetimezonedialog_la_CPPFLAGS =					\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(TZDIALOG_CFLAGS)

libetimezonedialog_la_SOURCES =	\
	e-timezone-dialog.c	\
	e-timezone-dialog.h

libetimezonedialog_la_LDFLAGS = $(NO_UNDEFINED)

libetimezonedialog_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(EVOLUTION_CALENDAR_LIBS)
	$(TZDIALOG_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

ui_DATA = e-timezone-dialog.ui

EXTRA_DIST = $(ui_DATA)

-include $(top_srcdir)/git.mk
