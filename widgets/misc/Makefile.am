# FIXME we use the EClippedLabel widget from EShortcutBar.  Probably
# it should be moved somewhere else.

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets/shortcut-bar	\
	$(EXTRA_GNOME_CFLAGS)			\
	-DG_LOG_DOMAIN=\"e-title-bar\"

noinst_LIBRARIES =	\
	libemiscwidgets.a

libemiscwidgets_a_SOURCES = 	\
	e-clipped-label.c	\
	e-clipped-label.h	\
	e-title-bar.c		\
	e-title-bar.h

noinst_PROGRAMS = 	\
	test-title-bar

test_title_bar_SOURCES = 	\
	test-title-bar.c

test_title_bar_LDADD = 						\
	./libemiscwidgets.a					\
	$(EXTRA_GNOME_LIBS)
