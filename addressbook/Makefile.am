
SUBDIRS = util printing importers gui

error_DATA = addressbook.error
errordir = $(privdatadir)/errors

@EVO_PLUGIN_RULE@

EXTRA_DIST =			\
	addressbook.error.xml

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
