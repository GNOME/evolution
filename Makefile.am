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
	po			\
	macros			\
	data			\
	libibex			\
	camel			\
	e-util			\
	widgets			\
	composer		\
	mail			\
	tests			\
	addressbook		\
	shell			\
	libical			\
	libversit		\
	calendar

dist-hook:
	for subdir in intl ; do \
	  mkdir $(distdir)/$$subdir ; \
	  cp -pr $(srcdir)/$$subdir $(distdir) ; \
	done
