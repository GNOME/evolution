INCLUDES = $(WOMBAT_CFLAGS)

# CORBA stuff

IDLS = $(top_srcdir)/wombat/Evolution-Wombat.idl
IDL_GENERATED = \
	Evolution-Wombat.h \
	Evolution-Wombat-common.c \
	Evolution-Wombat-skels.c \
	Evolution-Wombat-stubs.c

$(IDLS): $(ORBIT_IDL)

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl $(IDL_INCLUDES) \
		$(top_srcdir)/wombat/Evolution-Wombat.idl

# Wombat library
privlib_LTLIBRARIES = libwombat.la

wombatincludedir = $(privincludedir)/wombat
wombatinclude_HEADERS = \
	wombat-client.h

libwombat_la_SOURCES = \
	$(IDL_GENERATED) \
	wombat-client.c \
	$(wombatinclude_HEADERS)

# Static lib for pilot conduits

noinst_LTLIBRARIES = libwombat-static.la

libwombat_static_la_SOURCES = $(libwombat_la_SOURCES)
libwombat_static_la_LDFLAGS = --all-static

BUILT_SOURCES = $(IDL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = ChangeLog.pre-1-4
