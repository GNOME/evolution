
bin_PROGRAMS = evolution

INCLUDES = 						\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)					\
	$(BONOBO_GNOME_CFLAGS)				\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"

EVOLUTION_CORBA_GENERATED = 	\
	Evolution.h		\
	Evolution-common.c	\
	Evolution-skels.c	\
	Evolution-stubs.c

evolution_SOURCES =		\
	$(EVOLUTION_CORBA_GENERATED)	\
	main.c			\
	e-folder.c		\
	e-folder.h		\
	e-init.c		\
	e-init.h		\
	e-shell.c		\
	e-shell.h		\
	e-shell-shortcut.c	\
	e-shell-shortcut.h	\
	e-shell-view.c		\
	e-shell-view.h		\
	e-shell-view-menu.c	\
	e-shell-view-menu.h	\
	e-shortcut.c		\
	e-shortcut.h

Evolution-impl.o: Evolution.h

$(EVOLUTION_CORBA_GENERATED): Evolution.idl 
	orbit-idl -I`$(GNOME_CONFIG) --datadir`/idl -I$(srcdir) $(srcdir)/Evolution.idl

evolution_LDADD = 					\
	../widgets/shortcut-bar/libshortcut-bar.a	\
	../widgets/e-text/libetext.a			\
	../e-util/libeutil.la				\
	$(BONOBO_GNOME_LIBS)				

EXTRA_DIST = Evolution.idl
