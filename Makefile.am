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
	mail

dist-hook:
	for subdir in intl ; do \
	  mkdir $(distdir)/$$subdir ; \
	  cp -pr $(srcdir)/$$subdir $(distdir) ; \
	done
