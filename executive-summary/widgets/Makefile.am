INCLUDES = 	\
	-I$(top_srcdir) 	\
	-I$(includedir)		\
	$(GNOME_INCLUDEDIR)	\
	-DG_LOG_DOMAIN=\"Executive-Summary-Widgets\"	\
	-g 	\
	-Wall	\
	-Wmissing-prototypes	\
	-Wmissing-declarations

noinst_LIBRARIES = libesummary-widgets.a

libesummary_widgets_a_SOURCES = 	\
	e-summary-subwindow.c		\
	e-summary-subwindow.h		\
	e-summary-titlebar.c		\
	e-summary-titlebar.h		\
	e-summary-title-button.c	\
	e-summary-title-button.h

noinst_PROGRAMS =		\
	esummary-window-test

esummary_window_test_SOURCES =	\
	esummary-window-test.c

esummary_window_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	libesummary-widgets.a	\
	$(BONOBO_GNOME_LIBS)	\
	$(top_builddir)/e-util/libeutil.la

