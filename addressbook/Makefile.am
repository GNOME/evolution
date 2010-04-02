
if ENABLE_PILOT_CONDUITS
CONDUIT_SUBDIR = conduit
endif

SUBDIRS = util printing importers gui tools $(CONDUIT_SUBDIR)

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
