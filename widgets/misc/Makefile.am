# FIXME we use the EClippedLabel widget from EShortcutBar.  Probably
# it should be moved somewhere else.

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets/shortcut-bar	\
	$(EXTRA_GNOME_CFLAGS)			\
	-DG_LOG_DOMAIN=__FILE__

noinst_LIBRARIES =	\
	libemiscwidgets.a

libemiscwidgets_a_SOURCES = 	\
	e-calendar.c		\
	e-calendar.h		\
	e-calendar-item.c	\
	e-calendar-item.h	\
	e-cell-date-edit.c	\
	e-cell-date-edit.h	\
	e-clipped-label.c	\
	e-clipped-label.h	\
	e-dateedit.c		\
	e-dateedit.h		\
	e-dropdown-button.c	\
	e-dropdown-button.h	\
	e-messagebox.c		\
	e-messagebox.h		\
	e-search-bar.c		\
	e-search-bar.h		\
	e-title-bar.c		\
	e-title-bar.h

noinst_PROGRAMS = 		\
	test-calendar		\
	test-dateedit		\
	test-dropdown-button	\
	test-title-bar

# test-calendar

test_calendar_SOURCES = 	\
	test-calendar.c

test_calendar_LDADD = 						\
	./libemiscwidgets.a					\
	../../e-util/libeutil.la				\
	$(EXTRA_GNOME_LIBS)

# test-dateedit

test_dateedit_SOURCES = 	\
	test-dateedit.c

test_dateedit_LDADD = 						\
	./libemiscwidgets.a					\
	../../e-util/libeutil.la				\
	$(EXTRA_GNOME_LIBS)

# test-dropdown-button

test_dropdown_button_SOURCES = 	\
	test-dropdown-button.c

test_dropdown_button_LDADD =	\
	./libemiscwidgets.a	\
	$(EXTRA_GNOME_LIBS)

# test-title-bar

test_title_bar_SOURCES = 	\
	test-title-bar.c

test_title_bar_LDADD = 						\
	./libemiscwidgets.a					\
	$(EXTRA_GNOME_LIBS)

