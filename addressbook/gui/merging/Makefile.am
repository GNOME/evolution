INCLUDES =						\
	-DG_LOG_DOMAIN=\"eab-contact-merging\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES =				\
	libeabbookmerging.la

libeabbookmerging_la_SOURCES =			\
	eab-contact-compare.c			\
	eab-contact-compare.h			\
	eab-contact-merging.c			\
	eab-contact-merging.h


glade_DATA = eab-contact-duplicate-detected.glade				\
	     eab-contact-commit-duplicate-detected.glade

EXTRA_DIST = 			\
	 $(glade_DATA)
