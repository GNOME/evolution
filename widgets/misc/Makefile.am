# FIXME we use the EClippedLabel widget from EShortcutBar.  Probably
# it should be moved somewhere else.

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/widgets/shortcut-bar				\
	-I$(top_srcdir)/a11y/widgets/					\
	-DMAP_DIR=\""$(imagesdir)"\"					\
	-DG_LOG_DOMAIN=__FILE__						\
	-DG_DISABLE_DEPRECATED						\
	$(GNOME_FULL_CFLAGS)

privlib_LTLIBRARIES =	\
	libemiscwidgets.la

widgetsincludedir = $(privincludedir)/widgets

widgetsinclude_HEADERS =			\
	e-calendar.h				\
	e-calendar-item.h			\
	e-cell-date-edit.h			\
	e-cell-percent.h			\
	e-charset-picker.h			\
	e-clipped-label.h			\
	e-config-page.h				\
	e-combo-button.h			\
	e-dateedit.h				\
	e-dropdown-button.h			\
	e-map.h					\
	e-multi-config-dialog.h			\
	e-search-bar.h				\
	e-title-bar.h				\
	e-url-entry.h

libemiscwidgets_la_SOURCES =			\
	$(widgetsinclude_HEADERS)		\
	e-calendar.c				\
	e-calendar-item.c			\
	e-cell-date-edit.c			\
	e-cell-percent.c			\
	e-charset-picker.c			\
	e-clipped-label.c			\
	e-config-page.c				\
	e-combo-button.c			\
	e-dateedit.c				\
	e-dropdown-button.c			\
	e-map.c					\
	e-multi-config-dialog.c			\
	e-search-bar.c				\
	e-title-bar.c				\
	e-url-entry.c				\
	e-util-marshal.c

MARSHAL_GENERATED = e-util-marshal.c e-util-marshal.h
@EVO_MARSHAL_RULE@

libemiscwidgets_la_LIBADD = $(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/a11y/widgets/libevolution-widgets-a11y.la

noinst_LTLIBRARIES = libefilterbar.la

libefilterbar_la_SOURCES =	\
	e-filter-bar.c		\
	e-filter-bar.h

noinst_PROGRAMS = 			\
	test-calendar			\
	test-dateedit			\
	test-dropdown-button		\
	test-multi-config-dialog 	\
	test-title-bar

# test-calendar

test_calendar_SOURCES = 	\
	test-calendar.c

test_calendar_LDADD = 						\
	./libemiscwidgets.la					\
	../../e-util/libeutil.la				\
	$(GNOME_FULL_LIBS)

# test-dateedit

test_dateedit_SOURCES = 	\
	test-dateedit.c

test_dateedit_LDADD = 			\
	./libemiscwidgets.la		\
	../../e-util/libeutil.la	\
	$(GNOME_FULL_LIBS)

# test-dropdown-button

test_dropdown_button_SOURCES = 	\
	test-dropdown-button.c

test_dropdown_button_LDADD =	\
	./libemiscwidgets.la	\
	$(GNOME_FULL_LIBS)

# test-multi-config-dialog

test_multi_config_dialog_SOURCES = 	\
	test-multi-config-dialog.c

test_multi_config_dialog_LDADD = 	\
	./libemiscwidgets.la		\
	$(GNOME_FULL_LIBS)

# test-title-bar

test_title_bar_SOURCES = 	\
	test-title-bar.c

test_title_bar_LDADD = 		\
	./libemiscwidgets.la	\
	$(GNOME_FULL_LIBS)


BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES    = $(BUILT_SOURCES)

EXTRA_DIST =			\
	e-util-marshal.list	\
	ChangeLog.pre-1-4
