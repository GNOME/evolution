
bin_PROGRAMS = evolution

INCLUDES = \
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(srcdir)/../widgets				\
	$(EXTRA_GNOME_CFLAGS)

evolution_SOURCES =		\
	main.c

evolution_LDADD = 					\
	-L../widgets/shortcut-bar/libshortcut-bar.a	\
	$(EXTRA_GNOME_LIBS)				


