privlibexec_SCRIPTS =					\
	csv2vcard					\
	evolution-addressbook-clean

privlibexec_PROGRAMS =			\
	evolution-addressbook-export	

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-addressbook-tools\"	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-I$(top_srcdir)/addressbook			\
	-I$(top_builddir)/addressbook			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

evolution_addressbook_export_SOURCES =				\
	evolution-addressbook-export.c				\
	evolution-addressbook-export-list-cards.c 		\
	evolution-addressbook-export-list-folders.c		\
	evolution-addressbook-export.h

evolution_addressbook_export_LDADD =				\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

EXTRA_DIST = $(privlibexec_SCRIPTS) \
	evolution-addressbook-clean.in

CLEANFILES= evolution-addressbook-clean

evolution-addressbook-clean: evolution-addressbook-clean.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@EVOLUTION_TOOLSDIR\@?$(privlibexecdir)?g' \
           < $(srcdir)/evolution-addressbook-clean.in > evolution-addressbook-clean.tmp \
          && mv evolution-addressbook-clean.tmp evolution-addressbook-clean
