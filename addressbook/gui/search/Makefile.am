ruledir = $(prefix)/share/evolution
rule_DATA = addresstypes.xml

INCLUDES =						\
	-DG_LOG_DOMAIN=\"e-addressbook-search\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/contact-editor	\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)/widgets/misc			\
	-DSEARCH_RULE_DIR=\"$(ruledir)\"		\
	$(BONOBO_GNOME_CFLAGS)

noinst_LIBRARIES =				\
	libeaddressbooksearch.a

libeaddressbooksearch_a_SOURCES =		\
	e-addressbook-search-dialog.c		\
	e-addressbook-search-dialog.h
