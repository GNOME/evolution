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

noinst_LTLIBRARIES =				\
	libevolutionwidgets.la

libevolutionwidgets_la_SOURCES =		\
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
	e-msg-composer.h

