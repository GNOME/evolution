emformatincludedir = $(privincludedir)/em-format

privsolib_LTLIBRARIES = libemformat.la

emformatinclude_HEADERS =				\
	em-format.h					\
	em-format-quote.h				\
	em-inline-filter.h				\
	em-stripsig-filter.h

libemformat_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)

libemformat_la_SOURCES =				\
	$(emformatinclude_HEADERS)			\
	em-format.c					\
	em-format-quote.c				\
	em-inline-filter.c				\
	em-stripsig-filter.c

libemformat_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libemformat_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)

-include $(top_srcdir)/git.mk
