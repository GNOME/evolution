emformatincludedir = $(privincludedir)/em-format

privsolib_LTLIBRARIES = libemformat.la

emformatinclude_HEADERS =				\
	em-format.h					\
	em-format-quote.h				\
	em-stripsig-filter.h

libemformat_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

libemformat_la_SOURCES =				\
	$(emformatinclude_HEADERS)			\
	em-format.c					\
	em-format-quote.c				\
	em-stripsig-filter.c

libemformat_la_LDFLAGS = $(NO_UNDEFINED)

libemformat_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

-include $(top_srcdir)/git.mk
