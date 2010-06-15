SUBDIRS = common importers gui

error_DATA = calendar.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

EXTRA_DIST =			\
	calendar.error.xml	\
	$(error_DATA)		\
	zones.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
