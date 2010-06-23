privsolib_LTLIBRARIES =	libecontactlisteditor.la

libecontactlisteditor_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/addressbook/			\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DG_LOG_DOMAIN=\"contact-list-editor\"		\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CAMEL_CFLAGS)

libecontactlisteditor_la_SOURCES = 		\
	e-contact-list-editor.c			\
	e-contact-list-editor.h			\
	e-contact-list-model.c			\
	e-contact-list-model.h

libecontactlisteditor_la_LDFLAGS = $(NO_UNDEFINED)

libecontactlisteditor_la_LIBADD =				\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/widgets/table/libetable.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(CAMEL_LIBS)

ui_DATA  = contact-list-editor.ui

EXTRA_DIST = $(ui_DATA)

-include $(top_srcdir)/git.mk
