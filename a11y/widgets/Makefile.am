
# for debug
#A11Y_CFLAGS += -pedantic -ansi -DACC_DEBUG -Werror

privlib_LTLIBRARIES = libevolution-widgets-a11y.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-a11y\"		\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/a11y				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(A11Y_CFLAGS)					\
	$(EVOLUTION_CALENDAR_CFLAGS)

libevolution_widgets_a11y_la_SOURCES =		\
	ea-calendar-item.c			\
	ea-calendar-item.h			\
	ea-calendar-cell.c			\
	ea-calendar-cell.h			\
	ea-combo-button.c			\
	ea-combo-button.h			\
	ea-widgets.c				\
	ea-widgets.h

libevolution_widgets_a11y_la_LIBADD =   \
        $(top_builddir)/a11y/libevolution-a11y.la
                                                                                
