# Calendar IDL files
                                                                                
CALENDAR_IDLS = $(top_srcdir)/calendar/idl/evolution-calendar.idl

CALENDAR_IDL_GENERATED_H = evolution-calendar.h

$(CALENDAR_IDL_GENERATED_H): $(CALENDAR_IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)   \
                $(top_srcdir)/calendar/idl/evolution-calendar.idl

SUBDIRS = calendar

INCLUDES =						\
	-DPREFIX=\"$(prefix)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DG_LOG_DOMAIN=\"evolution-a11y\"		\
	-I$(top_srcdir) 				\
	-DG_DISABLE_DEPRECATED				\
	-DLIBGNOME_DISABLE_DEPRECATED			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(A11Y_CFLAGS)

privlib_LTLIBRARIES = libevolution-a11y.la 

libevolution_a11y_la_SOURCES =				\
	$(CALENDAR_IDL_GENERATED_H)			\
        ea-factory.h


BUILT_SOURCES = $(CALENDAR_IDL_GENERATED_H)

libevolution_a11y_la_LIBADD =				\
	$(A11Y_LIBS)					\
	$(top_builddir)/a11y/calendar/libevolution-calendar-a11y.la 
