SUBDIRS = versit gnomecard
help_base = $(datadir)/gnome/help/cal

CFLAGS += -Wall

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
	eventedit.c		\
	eventedit.h		\
	getdate.y		\
	gncal-day-panel.c	\
	gncal-day-panel.h	\
	gncal-day-view.c	\
	gncal-day-view.h	\
	gncal-full-day.c	\
	gncal-full-day.h	\
	gncal-week-view.c	\
	gncal-week-view.h	\
	gncal-todo.c		\
	gncal-todo.h		\
	gncal-year-view.c	\
	gncal-year-view.h	\
	gnome-month-item.c	\
	gnome-month-item.h	\
	gnome-cal.c		\
	gnome-cal.h		\
	goto.c			\
	main.c			\
	main.h			\
	month-view.c		\
	month-view.h		\
	popup-menu.c		\
	popup-menu.h		\
	prop.c			\
	timeutil.c		\
	timeutil.h		\
	view-utils.h		\
	view-utils.c

LINK_FLAGS = \
	$(GNOME_LIBDIR) \
	$(GNOMEUI_LIBS) \
	$(INTLLIBS) versit/libversit.a


#gncal_LDADD = $(LINK_FLAGS)

gnomecal_LDADD = $(LINK_FLAGS)

EXTRA_DIST = \
	gncal.desktop	\
	topic.dat	\
	gnome-cal.html	\
	bell.xpm	\
	recur.xpm


Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop

install-data-local:
	$(mkinstalldirs) $(help_base)/C
	$(INSTALL_DATA)  $(srcdir)/topic.dat $(help_base)/C/topic.dat
	$(INSTALL_DATA)  $(srcdir)/gnome-cal.html $(help_base)/C/gnome-cal.html
	cd $(help_base)/C; ln -sf gnome-cal.html index.html	
