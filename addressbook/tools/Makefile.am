privlibexec_SCRIPTS =					\
	csv2vcard					\
	evolution-addressbook-clean

bin_PROGRAMS =			\
	evolution-addressbook-export	

noinst_PROGRAMS= evolution-addressbook-abuse

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
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED	\
	$(GNOME_FULL_CFLAGS)

evolution_addressbook_export_SOURCES =				\
	evolution-addressbook-export.c				\
	evolution-addressbook-export-list-cards.c 		\
	evolution-addressbook-export-list-folders.c		\
	evolution-addressbook-export.h

evolution_addressbook_export_LDADD =				\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/e-util/ename/libename.la		\
	$(top_builddir)/e-util/libeutil.la

evolution_addressbook_abuse_LDADD =				\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/e-util/ename/libename.la		\
	$(top_builddir)/e-util/libeutil.la

EXTRA_DIST = $(privlibexec_SCRIPTS) \
	evolution-addressbook-clean.in

CLEANFILES= evolution-addressbook-clean

evolution-addressbook-clean: evolution-addressbook-clean.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@EVOLUTION_TOOLSDIR\@?$(privlibexecdir)?g' \
           < $(srcdir)/evolution-addressbook-clean.in > evolution-addressbook-clean.tmp \
          && mv evolution-addressbook-clean.tmp evolution-addressbook-clean
