imagesdir = $(datadir)/images/evolution

bin_PROGRAMS = evolution

INCLUDES = 						\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)					\
	$(BONOBO_GNOME_CFLAGS)				\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"		\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"   \
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DG_LOG_DOMAIN=\"evolution-shell\"

EVOLUTION_CORBA_GENERATED = 	\
	Evolution.h		\
	Evolution-common.c	\
	Evolution-skels.c	\
	Evolution-stubs.c

CLEANFILES = $(EVOLUTION_CORBA_GENERATED)

evolution_SOURCES = 			\
	$(EVOLUTION_CORBA_GENERATED)	\
	e-folder-type-repository.c	\
	e-folder-type-repository.h	\
	e-folder.c			\
	e-folder.h			\
	e-local-folder.c		\
	e-local-folder.h		\
	e-local-storage.c		\
	e-local-storage.h		\
	e-setup.c			\
	e-setup.h			\
	e-shell-utils.c			\
	e-shell-utils.h			\
	e-shell-view-menu.c		\
	e-shell-view-menu.h		\
	e-shell-view.c			\
	e-shell-view.h			\
	e-shell.c			\
	e-shell.h			\
	e-shortcuts-view.c		\
	e-shortcuts-view.h		\
	e-shortcuts.c			\
	e-shortcuts.h			\
	e-storage-set-view.c		\
	e-storage-set-view.h		\
	e-storage-set.c			\
	e-storage-set.h			\
	e-storage-watcher.c		\
	e-storage-watcher.h		\
	e-storage.c			\
	e-storage.h			\
	evolution-service-repository.c	\
	evolution-service-repository.h  \
	main.c

evolution_LDADD = 					\
	../widgets/shortcut-bar/libshortcut-bar.a	\
	../widgets/e-text/libetext.a			\
	../e-util/libeutil.la				\
	$(BONOBO_GNOME_LIBS)				

Evolution-impl.o: Evolution.h

$(EVOLUTION_CORBA_GENERATED): Evolution.idl evolution-service-repository.idl
	$(ORBIT_IDL) -I$(datadir)/idl -I`$(GNOME_CONFIG) --datadir`/idl -I$(srcdir) $(srcdir)/Evolution.idl

evolution_LDADD =						\
	$(top_builddir)/widgets/shortcut-bar/libshortcut-bar.a  \
	$(top_builddir)/widgets/e-text/libetext.a		\
	$(top_builddir)/e-util/libeutil.la			\
	$(BONOBO_GNOME_LIBS)

EXTRA_DIST = Evolution.idl			\
	Shell.idl				\
	evolution-service-repository.idl

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution.pure

evolution.pure: evolution
	@rm -f evolution.pure
	$(PLINK) $(evolution_LDFLAGS) $(evolution_OBJECTS) $(evolution_LDADD) $(LIBS)

endif
