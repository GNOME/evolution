bin_SCRIPTS =				\
	csv2vcard			\
	evolution-move-tasks		\
	evolution-addressbook-clean	\
	killev

bin_PROGRAMS =				\
	evolution-addressbook-import	\
	evolution-addressbook-export

noinst_PROGRAMS = evolution-addressbook-abuse

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-tools\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-DEVOLUTION_BINDIR=\""$(bindir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-I$(top_srcdir)/addressbook			\
	$(GNOME_FULL_CFLAGS)

EXTRA_DIST = $(bin_SCRIPTS) verify-evolution-install.sh \
	evolution-addressbook-clean.in

evolution_addressbook_import_LDADD =				\
	$(GNOME_FULL_LIBS)					\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/camel/libcamel.la			\
	$(top_builddir)/libibex/libibex.la			\
	$(top_builddir)/e-util/ename/libename.la		\
	$(top_builddir)/libversit/libversit.a			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/menus/libmenus.la

evolution_addressbook_export_LDADD =					\
	$(evolution_addressbook_import_LDADD)

evolution_addressbook_abuse_LDADD =					\
	$(evolution_addressbook_import_LDADD)

CLEANFILES = evolution-addressbook-clean


evolution-addressbook-clean: evolution-addressbook-clean.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@EVOLUTION_BINDIR\@?$(bindir)?g' \
	   < $(srcdir)/evolution-addressbook-clean.in > evolution-addressbook-clean.tmp \
	  && mv evolution-addressbook-clean.tmp evolution-addressbook-clean
