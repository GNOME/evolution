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
	-I$(top_srcdir)/libical/src			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	$(WOMBAT_CFLAGS)

IDLS = \
	Evolution-Wombat.idl

IDL_GENERATED_H = \
	Evolution-Wombat.h

IDL_GENERATED_C =				\
	Evolution-Wombat-common.c		\
	Evolution-Wombat-skels.c		\
	Evolution-Wombat-stubs.c

IDL_GENERATED = $(IDL_GENERATED_H) $(IDL_GENERATED_C)

$(IDL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(srcdir)/Evolution-Wombat.idl

$(IDL_GENERATED_C): $(IDL_GENERATED_H)


privlibexec_PROGRAMS =	\
	evolution-wombat

evolution_wombat_SOURCES = \
	$(IDL_GENERATED)		\
	wombat-interface-check.c	\
	wombat-interface-check.h	\
	wombat.c

evolution_wombat_LDADD =						\
	$(top_builddir)/addressbook/backend/pas/libpas.la		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/calendar/pcs/libpcsfile.a			\
	$(top_builddir)/calendar/pcs/libpcs.a				\
	$(top_builddir)/calendar/cal-util/libcal-util.la		\
	$(top_builddir)/e-util/libedb3util.la				\
	$(top_builddir)/e-util/libeutil.la				\
	$(WOMBAT_LIBS)

if ENABLE_LDAP
SERVER_IN_FILE=GNOME_Evolution_WombatLDAP.server.in.in
else
SERVER_IN_FILE=GNOME_Evolution_WombatNOLDAP.server.in.in
endif

GNOME_Evolution_Wombat.server.in.in: $(SERVER_IN_FILE) Makefile
	cp $< $@

server_in_files = GNOME_Evolution_Wombat.server.in.in

server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(privlibexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

idl_DATA = $(IDLS)

EXTRA_DIST = $(server_DATA)				\
	$(server_in_files)				\
	$(idl_DATA)					\
	ChangeLog.pre-1-4				\
	GNOME_Evolution_WombatLDAP.server.in.in		\
	GNOME_Evolution_WombatNOLDAP.server.in.in

BUILT_SOURCES = $(IDL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-wombat.pure

evolution-wombat.pure: evolution-wombat
	@rm -f evolution-wombat.pure
	$(PLINK) $(evolution_wombat_LDFLAGS) $(evolution_wombat_OBJECTS) $(evolution_wombat_LDADD) $(LIBS)

endif
