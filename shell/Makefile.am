
bin_PROGRAMS = evolution

INCLUDES = \
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(srcdir)/../widgets				\
	$(BONOBO_GNOME_CFLAGS)

EVOLUTION_CORBA_GENERATED = 	\
	Evolution.h		\
	Evolution-common.c	\
	Evolution-skels.c	\
	Evolution-stubs.c

evolution_SOURCES =		\
	main.c			\
	e-folder.c		\
	e-folder.h		\
	e-folder-mail.c		\
	e-folder-mail.h		\
	e-init.c		\
	e-init.h		\
	e-shell.c		\
	e-shell.h		\
	e-shell-view.c		\
	e-shell-view.h		\
	e-shell-view-menu.c	\
	e-shell-view-menu.h	\
	$(EVOLUTION_CORBA_GENERATED)

Evolution-impl.o: Evolution.h

$(EVOOLUTION_CORBA_GENERATED): Evolution.idl 
	orbit-idl -I`$(GNOME_CONFIG) --datadir`/idl -I$(srcdir) $(srcdir)/Evolution.idl

evolution_LDADD = 					\
	../widgets/shortcut-bar/libshortcut-bar.a	\
	../e-util/libeutil.a				\
	$(BONOBO_GNOME_LIBS)				

EXTRA_DIST = Evolution.idl