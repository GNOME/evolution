if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libevolution-addressbook.la
endif

AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/addressbook/			\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DG_LOG_DOMAIN=\"contact-list-editor\"		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

privsolib_LTLIBRARIES =				\
	libecontactlisteditor.la

libecontactlisteditor_la_SOURCES = 		\
	e-contact-list-editor.c			\
	e-contact-list-editor.h			\
	e-contact-list-model.c			\
	e-contact-list-model.h

libecontactlisteditor_la_LDFLAGS = $(NO_UNDEFINED)

libecontactlisteditor_la_LIBADD =						\
	$(WIN32_BOOTSTRAP_LIBS)							\
	$(top_builddir)/addressbook/util/libeabutil.la				\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la	\
	$(top_builddir)/widgets/table/libetable.la				\
	$(top_builddir)/widgets/misc/libemiscwidgets.la				\
	$(top_builddir)/e-util/libeutil.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

glade_DATA  = contact-list-editor.glade

EXTRA_DIST = $(glade_DATA)

-include $(top_srcdir)/git.mk
