INCLUDES =						\
	-DG_LOG_DOMAIN=\"e-card-gui\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LIBRARIES =				\
	libecardmerging.a

libecardmerging_a_SOURCES =			\
	e-card-merging.c			\
	e-card-merging.h


gladedir   = $(datadir)/evolution/glade
glade_DATA = 	e-card-duplicate-detected.glade	\
		e-card-merging-book-commit-duplicate-detected.glade

EXTRA_DIST = 			\
	 $(glade_DATA)
