# CORBA stuff

IDLS = $(top_builddir)/wombat/wombat.idl
IDL_GENERATED = \
	wombat.h \
	wombat-common.c \
	wombat-skels.c \
	wombat-stubs.c

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl` \
		$(top_builddir)/wombat/wombat.idl

# Wombat library
lib_LTLIBRARIES = libwombat.la

wombatincludedir = $(includedir)/evolution/wombat
wombatinclude_HEADERS = \
	wombat-client.h

libwombat_la_SOURCES = \
	$(IDL_GENERATED) \
	wombat-client.c \
	$(wombatinclude_HEADERS)
