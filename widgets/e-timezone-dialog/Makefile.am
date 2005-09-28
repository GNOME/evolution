if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libemiscwidgets.la
endif

INCLUDES =								\
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

libetimezonedialog_la_LIBADD =		\
	$(WIN32_BOOTSTRAP_LIBS)		\
	$(EVOLUTION_CALENDAR_LIBS)

glade_DATA = e-timezone-dialog.glade

EXTRA_DIST = $(glade_DATA)
