INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

noinst_LTLIBRARIES = libemformat.la

libemformat_la_SOURCES =	\
	em-format.h		\
	em-format.c		\
	em-format-quote.h	\
	em-format-quote.c	\
	em-stripsig-filter.c	\
	em-stripsig-filter.h

libemformat_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libemformat_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/shell/libeshell.la	\
	$(EVOLUTION_MAIL_LIBS)

-include $(top_srcdir)/git.mk
