INCLUDES = 			\
	-I$(includedir)		\
	$(GNOME_INCLUDEDIR)	\
	-I../../gcalendar

bin_PROGRAMS = gncal objedit

objedit_SOURCES = objedit.c

gncal_SOURCES = \
	gncal.c		\
	gncal.h		\
	calcs.c		\
	calcs.h		\
	clist.c		\
	clist.h

LINK_FLAGS = \
	$(GNOME_LIBDIR) \
	$(GNOMEUI_LIBS) \
	$(INTLLIBS)	\
	../../gcalendar/gtkcalendar.o \
	../../gcalendar/lib_date.o

gncal_LDADD = $(LINK_FLAGS)
objedit_LDADD = $(LINK_FLAGS)

EXTRA_DIST = \
	gncal.desktop

Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop

