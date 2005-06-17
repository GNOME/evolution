INCLUDES =					\
	-I$(top_srcdir)                         \
	-I$(top_srcdir)/widgets                 \
	$(EXTRA_GNOME_CFLAGS)			\
	$(GNOME_BONOBO_CFLAGS)			\
	$(GNOME_INCLUDEDIR)			\
	$(ICONV_CFLAGS)				\
	$(GNOME_FULL_CFLAGS)			\
        -DG_LOG_DOMAIN=\"e-table\"

noinst_LTLIBRARIES = libgal-a11y-etable.la

libgal_a11y_etable_la_SOURCES =		\
	gal-a11y-e-tree.c			\
	gal-a11y-e-tree-factory.c		\
	gal-a11y-e-cell.c			\
	gal-a11y-e-cell-text.c			\
	gal-a11y-e-cell-tree.c			\
	gal-a11y-e-cell-toggle.c		\
	gal-a11y-e-cell-popup.c			\
	gal-a11y-e-cell-registry.c		\
	gal-a11y-e-cell-vbox.c			\
	gal-a11y-e-table.c			\
	gal-a11y-e-table-item.c			\
	gal-a11y-e-table-item-factory.c		\
	gal-a11y-e-table-click-to-add.c		\
	gal-a11y-e-table-column-header.c	\
	gal-a11y-e-table-click-to-add-factory.c	\
	gal-a11y-e-table-factory.c		

libgal_a11y_etableincludedir = $(privincludedir)/a11y/e-table

libgal_a11y_etableinclude_HEADERS =		\
	gal-a11y-e-tree.h			\
	gal-a11y-e-tree-factory.h		\
	gal-a11y-e-cell.h			\
	gal-a11y-e-cell-text.h			\
	gal-a11y-e-cell-tree.h			\
	gal-a11y-e-cell-toggle.h		\
	gal-a11y-e-cell-popup.h			\
	gal-a11y-e-cell-registry.h		\
	gal-a11y-e-cell-vbox.h			\
	gal-a11y-e-table.h			\
	gal-a11y-e-table-item.h			\
	gal-a11y-e-table-click-to-add-factory.h	\
	gal-a11y-e-table-click-to-add.h		\
	gal-a11y-e-table-column-header.h	\
	gal-a11y-e-table-item-factory.h		\
	gal-a11y-e-table-factory.h
