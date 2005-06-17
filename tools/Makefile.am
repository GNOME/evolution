privlibexec_PROGRAMS =			\
	killev

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-tools\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	$(GNOME_FULL_CFLAGS)

EXTRA_DIST = verify-evolution-install.sh

CORBA_SOURCE =                          \
        Evolution-Composer.h            \
        Evolution-Composer-common.c     \
        Evolution-Composer-skels.c      \
        Evolution-Composer-stubs.c

BUILT_SOURCES = $(CORBA_SOURCE)

idls =                                                          \
        $(top_srcdir)/composer/Evolution-Composer.idl

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(idls)

killev_SOURCES =						\
	killev.c

killev_LDADD =							\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/e-util/libeutil.la			\
	$(E_UTIL_LIBS)

CLEANFILES = evolution-addressbook-clean $(BUILT_SOURCES)
