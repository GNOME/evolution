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
	camel			\
	e-util			\
	widgets			\
	composer		\
	tests			\
	addressbook		\
	shell			\
	mail			\
	libversit		\
	calendar

dist-hook:
	for subdir in intl ; do \
	  mkdir $(distdir)/$$subdir ; \
	  cp -pr $(srcdir)/$$subdir $(distdir) ; \
	done
