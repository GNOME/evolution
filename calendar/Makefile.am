if ENABLE_PILOT_CONDUITS
CONDUIT_DIR = conduits
else
CONDUIT_DIR =
endif

SUBDIRS = idl common gui importers $(CONDUIT_DIR)

error_DATA = calendar.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

EXTRA_DIST =			\
	calendar.error.xml	\
	ChangeLog.pre-1-4	\
	$(error_DATA)		\
	zones.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)
