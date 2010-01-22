noinst_LTLIBRARIES = libevolution-calendarprivate.la

ecalendarcommonincludedir = $(privincludedir)/calendar/common
ecalendarcommoninclude_HEADERS =			\
	authentication.h

libevolution_calendarprivate_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/widgets				\
	$(EVOLUTION_CALENDAR_CFLAGS)

libevolution_calendarprivate_la_SOURCES =	\
	authentication.c			\
	authentication.h

-include $(top_srcdir)/git.mk
