if ENABLE_PILOT_CONDUITS
CONDUIT_SUBDIR=conduit
endif

SUBDIRS =	\
	util printing gui importers tools $(CONDUIT_SUBDIR)

error_DATA = addressbook-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

EXTRA_DIST =			\
	ChangeLog.pre-1-4	\
	$(error_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(error_i18n)

CLEANFILES = $(BUILT_SOURCES)
