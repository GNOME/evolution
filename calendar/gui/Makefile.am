INCLUDES = 			\
	-I$(includedir)		\
	$(GNOME_INCLUDEDIR)	\
	-I../../gcalendar

bin_PROGRAMS = gncal

gncal_SOURCES = \
	gncal.c			\
	gncal.h			\
	calcs.c			\
	calcs.h			\
	clist.c			\
	clist.h			\
	calendar.c		\
	calendar.h		\
	calobj.c		\
	calobj.h		\
	gncal-day-view.c	\
	gncal-day-view.h	\
	timeutil.c		\
	timeutil.h

LINK_FLAGS = \
	$(GNOME_LIBDIR) \
	$(GNOMEUI_LIBS) \
	$(INTLLIBS)

gncal_LDADD = $(LINK_FLAGS)

EXTRA_DIST = \
	gncal.desktop

Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop

