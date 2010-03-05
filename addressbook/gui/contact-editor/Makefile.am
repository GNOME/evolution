privsolib_LTLIBRARIES =	libecontacteditor.la

libecontacteditor_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/addressbook/			\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/widgets/table			\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DG_LOG_DOMAIN=\"contact-editor\"		\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

libecontacteditor_la_SOURCES = 			\
	eab-editor.c				\
	eab-editor.h				\
	e-contact-editor-fullname.c		\
	e-contact-editor-fullname.h		\
	e-contact-editor.c			\
	e-contact-editor.h			\
	e-contact-quick-add.c			\
	e-contact-quick-add.h

libecontacteditor_la_LDFLAGS = $(NO_UNDEFINED)

libecontacteditor_la_LIBADD =						\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/addressbook/util/libeabutil.la			\
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la	\
	$(top_builddir)/addressbook/gui/merging/libeabbookmerging.la	\
	$(top_builddir)/addressbook/printing/libecontactprint.la	\
	$(top_builddir)/widgets/menus/.libs/libmenus.la	\
	$(EVOLUTION_ADDRESSBOOK_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

ui_DATA = 				\
	contact-editor.ui		\
	fullname.ui

EXTRA_DIST=	$(ui_DATA)

-include $(top_srcdir)/git.mk
