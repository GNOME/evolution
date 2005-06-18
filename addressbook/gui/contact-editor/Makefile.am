if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libevolution-addressbook.la
endif

INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/addressbook/			\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/widgets/table			\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DG_LOG_DOMAIN=\"contact-editor\"		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

privsolib_LTLIBRARIES =				\
	libecontacteditor.la

libecontacteditor_la_SOURCES = 			\
	$(MARSHAL_GENERATED)			\
	eab-editor.c				\
	eab-editor.h				\
	e-contact-editor-im.c			\
	e-contact-editor-im.h			\
	e-contact-editor-address.c		\
	e-contact-editor-address.h		\
	e-contact-editor-fullname.c		\
	e-contact-editor-fullname.h		\
	e-contact-editor.c			\
	e-contact-editor.h			\
	e-contact-quick-add.c			\
	e-contact-quick-add.h

libecontacteditor_la_LDFLAGS = $(NO_UNDEFINED)

libecontacteditor_la_LIBADD =				\
	$(WIN32_BOOTSTRAP_LIBS)				\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

MARSHAL_GENERATED = e-contact-editor-marshal.c e-contact-editor-marshal.h
@EVO_MARSHAL_RULE@

glade_DATA = 				\
	im.glade			\
	contact-editor.glade		\
	fulladdr.glade			\
	fullname.glade

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST=	$(glade_DATA)		\
		e-contact-editor-marshal.list
