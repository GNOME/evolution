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

noinst_LTLIBRARIES =				\
	libfilter.la

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
	filter-editor.h				\
	filter-driver.c				\
	filter-driver.h

EXTRA_DIST = blank.xpm check.xpm
