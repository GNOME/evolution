INCLUDES =								\
	-I$(top_srcdir)							\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(TZDIALOG_CFLAGS)

privlib_LTLIBRARIES =	\
	libetimezonedialog.la

libetimezonedialog_la_SOURCES =	\
	e-timezone-dialog.c	\
	e-timezone-dialog.h

glade_DATA = e-timezone-dialog.glade

EXTRA_DIST = $(glade_DATA)
