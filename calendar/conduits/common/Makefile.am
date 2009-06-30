AM_CPPFLAGS = 					\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/e-util			\
	-I$(top_builddir)/e-util		\
	$(EVOLUTION_CALENDAR_CONDUIT_CFLAGS)

# Calendar Conduit library
privconduit_LTLIBRARIES = libecalendar_common_conduit.la

ecalendar_common_conduitincludedir = $(privincludedir)/calendar/conduits/common

ecalendar_common_conduitinclude_HEADERS =		\
	libecalendar-common-conduit.h

libecalendar_common_conduit_la_SOURCES = 	\
	libecalendar-common-conduit.c

libecalendar_common_conduit_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
libecalendar_common_conduit_la_LIBADD = 				\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/e-util/libeconduit.la		 		\
	$(EVOLUTION_CALENDAR_CONDUIT_LIBS)



-include $(top_srcdir)/git.mk
