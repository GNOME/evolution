INCLUDES =						\
	-DG_LOG_DOMAIN=\"e-card-gui\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES =				\
	libecardmerging.la

libecardmerging_la_SOURCES =			\
	e-card-merging.c			\
	e-card-merging.h


glade_DATA = e-card-duplicate-detected.glade				\
	     e-card-merging-book-commit-duplicate-detected.glade

EXTRA_DIST = 			\
	 $(glade_DATA)
