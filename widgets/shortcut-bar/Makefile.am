
noinst_LIBRARIES = libshortcut-bar.a
noinst_PROGRAMS = test-shortcut-bar

INCLUDES = \
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	$(EXTRA_GNOME_CFLAGS)

libshortcut_bar_a_SOURCES = 	\
	e-clipped-label.c	\
	e-clipped-label.h	\
	e-group-bar.c		\
	e-group-bar.h		\
	e-icon-bar-bg-item.c	\
	e-icon-bar-bg-item.h	\
	e-icon-bar.c		\
	e-icon-bar.h		\
	e-shortcut-bar.c	\
	e-shortcut-bar.h	\
	e-vscrolled-bar.c	\
	e-vscrolled-bar.h

test_shortcut_bar_SOURCES = 	\
	test-shortcut-bar.c

test_shortcut_bar_LDADD = 	\
	./libshortcut-bar.a	\
	../e-text/libetext.a	\
	../../e-util/libeutil.a	\
	$(EXTRA_GNOME_LIBS)

test_shortcut_bar_LDFLAGS =	\
	`gnome-config --libs gdk_pixbuf gnomecanvaspixbuf`
