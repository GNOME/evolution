changelogs =			\
	ChangeLog

EXTRA_DIST =			\
	AUTHORS			\
	$(changelogs)		\
	README			\
	HACKING			\
	MAINTAINERS		\
	NEWS

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
	tests			\
	libical			\
	libversit		\
	addressbook		\
	calendar		\
	wombat			\
	art			\
	po

dist-hook:
	for subdir in intl ; do \
	  mkdir $(distdir)/$$subdir ; \
	  cp -pr $(srcdir)/$$subdir $(distdir) ; \
	done
