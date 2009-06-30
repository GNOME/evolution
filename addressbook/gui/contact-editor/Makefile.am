if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libevolution-addressbook.la
endif

AM_CPPFLAGS =						\
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
	eab-editor.c				\
	eab-editor.h				\
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

glade_DATA = 				\
	contact-editor.glade		\
	fullname.glade

EXTRA_DIST=	$(glade_DATA)

-include $(top_srcdir)/git.mk
