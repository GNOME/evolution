if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libemiscwidgets.la
endif

AM_CPPFLAGS =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	$(E_UTIL_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)		\
	-DG_LOG_DOMAIN=\"e-text\"


privsolib_LTLIBRARIES = libetext.la

libetext_la_SOURCES =				\
	e-text-model-repos.c			\
	e-text-model.c				\
	e-text.c

libetextincludedir = $(privincludedir)/text

libetextinclude_HEADERS = 			\
	e-text-model-repos.h			\
	e-text-model.h				\
	e-text.h

libetext_la_LDFLAGS = $(NO_UNDEFINED)

libetext_la_LIBADD = 					\
	$(WIN32_BOOTSTRAP_LIBS)				\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/a11y/libevolution-a11y.la	\
	$(top_builddir)/widgets/table/libetable.la	\
	$(E_UTIL_LIBS)					\
	$(GNOME_PLATFORM_LIBS)				\
	$(REGEX_LIBS)

-include $(top_srcdir)/git.mk
