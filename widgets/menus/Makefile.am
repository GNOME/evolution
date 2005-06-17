privlib_LTLIBRARIES = libmenus.la

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	-I$(top_srcdir)/widgets/menus		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"  \
	-DG_LOG_DOMAIN=\"menus\"		\
	$(EXTRA_GNOME_CFLAGS)			\
	$(GNOME_INCLUDEDIR)			\
	$(ICONV_CFLAGS)				\
	$(GNOME_FULL_CFLAGS)

libmenus_la_SOURCES = 				\
	gal-view-menus.c			\
	gal-define-views-dialog.c		\
	gal-define-views-model.c		\
	gal-view-collection.c			\
	gal-view-etable.c			\
	gal-view-factory-etable.c		\
	gal-view-factory.c			\
	gal-view-instance-save-as-dialog.c	\
	gal-view-instance.c			\
	gal-view-new-dialog.c			\
	gal-view.c				\
	gal-view-menus.h

glade_DATA = 					\
	gal-define-views.glade			\
	gal-view-new-dialog.glade		\
	gal-view-instance-save-as-dialog.glade

libmenusincludedir = $(privincludedir)/menus

libmenusinclude_HEADERS =			\
	gal-define-views-dialog.h		\
	gal-define-views-model.h		\
	gal-view-collection.h			\
	gal-view-etable.h			\
	gal-view-factory-etable.h		\
	gal-view-factory.h			\
	gal-view-instance-save-as-dialog.h	\
	gal-view-instance.h			\
	gal-view-new-dialog.h			\
	gal-view.h

icons =
EXTRA_DIST =			\
	 $(icons)		\
	 $(glade_DATA)
