INCLUDES =						\
	-DG_LOG_DOMAIN=\"e-card-gui\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend/ebook	\
	$(BONOBO_GNOME_CFLAGS)				\
	$(EXTRA_GNOME_CFLAGS)

noinst_LIBRARIES =				\
	libecardmerging.a

libecardmerging_a_SOURCES =			\
	e-card-merging.c			\
	e-card-merging.h


gladedir   = $(datadir)/evolution/glade
glade_DATA = e-card-duplicate-detected.glade

EXTRA_DIST = 			\
	 $(glade_DATA)
