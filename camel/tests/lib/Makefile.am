
INCLUDES =						\
	-I$(includedir)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/intl			 	\
	-I$(top_srcdir)/e-util			 	\
	-I$(top_srcdir)/camel				\
	-DCAMEL_BUILD_DIR=\"$(top_builddir)/camel\"	\
	$(CAMEL_CFLAGS)

check_LIBRARIES = libcameltest.a libcameltest-provider.a

libcameltest_a_SOURCES = \
	camel-test.c camel-test.h \
	messages.c messages.h \
	addresses.c addresses.h \
	folders.c folders.h \
	session.c session.h \
	streams.c streams.h \
	address-data.h

libcameltest_provider_a_SOURCES = \
	camel-test-provider.c camel-test-provider.h

libcameltest_a_LIBADD =				\
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/libedataserver/libedataserver-${BASE_VERSION}.la

libcameltest_provider_a_LIBADD =				\
	$(top_builddir)/camel/libcamel-provider.la \
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/libedataserver/libedataserver-${BASE_VERSION}.la
