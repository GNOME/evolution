
INCLUDES =					\
	$(GNOME_INCLUDEDIR)			\
	-I $(top_srcdir)			\
	-DG_LOG_DOMAIN=\"meeting-time-sel\"

noinst_LIBRARIES =				\
	libevolutionmtsel.a

libevolutionmtsel_a_SOURCES =			\
	e-meeting-time-sel.c			\
	e-meeting-time-sel.h			\
	e-meeting-time-sel-item.c		\
	e-meeting-time-sel-item.h		\
	e-meeting-time-sel-list-item.c		\
	e-meeting-time-sel-list-item.h

noinst_PROGRAMS =	\
	test-meeting-time-selector

test_meeting_time_selector_SOURCES = 	\
	test-meeting-time-sel.c

test_meeting_time_selector_LDADD =			\
	libevolutionmtsel.a				\
	$(top_builddir)/widgets/e-text/libetext.a	\
	$(top_builddir)/e-util/libeutil.la		\
	$(EXTRA_GNOME_LIBS)				\
	$(GNOMEUI_LIBS)

test_meeting_time_selector_LDFLAGS = $(GNOME_LIBDIR)

EXTRA_DIST =				\
	e-meeting-time-sel-mail.xpm	\
	e-meeting-time-sel-no-mail.xpm

