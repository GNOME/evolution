INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"EName\"			\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
        $(GNOME_INCLUDEDIR)

gnome_libs =						\
	$(GNOME_LIBDIR)					\
        $(GNOMEUI_LIBS)					\
	$(GNOMEGNORBA_LIBS)				\
	$(INTLLIBS)

ename_libs = 						\
	libename.la 					\
	$(gnome_libs)

lib_LTLIBRARIES = libename.la

libename_la_SOURCES =					\
	e-name-western.c

libenameincludedir = $(includedir)/ename

libenameinclude_HEADERS =				\
	e-name-western.h

noinst_PROGRAMS =					\
	test-ename-western				\
	test-ename-western-gtk

test_ename_western_SOURCES =				\
	test-ename-western.c

test_ename_western_LDADD = $(ename_libs)

test_ename_western_gtk_SOURCES =				\
	test-ename-western-gtk.c

test_ename_western_gtk_LDADD = $(ename_libs) $(gnome_libs)
