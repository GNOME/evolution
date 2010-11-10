ui_DATA = 					\
	e-table-config.ui

privsolib_LTLIBRARIES = libetable.la

libetable_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	$(E_UTIL_CFLAGS)			\
	$(E_WIDGETS_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)		\
	-DEVOLUTION_UIDIR=\"$(uidir)\"	\
        -DG_LOG_DOMAIN=\"e-table\"

libetable_la_SOURCES = 				\
	e-cell.c				\
	e-cell-checkbox.c			\
	e-cell-combo.c				\
	e-cell-date.c				\
	e-cell-date-edit.c			\
	e-cell-number.c				\
	e-cell-percent.c			\
	e-cell-pixbuf.c				\
	e-cell-popup.c				\
	e-cell-size.c				\
	e-cell-text.c				\
	e-cell-toggle.c				\
	e-cell-tree.c				\
	e-cell-vbox.c				\
	e-cell-hbox.c				\
	e-table-click-to-add.c			\
	e-table-col.c				\
	e-table-column-specification.c		\
	e-table-config.c			\
	e-table-extras.c			\
	e-table-field-chooser-dialog.c		\
	e-table-field-chooser-item.c		\
	e-table-field-chooser.c			\
	e-table-group.c				\
	e-table-group-container.c		\
	e-table-group-leaf.c			\
	e-table-header.c			\
	e-table-header-item.c			\
	e-table-header-utils.c			\
	e-table-item.c				\
	e-table-memory-callbacks.c		\
	e-table-memory-store.c			\
	e-table-memory.c			\
	e-table-model.c				\
	e-table-one.c				\
	e-table-search.c			\
	e-table-selection-model.c		\
	e-table-sort-info.c			\
	e-table-sorted.c			\
	e-table-sorted-variable.c		\
	e-table-sorter.c			\
	e-table-sorting-utils.c			\
	e-table-specification.c			\
	e-table-state.c				\
	e-table-subset.c			\
	e-table-subset-variable.c		\
	e-table-utils.c				\
	e-table-without.c			\
	e-table.c				\
	e-tree-memory-callbacks.c		\
	e-tree-memory.c				\
	e-tree-model.c				\
	e-tree-selection-model.c		\
	e-tree-sorted.c				\
	e-tree-table-adapter.c			\
	e-tree.c				\
	gal-a11y-e-cell.c			\
	gal-a11y-e-cell-popup.c			\
	gal-a11y-e-cell-registry.c		\
	gal-a11y-e-cell-toggle.c		\
	gal-a11y-e-cell-tree.c			\
	gal-a11y-e-cell-vbox.c			\
	gal-a11y-e-table.c			\
	gal-a11y-e-table-click-to-add.c		\
	gal-a11y-e-table-click-to-add-factory.c	\
	gal-a11y-e-table-column-header.c	\
	gal-a11y-e-table-factory.c		\
	gal-a11y-e-table-item.c			\
	gal-a11y-e-table-item-factory.c		\
	gal-a11y-e-tree.c			\
	gal-a11y-e-tree-factory.c

libetableincludedir = $(privincludedir)/table

libetableinclude_HEADERS = 			\
	e-cell.h				\
	e-cell-checkbox.h			\
	e-cell-combo.h				\
	e-cell-date.h				\
	e-cell-date-edit.h			\
	e-cell-number.h				\
	e-cell-percent.h			\
	e-cell-pixbuf.h				\
	e-cell-popup.h				\
	e-cell-size.h				\
	e-cell-text.h				\
	e-cell-toggle.h				\
	e-cell-tree.h				\
	e-cell-vbox.h				\
	e-cell-hbox.h				\
	e-table-click-to-add.h			\
	e-table-col-dnd.h			\
	e-table-col.h				\
	e-table-column-specification.h		\
	e-table-config.h			\
	e-table-defines.h			\
	e-table-extras.h			\
	e-table-field-chooser-dialog.h		\
	e-table-field-chooser-item.h		\
	e-table-field-chooser.h			\
	e-table-group.h				\
	e-table-group-container.h		\
	e-table-group-leaf.h			\
	e-table-header.h			\
	e-table-header-item.h			\
	e-table-header-utils.h			\
	e-table-item.h				\
	e-table-memory-callbacks.h		\
	e-table-memory-store.h			\
	e-table-memory.h			\
	e-table-model.h				\
	e-table-one.h				\
	e-table-search.h			\
	e-table-selection-model.h		\
	e-table-sort-info.h			\
	e-table-sorted.h			\
	e-table-sorted-variable.h		\
	e-table-sorter.h			\
	e-table-sorting-utils.h			\
	e-table-specification.h			\
	e-table-state.h				\
	e-table-subset.h			\
	e-table-subset-variable.h		\
	e-table-utils.h				\
	e-table-without.h			\
	e-table.h				\
	e-tree-memory-callbacks.h		\
	e-tree-memory.h				\
	e-tree-model.h				\
	e-tree-selection-model.h		\
	e-tree-sorted.h				\
	e-tree-table-adapter.h			\
	e-tree.h				\
	gal-a11y-e-cell.h			\
	gal-a11y-e-cell-popup.h			\
	gal-a11y-e-cell-registry.h		\
	gal-a11y-e-cell-toggle.h		\
	gal-a11y-e-cell-tree.h			\
	gal-a11y-e-cell-vbox.h			\
	gal-a11y-e-table.h			\
	gal-a11y-e-table-click-to-add.h		\
	gal-a11y-e-table-click-to-add-factory.h	\
	gal-a11y-e-table-column-header.h	\
	gal-a11y-e-table-factory.h		\
	gal-a11y-e-table-item.h			\
	gal-a11y-e-table-item-factory.h		\
	gal-a11y-e-tree.h			\
	gal-a11y-e-tree-factory.h

libetable_la_LDFLAGS = $(NO_UNDEFINED)

libetable_la_LIBADD =						\
	$(top_builddir)/a11y/libevolution-a11y.la		\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/widgets/text/libetext.la		\
	$(top_builddir)/libgnomecanvas/libgnomecanvas.la	\
	$(top_builddir)/libart_lgpl/libart_lgpl.la		\
	$(E_UTIL_LIBS)						\
	$(E_WIDGETS_LIBS)					\
	$(MATH_LIB)						\
	$(GNOME_PLATFORM_LIBS)

icons = 			\
	add-col.xpm		\
	arrow-down.xpm		\
	arrow-up.xpm		\
	add-col.xpm		\
	check-empty.xpm		\
	check-filled.xpm	\
	remove-col.xpm		\
	tree-expanded.xpm	\
	tree-unexpanded.xpm

EXTRA_DIST =			\
	$(icons)		\
	$(ui_DATA)

-include $(top_srcdir)/git.mk
