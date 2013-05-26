noinst_PROGRAMS=import-cert

TEST_CPPFLAGS=						\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/smime/lib			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CERT_UI_CFLAGS)

TEST_LIBS= \
	$(top_builddir)/smime/lib/libessmime.la		\
	-L/home/toshok/src/mozilla/mozilla/dist/lib	\
	$(CERT_UI_LIBS)					\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)

import_cert_LDADD=$(TEST_LIBS)

-include $(top_srcdir)/git.mk
