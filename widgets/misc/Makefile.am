# FIXME we use the EClippedLabel widget from EShortcutBar.  Probably
# it should be moved somewhere else.

INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets/shortcut-bar		\
	-DMAP_DIR=\""$(datadir)/images/evolution"\"	\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DG_LOG_DOMAIN=__FILE__				\
	$(GNOME_FULL_CFLAGS)

noinst_LIBRARIES =	\
	libemiscwidgets.a

libemiscwidgets_a_SOURCES = 	\
	e-bonobo-widget.c	\
	e-bonobo-widget.h	\
	e-calendar.c		\
	e-calendar.h		\
	e-calendar-item.c	\
	e-calendar-item.h	\
	e-cell-date-edit.c	\
	e-cell-date-edit.h	\
	e-cell-percent.c	\
	e-cell-percent.h	\
	e-charset-picker.c	\
	e-charset-picker.h	\
	e-clipped-label.c	\
	e-clipped-label.h	\
	e-combo-button.c	\
	e-combo-button.h	\
	e-dateedit.c		\
	e-dateedit.h		\
	e-dropdown-button.c	\
	e-dropdown-button.h	\
	e-filter-bar.c		\
	e-filter-bar.h		\
	e-map.c			\
	e-map.h			\
	e-messagebox.c		\
	e-messagebox.h		\
	e-search-bar.c		\
	e-search-bar.h		\
	e-title-bar.c		\
	e-title-bar.h		\
	e-url-entry.c		\
	e-url-entry.h

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
	$(GNOME_FULL_LIBS)

# test-dateedit

test_dateedit_SOURCES = 	\
	test-dateedit.c

test_dateedit_LDADD = 			\
	./libemiscwidgets.a		\
	../../e-util/libeutil.la	\
	$(GNOME_FULL_LIBS)

# test-dropdown-button

test_dropdown_button_SOURCES = 	\
	test-dropdown-button.c

test_dropdown_button_LDADD =	\
	./libemiscwidgets.a	\
	$(GNOME_FULL_LIBS)

# test-title-bar

test_title_bar_SOURCES = 	\
	test-title-bar.c

test_title_bar_LDADD = 		\
	./libemiscwidgets.a	\
	$(GNOME_FULL_LIBS)

