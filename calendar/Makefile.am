INCLUDES = 			\
	-I$(includedir)	
	-I$(includedir)

bin_PROGRAMS = gncal

gncal_SOURCES = \
	gncal.c		\
	gncal.h		\
	calcs.c		\
	calcs.h

gncal_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOMEUI_LIBS) \
	$(top_builddir)/support/libsupport.a \
	$(INTLLIBS)

EXTRA_DIST = \
	gncal.desktop

Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop

