if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libevolution-addressbook.la
endif

INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/addressbook/			\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DG_LOG_DOMAIN=\"contact-list-editor\"		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

privsolib_LTLIBRARIES =				\
	libecontactlisteditor.la

libecontactlisteditor_la_SOURCES = 		\
	$(MARSHAL_GENERATED)			\
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

MARSHAL_GENERATED = e-contact-list-editor-marshal.c e-contact-list-editor-marshal.h
@EVO_MARSHAL_RULE@

glade_DATA  = contact-list-editor.glade
etspec_DATA = e-contact-list-editor.etspec

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(glade_DATA)	\
	     $(etspec_DATA)	\
	     e-contact-list-editor-marshal.list
