INCLUDES = 			\
	-I$(includedir)		\
	$(GNOME_INCLUDEDIR)	\
	-I../../gcalendar

bin_PROGRAMS = gncal

gncal_SOURCES = \
	gncal.c		\
	gncal.h		\
	calcs.c		\
	calcs.h

gncal_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOMEUI_LIBS) \
	$(INTLLIBS)	\
	../../gcalendar/gtkcalendar.o \
	../../gcalendar/lib_date.o

EXTRA_DIST = \
	gncal.desktop

Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop

