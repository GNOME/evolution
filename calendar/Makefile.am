INCLUDES = 			\
	-I$(includedir)		\
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = gncal

gncal_SOURCES = \
	gncal.c		\
	gncal.h		\
	calcs.c		\
	calcs.h

gncal_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOMEUI_LIBS) \
	$(INTLLIBS)

EXTRA_DIST = \
	gncal.desktop

Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop

