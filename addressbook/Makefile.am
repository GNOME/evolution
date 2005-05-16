if ENABLE_PILOT_CONDUITS
CONDUIT_SUBDIR=conduit
endif

SUBDIRS =	\
	util printing gui importers tools $(CONDUIT_SUBDIR)

error_DATA = addressbook.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

EXTRA_DIST =			\
	ChangeLog.pre-1-4	\
	addressbook.error.xml

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)
