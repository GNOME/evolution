INCLUDES =						\
	-DG_LOG_DOMAIN=\"EName\"			\
	-I$(srcdir)					\
	-I$(srcdir)/..					\
	-I$(top_srcdir) 				\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
	$(E_NAME_CFLAGS)

ename_libs = 						\
	libename.la 					\
	$(E_NAME_LIBS)

lib_LTLIBRARIES = libename.la

libename_la_SOURCES =					\
	e-address-western.c				\
	e-name-western.c

libenameincludedir = $(includedir)/ename

libenameinclude_HEADERS =				\
	e-address-western.h				\
	e-name-western-tables.h				\
	e-name-western.h


noinst_LTLIBRARIES = libename-static.la
libename_static_la_SOURCES = $(libename_la_SOURCES)
libename_static_la_LDFLAGS = --all-static


noinst_PROGRAMS =		\
	test-ename-western	\
	test-ename-western-gtk

test_ename_western_SOURCES =	\
	test-ename-western.c

test_ename_western_LDADD =	\
	$(ename_libs)

test_ename_western_gtk_SOURCES =				\
	test-ename-western-gtk.c

test_ename_western_gtk_LDADD = 			\
	$(ename_libs) 				\
	$(E_UTIL_LIBS)				\
	$(top_builddir)/e-util/libeutil.la
