changelogs =			\
	ChangeLog

EXTRA_DIST =			\
	AUTHORS			\
	$(changelogs)		\
	README			\
	HACKING			\
	MAINTAINERS		\
	NEWS			\
	evolution.spec.in

SUBDIRS =			\
	intl			\
	macros			\
	data			\
	e-util			\
	widgets			\
	shell			\
	libibex			\
	camel			\
	filter			\
	composer		\
	mail			\
	libical			\
	libversit		\
	addressbook		\
	calendar		\
	wombat			\
	art			\
	default_user		\
	tools			\
	po			\
	doc

appicondir = $(datadir)/pixmaps
appicon_DATA = evolution.png

applicationdir = $(datadir)/gnome/apps/Applications
application_DATA = evolution.desktop

dist-hook: evolution.spec
	cp evolution.spec $(distdir)
