INCLUDES =					\
	$(EXTRA_GNOME_CFLAGS)			\
	$(GNOME_INCLUDEDIR)			\
	-I$(top_srcdir)/widgets/e-text		\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)

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
	e-table-col.c				\
	e-table-col.h				\
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
	e-table-sorted.c			\
	e-table-sorted.h			\
	e-table-subset.c			\
	e-table-subset.h			\
	e-table-subset-variable.c		\
	e-table-subset-variable.h

noinst_PROGRAMS =	\
	table-test

table_test_SOURCES = 	\
	test-table.c	\
	test-check.c	\
	test-cols.c	\
	table-test.c 

table_test_LDADD =				\
	libetable.a				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/e-util/libeutil.a	\
	$(top_builddir)/widgets/e-text/libetext.a

table_test_LDFLAGS = `gnome-config --libs gdk_pixbuf`

EXTRA_DIST =			\
	sample.table		\
	add-col.xpm		\
	check-empty.xpm		\
	check-filled.xpm	\
	remove-col.xpm
