
bin_PROGRAMS = evolution

INCLUDES = \
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(srcdir)/../widgets				\
	$(BONOBO_GNOME_CFLAGS)

evolution_SOURCES =		\
	main.c			\
	e-folder.c		\
	e-folder.h		\
	e-folder-mail.c		\
	e-folder-mail.h		\
	e-init.c		\
	e-init.h		\
	e-shell.c		\
	e-shell.h


evolution_LDADD = 					\
	../widgets/shortcut-bar/libshortcut-bar.a	\
	../e-util/libeutil.a				\
	$(BONOBO_GNOME_LIBS)				

