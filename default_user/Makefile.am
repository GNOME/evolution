SUBDIRS = local

defaultdir = $(privdatadir)/default_user
default_DATA = addressbook-sources.xml searches.xml vfolders.xml

EXTRA_DIST = $(default_DATA)

install-data-hook:
	-mkdir $(DESTDIR)$(defaultdir)/private
if ENABLE_NNTP
	-mkdir $(DESTDIR)$(defaultdir)/news
	-mkdir $(DESTDIR)$(defaultdir)/config
endif
