INCLUDES =					\
	-I$(top_srcdir)/camel			\
	-I$(top_builddir)/camel			\
	$(GNOME_INCLUDEDIR)

noinst_LIBRARIES =				\
	libevolutionwidgets.a

libevolutionwidgets_a_SOURCES = 		\
	e-cell.c				\
	e-cell.h				\
	e-cell-text.c				\
	e-cell-text.h				\
	e-cell-toggle.c				\
	e-cell-toggle.h				\
	e-cursors.c				\
	e-cursors.h				\
	e-table-col.c				\
	e-table-col.h				\
	e-table-group.c				\
	e-table-group.h				\
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
	e-table-sorted.h

noinst_PROGRAMS =	\
	table-test

table_test_SOURCES = 	\
	table-test.c 

table_test_LDADD =		\
	$(EXTRA_GNOME_LIBS)	\
	libevolutionwidgets.a

EXTRA_DIST =		\
	sample.table
