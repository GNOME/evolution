
INCLUDES =						\
	-I$(includedir)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/intl			 	\
	-I$(top_srcdir)/e-util			 	\
	-I$(top_srcdir)/camel				\
	$(CAMEL_CFLAGS)

check_LIBRARIES = libcameltest.a

libcameltest_a_SOURCES = \
	camel-test.c camel-test.h \
	messages.c messages.h \
	addresses.c addresses.h \
	folders.c folders.h \
	session.c session.h \
	streams.c streams.h \
	address-data.h

libcameltest_a_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la
