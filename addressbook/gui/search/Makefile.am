ruledir = $(privdatadir)
rule_DATA = addresstypes.xml

EXTRA_DIST = addresstypes.xml

INCLUDES =						\
	-DG_LOG_DOMAIN=\"e-addressbook-search\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/contact-editor	\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_builddir)/shell				\
	-DSEARCH_RULE_DIR=\"$(ruledir)\"		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES =				\
	libeaddressbooksearch.la

libeaddressbooksearch_la_SOURCES =		\
	e-addressbook-search-dialog.c		\
	e-addressbook-search-dialog.h
