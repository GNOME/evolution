INCLUDES =						\
	-DG_LOG_DOMAIN=\"wombat\"			\
	-DPREFIX=\"$(prefix)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-I$(top_srcdir)/e-util				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	$(WOMBAT_CFLAGS)

bin_PROGRAMS =	\
	wombat

wombat_SOURCES = 			\
	wombat.c

if ENABLE_LDAP
LDAP_STUFF =								\
	$(top_builddir)/addressbook/backend/pas/libpasldap.a		\
	$(LDAP_LIBS)
endif

wombat_LDADD =								\
	$(top_builddir)/addressbook/backend/pas/libpasfile.a		\
	$(DB3_LDADD)							\
	$(LDAP_STUFF)							\
	$(top_builddir)/addressbook/backend/pas/libpas.a		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/camel/libcamel.la				\
	$(top_builddir)/e-util/ename/libename.la			\
	$(top_builddir)/calendar/pcs/libpcsfile.a			\
	$(top_builddir)/calendar/pcs/libpcs.a				\
	$(top_builddir)/calendar/cal-util/libcal-util.la		\
	$(top_builddir)/libical/src/libical/libical-evolution.la	\
	$(top_builddir)/libversit/libversit.a				\
	$(top_builddir)/e-util/libedb3util.la				\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libwombat/libwombat.la				\
	$(WOMBAT_LIBS)

serversdir = $(libdir)/bonobo/servers

if ENABLE_LDAP
SERVER_IN_FILE=GNOME_Evolution_WombatLDAP.server.in
else
SERVER_IN_FILE=GNOME_Evolution_WombatNOLDAP.server.in
endif

GNOME_Evolution_Wombat.server.in: $(SERVER_IN_FILE)
	cp $< $@

server_in_files = GNOME_Evolution_Wombat.server.in
servers_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

idldir = $(datadir)/idl

idl_DATA = Evolution-Wombat.idl

EXTRA_DIST = $(servers_DATA) $(server_in_files) $(idl_DATA) \
	     GNOME_Evolution_WombatLDAP.server.in GNOME_Evolution_WombatNOLDAP.server.in

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: wombat.pure

wombat.pure: wombat
	@rm -f wombat.pure
	$(PLINK) $(wombat_LDFLAGS) $(wombat_OBJECTS) $(wombat_LDADD) $(LIBS)

endif
