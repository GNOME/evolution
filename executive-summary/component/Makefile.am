bin_PROGRAMS =	evolution-executive-summary

INCLUDES = \
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/e-text				\
	-I$(top_srcdir)						\
	-I$(top_builddir)/shell					\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/executive-summary			\
	-I$(top_srcdir)/executive-summary			\
	-I$(top_builddir)/executive-summary/evolution-services  \
	-I$(top_srcdir)/executive-summary/evolution-services    \
	$(EXTRA_GNOME_CFLAGS)					\
	$(GNOME_VFS_CFLAGS)					\
	$(UNICODE_CFLAGS)					\
	$(GTKHTML_CFLAGS)					\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            	\
	-DG_LOG_DOMAIN=\"evolution-executive-summary\"

CLEANFILES = $(COMPOSER_GENERATED)

COMPOSER_GENERATED = \
	Composer.h			\
	Composer-common.c		\
	Composer-skels.c		\
	Composer-stubs.c

Composer-impl.o: Composer.h

$(COMPOSER_GENERATED):
	$(ORBIT_IDL) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl` \
		-I$(srcdir) $(top_srcdir)/composer/Composer.idl

evolution_executive_summary_SOURCES =		\
	$(COMPOSER_GENERATED)		\
	component-factory.c		\
	component-factory.h		\
	e-summary.c			\
	e-summary.h			\
	e-summary-factory.c		\
	e-summary-factory.h		\
	e-summary-url.c			\
	e-summary-url.h			\
	e-summary-util.c		\
	e-summary-util.h		\
	main.c

evolution_executive_summary_LDADD = 					\
	$(top_builddir)/shell/libeshell.a		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/executive-summary/evolution-services/libevolution-services.la	\
	$(top_builddir)/executive-summary/widgets/libesummary-widgets.a	\
	$(BONOBO_VFS_GNOME_LIBS)			\
	$(EXTRA_GNOME_LIBS)				\
	$(GTKHTML_LIBS)					\
	$(UNICODE_LIBS)

