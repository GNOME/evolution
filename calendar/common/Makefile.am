noinst_LTLIBRARIES = libevolution-calendarprivate.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/a11y/calendar			\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(EVOLUTION_CALENDAR_CFLAGS)

libevolution_calendarprivate_la_SOURCES =	\
	authentication.c			\
	authentication.h

libevolution_calendarprivate_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la				\
	$(EVOLUTION_CALENDAR_LIBS)

libevolution_calendarprivate_la_LDFLAGS = -avoid-version
