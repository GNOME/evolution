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
	po

dist-hook: evolution.spec
	cp evolution.spec $(distdir)
