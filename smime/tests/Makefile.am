
noinst_PROGRAMS=import-cert

INCLUDES=						\
	-I$(top_srcdir)/smime/lib			\
	$(CERT_UI_CFLAGS)

TEST_LIBS= \
	$(top_builddir)/smime/lib/libessmime.la	\
	-L/home/toshok/src/mozilla/mozilla/dist/lib \
	$(CERT_UI_LIBS)


import_cert_LDADD=$(TEST_LIBS)