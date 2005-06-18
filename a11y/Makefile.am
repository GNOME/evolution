# Somewhat odd looking to have "." in SUBDIRS, but apparently it works?
SUBDIRS = e-text e-table . calendar widgets addressbook

if OS_WIN32
WIN32_BOOTSTRAP_LIBS =					\
	$(top_builddir)/win32/libemiscwidgets.la	\
	$(top_builddir)/win32/libetable.la		\
	$(top_builddir)/win32/libetext.la
endif

# for debug
#A11Y_CFLAGS += -pedantic -ansi -DACC_DEBUG -Werror

privlib_LTLIBRARIES = libevolution-a11y.la

INCLUDES =				\
	$(A11Y_CFLAGS)

libevolution_a11y_la_SOURCES =		\
	ea-factory.h			\
	ea-cell-table.h			\
	ea-cell-table.c			\
	gal-a11y-util.c

libevolution_a11yincludedir = $(privincludedir)/a11y

libevolution_a11yinclude_HEADERS =		\
	gal-a11y-util.h				\
	gal-a11y-factory.h

libevolution_a11y_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_a11y_la_LIBADD =			\
	$(WIN32_BOOTSTRAP_LIBS)			\
	e-text/libgal-a11y-etext.la		\
	e-table/libgal-a11y-etable.la		\
	$(top_builddir)/e-util/libeutil.la	\
	$(EXTRA_GNOME_LIBS)
