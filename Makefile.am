SUBDIRS =					\
	po					\
	macros					\
	camel					\
	e-util					\
	widgets					\
	composer				\
	tests					\
	addressbook

dist-hook:
	for subdir in intl ; do \
	  mkdir $(distdir)/$$subdir ; \
	  cp -pr $(srcdir)/$$subdir $(distdir) ; \
	done
