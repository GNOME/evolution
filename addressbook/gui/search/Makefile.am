ruledir = $(privdatadir)
rule_DATA = addresstypes.xml

EXTRA_DIST = addresstypes.xml

INCLUDES =						\
	-DG_LOG_DOMAIN=\"e-addressbook-search\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES =				\
	libeaddressbooksearch.la

libeaddressbooksearch_la_SOURCES =		\
	e-addressbook-search-dialog.c		\
	e-addressbook-search-dialog.h
