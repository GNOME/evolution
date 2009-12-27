# for debug
#A11Y_CFLAGS += -pedantic -ansi -DACC_DEBUG -Werror

privsolib_LTLIBRARIES = libevolution-a11y.la

libevolution_a11y_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)	\
	$(A11Y_CFLAGS)

libevolution_a11y_la_SOURCES =		\
	ea-factory.h			\
	gal-a11y-util.c			\
	gal-a11y-util.h			\
	gal-a11y-factory.h

libevolution_a11y_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_a11y_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la	\
	$(GNOME_PLATFORM_LIBS)

-include $(top_srcdir)/git.mk
