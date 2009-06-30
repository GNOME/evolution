AM_CPPFLAGS =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(TZDIALOG_CFLAGS)

privsolib_LTLIBRARIES =	\
	libetimezonedialog.la

libetimezonedialog_la_SOURCES =	\
	e-timezone-dialog.c	\
	e-timezone-dialog.h

libetimezonedialog_la_LDFLAGS = $(NO_UNDEFINED)

libetimezonedialog_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(EVOLUTION_CALENDAR_LIBS)

glade_DATA = e-timezone-dialog.glade

EXTRA_DIST = $(glade_DATA)

-include $(top_srcdir)/git.mk
