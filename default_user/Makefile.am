SUBDIRS = local

defaultdir = $(datadir)/evolution/default_user
default_DATA = addressbook-sources.xml searches.xml vfolders.xml

EXTRA_DIST = $(default_DATA)

install-data-hook:
	-mkdir $(DESTDIR)$(defaultdir)/private
	-mkdir $(DESTDIR)$(defaultdir)/views
	-mkdir $(DESTDIR)$(defaultdir)/views/addressbook
	-mkdir $(DESTDIR)$(defaultdir)/views/calendar
	-mkdir $(DESTDIR)$(defaultdir)/views/mail
	-mkdir $(DESTDIR)$(defaultdir)/views/tasks
if ENABLE_NNTP
	-mkdir $(DESTDIR)$(defaultdir)/news
	-mkdir $(DESTDIR)$(defaultdir)/config
endif
