ruledir = $(datadir)/evolution-$(BASE_VERSION)
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
	-DG_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED			\
	-DLIBGNOME_DISABLE_DEPRECATED			\
	-DLIBGNOMEUI_DISABLE_DEPRECATED			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LIBRARIES =				\
	libeaddressbooksearch.a

libeaddressbooksearch_a_SOURCES =		\
	e-addressbook-search-dialog.c		\
	e-addressbook-search-dialog.h
