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
	e-util			\
	libibex			\
	camel			\
	filter			\
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
