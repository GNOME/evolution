privsolib_LTLIBRARIES = libmenus.la

libmenus_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"  \
	-DG_LOG_DOMAIN=\"menus\"		\
	$(GNOME_PLATFORM_CFLAGS)		\
	$(E_UTIL_CFLAGS)

libmenus_la_SOURCES = 				\
	gal-define-views-dialog.c		\
	gal-define-views-model.c		\
	gal-view-collection.c			\
	gal-view-etable.c			\
	gal-view-factory-etable.c		\
	gal-view-factory.c			\
	gal-view-instance-save-as-dialog.c	\
	gal-view-instance.c			\
	gal-view-new-dialog.c			\
	gal-view.c

ui_DATA = 					\
	gal-define-views.ui			\
	gal-view-new-dialog.ui		\
	gal-view-instance-save-as-dialog.ui

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

libmenus_la_LDFLAGS = $(NO_UNDEFINED)

libmenus_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/widgets/table/libetable.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(GNOME_PLATFORM_LIBS)				\
	$(E_UTIL_LIBS)

icons =
EXTRA_DIST =			\
	 $(icons)		\
	 $(ui_DATA)

-include $(top_srcdir)/git.mk
