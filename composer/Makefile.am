gladedir = $(datadir)/evolution/glade

glade_DATA =					\
	e-msg-composer-address-dialog.glade	\
	e-msg-composer-address-dialog.glade.h	\
	e-msg-composer-attachment.glade		\
	e-msg-composer-attachment.glade.h

libcomposerincludedir = $(includedir)/composer

noinst_LTLIBRARIES = libcomposer.la
libcomposer_la_LDFLAGS = -static

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_srcdir)/camel			\
	-I$(top_builddir)/camel			\
	$(GNOME_INCLUDEDIR)			\
	$(BONOBO_GNOME_CFLAGS)			\
	-DG_LOG_DOMAIN=\"composer\"

CPPFLAGS =					\
	-DE_GLADEDIR=\"$(gladedir)\"

libcomposer_la_SOURCES = 			\
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
	e-msg-composer-select-file.c		\
	e-msg-composer-select-file.h		\
	e-msg-composer.c			\
	e-msg-composer.h			

EXTRA_DIST =					\
	$(glade_DATA)				\
	ChangeLog
