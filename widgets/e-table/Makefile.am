
gladedir = $(datadir)/evolution/glade
glade_DATA = 					\
	e-table-config.glade			\
	e-table-group.glade

glade_headers = 				\
	e-table-config.glade.h			\
	e-table-group.glade.h

INCLUDES =					\
	$(EXTRA_GNOME_CFLAGS)			\
	$(GNOME_INCLUDEDIR)			\
	-DETABLE_GLADEDIR=\""$(gladedir)"\"	\
	-I$(top_srcdir)/widgets/e-text		\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)                         \
        -DG_LOG_DOMAIN=\"e-table\"

noinst_LIBRARIES =				\
	libetable.a

libetable_a_SOURCES = 				\
	e-cell.c				\
	e-cell.h				\
	e-cell-checkbox.c			\
	e-cell-checkbox.h			\
	e-cell-text.c				\
	e-cell-text.h				\
	e-cell-toggle.c				\
	e-cell-toggle.h				\
	e-table.c				\
	e-table.h				\
	e-table-col-dnd.h			\
	e-table-col.c				\
	e-table-col.h				\
	e-table-config.c			\
	e-table-config.h			\
	e-table-group.c				\
	e-table-group.h				\
	e-table-group-container.c		\
	e-table-group-container.h		\
	e-table-group-leaf.c			\
	e-table-group-leaf.h			\
	e-table-header.c			\
	e-table-header.h			\
	e-table-header-item.c			\
	e-table-header-item.h			\
	e-table-item.c				\
	e-table-item.h				\
	e-table-model.c				\
	e-table-model.h				\
	e-table-simple.c			\
	e-table-simple.h			\
	e-table-sort-info.c			\
	e-table-sort-info.h			\
	e-table-sorted.c			\
	e-table-sorted.h			\
	e-table-sorted-variable.c		\
	e-table-sorted-variable.h		\
	e-table-subset.c			\
	e-table-subset.h			\
	e-table-subset-variable.c		\
	e-table-subset-variable.h		\
	e-table-text-model.c			\
	e-table-text-model.h

noinst_PROGRAMS =	\
	table-test table-example-1 table-example-2 table-size-test

table_test_SOURCES = 	\
	test-table.c	\
	test-check.c	\
	test-cols.c	\
	table-test.c 	\
	table-test.h

table_test_LDADD =				\
	libetable.a				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/widgets/e-text/libetext.a \
	$(top_builddir)/e-util/libeutil.la	

table_test_LDFLAGS = `gnome-config --libs gdk_pixbuf`

table_size_test_SOURCES = 	\
	e-table-size-test.c

table_size_test_LDADD =				\
	libetable.a				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/widgets/e-text/libetext.a \
	$(top_builddir)/e-util/libeutil.la	

table_size_test_LDFLAGS = `gnome-config --libs gdk_pixbuf`

table_example_1_SOURCES = 	\
	e-table-example-1.c

table_example_1_LDADD =				\
	libetable.a				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/widgets/e-text/libetext.a \
	$(top_builddir)/e-util/libeutil.la	

table_example_1_LDFLAGS = `gnome-config --libs gdk_pixbuf`

table_example_2_SOURCES = 	\
	e-table-example-2.c

table_example_2_LDADD =				\
	libetable.a				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/widgets/e-text/libetext.a \
	$(top_builddir)/e-util/libeutil.la	

table_example_2_LDFLAGS = `gnome-config --libs gdk_pixbuf`

icons = 			\
	arrow-down.xpm		\
	arrow-up.xpm		\
	add-col.xpm		\
	check-empty.xpm		\
	check-filled.xpm	\
	remove-col.xpm

EXTRA_DIST =			\
	sample.table		\
	$(icons)		\
	$(glade_DATA)		\
	$(glade_headers)
