INCLUDES =						\
	-DG_LOG_DOMAIN=\"wombat\"			\
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

WOMBAT_IDL = Evolution-Wombat.idl

WOMBAT_IDL_GENERATED =				\
	Evolution-Wombat.h			\
	Evolution-Wombat-common.c		\
	Evolution-Wombat-skels.c		\
	Evolution-Wombat-stubs.c

$(WOMBAT_IDL_GENERATED): $(WOMBAT_IDL)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl` $(WOMBAT_IDL)

idldir = $(datadir)/idl

idl_DATA = $(WOMBAT_IDL)

wombat_SOURCES = 			\
	$(WOMBAT_IDL_GENERATED)		\
	wombat-interface-check.c	\
	wombat-interface-check.h	\
	wombat-moniker.c		\
	wombat-moniker.h		\
	wombat-private-moniker.c	\
	wombat-private-moniker.h	\
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

oafdir = $(datadir)/oaf

if ENABLE_LDAP
OAF_IN_FILE=GNOME_Evolution_WombatLDAP.oaf.in
else
OAF_IN_FILE=GNOME_Evolution_WombatNOLDAP.oaf.in
endif

GNOME_Evolution_Wombat.oaf.in: $(OAF_IN_FILE)
	cp $< $@

oaf_in_files = GNOME_Evolution_Wombat.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_DATA) $(oaf_in_files) $(idl_DATA) \
	     GNOME_Evolution_WombatLDAP.oaf.in GNOME_Evolution_WombatNOLDAP.oaf.in

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: wombat.pure

wombat.pure: wombat
	@rm -f wombat.pure
	$(PLINK) $(wombat_LDFLAGS) $(wombat_OBJECTS) $(wombat_LDADD) $(LIBS)

endif
