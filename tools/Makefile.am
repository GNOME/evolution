privlibexec_SCRIPTS =			\
	csv2vcard			\
	evolution-addressbook-clean

privlibexec_PROGRAMS =			\
	evolution-addressbook-import	\
	killev


noinst_PROGRAMS = evolution-addressbook-abuse

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-tools\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED	\
	$(GNOME_FULL_CFLAGS)

EXTRA_DIST = $(privlibexec_SCRIPTS) verify-evolution-install.sh \
	evolution-addressbook-clean.in

CORBA_SOURCE =                          \
        Evolution-Composer.h            \
        Evolution-Composer-common.c     \
        Evolution-Composer-skels.c      \
        Evolution-Composer-stubs.c

BUILT_SOURCES = $(CORBA_SOURCE)

idls =                                                          \
        $(top_srcdir)/composer/Evolution-Composer.idl

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(idls)

evolution_addressbook_import_LDADD =				\
	$(GNOME_FULL_LIBS)					\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/widgets/menus/libmenus.la

evolution_addressbook_abuse_LDADD =				\
	$(evolution_addressbook_import_LDADD)

killev_SOURCES =						\
	killev.c

killev_LDADD =							\
	$(top_builddir)/e-util/libeutil.la

CLEANFILES = evolution-addressbook-clean $(BUILT_SOURCES)


evolution-addressbook-clean: evolution-addressbook-clean.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@EVOLUTION_TOOLSDIR\@?$(privlibexecdir)?g' \
	   < $(srcdir)/evolution-addressbook-clean.in > evolution-addressbook-clean.tmp \
	  && mv evolution-addressbook-clean.tmp evolution-addressbook-clean
