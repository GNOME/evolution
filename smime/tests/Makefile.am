
noinst_PROGRAMS=import-cert

INCLUDES=						\
	-I$(top_srcdir)/smime/lib			\
	$(SMIME_CFLAGS)

TEST_LIBS= \
	$(top_builddir)/smime/lib/libessmime.la	\
	$(SMIME_LIBS)


import_cert_LDADD=$(TEST_LIBS)