guidir = $(datadir)/evolution/gui

gui_DATA =					\
	e-msg-composer-address-dialog.glade	\
	e-msg-composer-attachment.glade		\
	e-msg-composer.glade

INCLUDES =					\
	-I$(top_srcdir)/camel			\
	-I$(top_builddir)/camel			\
	$(GNOME_INCLUDEDIR)

CPPFLAGS = 					\
	-DE_GUIDIR=\"$(guidir)\"

noinst_LIBRARIES =				\
	libevolutionwidgets.a

libevolutionwidgets_a_SOURCES = 		\
	e-cursors.c				\
	e-cursors.h				\
	e-msg-composer-address-dialog.c		\
	e-msg-composer-address-dialog.h		\
	e-msg-composer-address-entry.c		\
	e-msg-composer-address-entry.h		\
	e-msg-composer-attachment-bar.c		\
	e-msg-composer-attachment-bar.h		\
	e-msg-composer-attachment.c		\
	e-msg-composer-attachment.h		\
	e-msg-composer-hdrs.c			\
	e-msg-composer-hdrs.h			\
	e-msg-composer.c			\
	e-msg-composer.h			\
	e-table-col.c				\
	e-table-col.h				\
	e-table-group.c				\
	e-table-group.h				\
	e-table-header.c			\
	e-table-header.h			\
	e-table-header-item.c			\
	e-table-header-item.h			\
	e-table-model.c				\
	e-table-model.h				\
	e-table-render.c			\
	e-table-render.h			\
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
