if OS_WIN32
else
PLATFORM_DEP_TOOLS = \
	killev
endif

privlibexec_PROGRAMS =		\
	$(PLATFORM_DEP_TOOLS)

AM_CPPFLAGS =						\
	-DG_LOG_DOMAIN=\"evolution-tools\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	$(GNOME_PLATFORM_CFLAGS)

EXTRA_DIST = verify-evolution-install.sh

killev_SOURCES =						\
	killev.c

killev_LDADD =							\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/e-util/libeutil.la			\
	$(E_UTIL_LIBS)

CLEANFILES = evolution-addressbook-clean $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
