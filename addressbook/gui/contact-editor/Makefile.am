privsolib_LTLIBRARIES =	libecontacteditor.la

libecontacteditor_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/			\
	-I$(top_srcdir)/addressbook/gui/widgets		\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DG_LOG_DOMAIN=\"contact-editor\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)

libecontacteditor_la_SOURCES = 			\
	eab-editor.c				\
	eab-editor.h				\
	e-contact-editor-fullname.c		\
	e-contact-editor-fullname.h		\
	e-contact-editor.c			\
	e-contact-editor.h			\
	e-contact-quick-add.c			\
	e-contact-quick-add.h

libecontacteditor_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libecontacteditor_la_LIBADD =						\
	$(top_builddir)/e-util/libevolution-util.la			\
	$(top_builddir)/addressbook/util/libeabutil.la			\
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la	\
	$(top_builddir)/addressbook/printing/libecontactprint.la	\
	$(EVOLUTION_ADDRESSBOOK_LIBS)					\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(GTKHTML_LIBS)

ui_DATA = 				\
	contact-editor.ui		\
	fullname.ui

EXTRA_DIST=	$(ui_DATA)

-include $(top_srcdir)/git.mk
