SUBDIRS =					\
	po					\
	macros					\
	camel					\
	composer				\
	tests

dist-hook:
	for subdir in intl ; do \
	  mkdir $(distdir)/$$subdir ; \
	  cp -pr $(srcdir)/$$subdir $(distdir) ; \
	done
