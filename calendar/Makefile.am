SUBDIRS = versit

INCLUDES = 			\
	-I$(includedir)					\
	$(GNOME_INCLUDEDIR)				\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I../../gcalendar

bin_PROGRAMS = gnomecal

gnomecal_SOURCES = \
	calendar.c		\
	calendar.h		\
	calobj.c		\
	calobj.h		\
	gncal-day-view.c	\
	gncal-day-view.h	\
	gncal-full-day.c	\
	gncal-full-day.h	\
	gncal-week-view.c	\
	gncal-week-view.h	\
	gnome-cal.c		\
	gnome-cal.h		\
	main.c			\
	timeutil.c		\
	timeutil.h		\
	view-utils.h		\
	view-utils.c		\
	views.h			\
	views.c			\
	eventedit.c		\
	eventedit.h
	

#gncal_SOURCES = \
#	gncal.c			\
#	gncal.h			\
#	calcs.c			\
#	calcs.h			\
#	clist.c			\
#	clist.h			\
#	calendar.c		\
#	calendar.h		\
#	calobj.c		\
#	calobj.h		\
#	gncal-day-view.c	\
#	gncal-day-view.h	\
#	timeutil.c		\
#	timeutil.h

LINK_FLAGS = \
	$(GNOME_LIBDIR) \
	$(GNOMEUI_LIBS) \
	$(INTLLIBS) versit/libversit.a


#gncal_LDADD = $(LINK_FLAGS)
#objedit_LDADD = $(LINK_FLAGS)

gnomecal_LDADD = $(LINK_FLAGS)

EXTRA_DIST = \
	gncal.desktop

Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop

