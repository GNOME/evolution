SUBDIRS = versit
help_base = $(datadir)/gnome/help/cal

INCLUDES = 			\
	-I$(includedir)					\
	$(GNOME_INCLUDEDIR)				\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I../../gcalendar

bin_PROGRAMS = gnomecal

gnomecal_SOURCES = \
	alarm.c			\
	alarm.h			\
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
	gncal-year-view.c	\
	gncal-year-view.h	\
	getdate.y		\
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
	gncal.desktop	\
	topic.dat	\
	gnome-cal.html


Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop

install-data-local:
	$(mkinstalldirs) $(help_base)/C
	$(INSTALL_DATA)  $(srcdir)/topic.dat $(help_base)/C/topic.dat
	$(INSTALL_DATA)  $(srcdir)/gnome-cal.html $(help_base)/C/gnome-cal.html
	cd $(help_base)/C; ln -sf gnome-cal.html index.html	