gladedir = $(datadir)/evolution/glade

glade_DATA =					\
	e-msg-composer-address-dialog.glade	\
	e-msg-composer-attachment.glade

libcomposerincludedir = $(includedir)/composer

noinst_LTLIBRARIES = libcomposer.la
libcomposer_la_LDFLAGS = -static

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_srcdir)/camel			\
	-I$(top_builddir)/camel			\
	$(GNOME_INCLUDEDIR)			\
	$(BONOBO_GNOME_CFLAGS)

CPPFLAGS =					\
	-DE_GLADEDIR=\"$(gladedir)\"

libcomposer_la_SOURCES = 			\
	e-msg-composer-address-dialog.c		\
	e-msg-composer-address-entry.c		\
	e-msg-composer-attachment-bar.c		\
	e-msg-composer-attachment.c		\
	e-msg-composer-hdrs.c			\
	e-msg-composer.c			

libcomposerinclude_HEADERS = 			\
	e-msg-composer-address-dialog.h		\
	e-msg-composer-address-entry.h		\
	e-msg-composer-attachment-bar.h		\
	e-msg-composer-attachment.h		\
	e-msg-composer-hdrs.h			\
	e-msg-composer.h			

## libcomposer_LDADD =	 			\
	$(top_builddir)/camel/libcamel.la	\
	$(BONOBO_GNOME_LIBS)			\
	$(GNOME_LIBDIR)				\
	$(GNOMEUI_LIBS)				\
	$(INTLLIBS)				\
	$(PTHREAD_LIB)				\
	$(EXTRA_GNOME_LIBS)


EXTRA_DIST =					\
	$(glade_DATA)				\
	ChangeLog


