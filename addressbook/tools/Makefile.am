bin_PROGRAMS =			\
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
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(GNOME_FULL_CFLAGS)

evolution_addressbook_export_SOURCES =				\
	evolution-addressbook-export.c				\
	evolution-addressbook-export-list-cards.c 		\
	evolution-addressbook-export-list-folders.c		\
	evolution-addressbook-export.h

evolution_addressbook_export_LDADD =				\
	$(GNOME_FULL_LIBS)					\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/widgets/menus/libmenus.la		\
	$(top_builddir)/camel/libcamel.la			\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/e-util/ename/libename.la		\
	$(top_builddir)/e-util/libeutil.la

