INCLUDES = 					\
	-I $(top_srcdir)			\
	-I $(top_srcdir)/libibex		\
	-I $(top_srcdir)/camel			\
	-I $(top_srcdir)/e-util			\
	-I $(top_srcdir)/camel/providers/mbox	\
	-I $(GNOME_INCLUDEDIR)			\
	$(GTKHTML_CFLAGS)			\
	-DG_LOG_DOMAIN=\"filter\"
##

noinst_PROGRAMS =				\
	filter-driver

noinst_LTLIBRARIES =				\
	libfilter.la

##

filter_driver_SOURCES = 			\
	filter-driver.c

filter_driver_LDADD = 				\
	libfilter.la				\
	../camel/libcamel.la			\
	../e-util/libeutil.la			\
	../libibex/libibex.la			\
	$(GNOME_LIBDIR)				\
	$(GNOMEUI_LIBS)				\
	$(INTLLIBS)				\
	$(PTHREAD_LIB)				\
	$(EXTRA_GNOME_LIBS)

##

libfilter_la_SOURCES =				\
	filter-arg-types.c			\
	filter-arg-types.h			\
	filter-arg.c				\
	filter-arg.h				\
	filter-xml.c				\
	filter-xml.h				\
	filter-format.c				\
	filter-format.h				\
	filter-druid.c				\
	filter-druid.h				\
	filter-editor.c				\
	filter-editor.h

EXTRA_DIST = blank.xpm check.xpm
