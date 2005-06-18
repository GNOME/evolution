if OS_WIN32
WIN32_BOOTSTRAP_LIBS =						\
	$(top_builddir)/win32/libemiscwidgets.la		\
	$(top_builddir)/win32/libevolution-addressbook.la
endif

privlib_LTLIBRARIES = libevolution-addressbook-a11y.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-a11y\"		\
	-I$(top_srcdir)		                        \
	-I$(top_srcdir)/a11y				\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/addressbook/gui/widgets		\
	-DG_DISABLE_DEPRECATED				\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

libevolution_addressbook_a11y_la_SOURCES =	\
	ea-minicard.c				\
	ea-minicard.h				\
	ea-minicard-view.c			\
	ea-minicard-view.h			\
	ea-addressbook-view.c			\
	ea-addressbook-view.h			\
	ea-addressbook.c			\
	ea-addressbook.h

libevolution_addressbook_a11y_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_addressbook_a11y_la_LIBADD =	\
	$(WIN32_BOOTSTRAP_LIBS)			\
	$(EVOLUTION_ADDRESSBOOK_LIBS)
