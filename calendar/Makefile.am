if ENABLE_PILOT_CONDUITS
CONDUIT_DIR = conduits
else
CONDUIT_DIR =
endif

SUBDIRS = idl common gui importers $(CONDUIT_DIR)

error_DATA = calendar-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

EXTRA_DIST =			\
	ChangeLog.pre-1-4	\
	$(error_DATA)		\
	zones.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(error_i18n)

CLEANFILES = $(BUILT_SOURCES)
