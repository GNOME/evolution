if OS_WIN32
platform_sources = eggsmclient-win32.c
platform_logout_test_ldflags = -mwindows
else
platform_defines = -DEGG_SM_CLIENT_BACKEND_XSMP
platform_ltlibraries = libeggdesktopfile.la
platform_libs = libeggdesktopfile.la -lSM -lICE
platform_sources = eggsmclient-xsmp.c
endif

noinst_LTLIBRARIES = \
	libeggsmclient.la \
	$(platform_ltlibraries)

libeggsmclient_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\""EggSMClient"\" \
	$(platform_defines) \
	$(EGG_SMCLIENT_CFLAGS)

libeggsmclient_la_LIBADD = \
	$(EGG_SMCLIENT_LIBS) \
	$(platform_libs)

libeggsmclient_la_LDFLAGS = \
	$(platform_ldflags)

libeggsmclient_la_SOURCES = \
	eggsmclient.c \
	eggsmclient.h \
	eggsmclient-private.h \
	$(platform_sources)

libeggdesktopfile_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\""EggSMClient"\" \
	$(platform_defines) \
	$(EGG_SMCLIENT_CFLAGS)

libeggdesktopfile_la_LIBADD = \
	$(EGG_SMCLIENT_LIBS)

libeggdesktopfile_la_SOURCES = \
	eggdesktopfile.c \
	eggdesktopfile.h

EXTRA_DIST = \
	eggsmclient-win32.c \
	eggsmclient-xsmp.c

-include $(top_srcdir)/git.mk
