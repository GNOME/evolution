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
	$(GTKHTML_CFLAGS)					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            	\
	-DG_LOG_DOMAIN=\"evolution-executive-summary\"

COMPOSER_GENERATED = \
	Composer.h			\
	Composer-common.c		\
	Composer-skels.c		\
	Composer-stubs.c

Composer-impl.o: Composer.h

selectnamesdir = $(top_srcdir)/addressbook/gui/component/select-names

IDLS = \
	$(selectnamesdir)/Evolution-Addressbook-SelectNames.idl	\
	$(top_srcdir)/composer/Evolution-Composer.idl		\
	$(top_srcdir)/composer/Composer.idl

$(COMPOSER_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl` \
		$(top_srcdir)/composer/Composer.idl

evolution_executive_summary_SOURCES =		\
	$(COMPOSER_GENERATED)		\
	component-factory.c		\
	component-factory.h		\
	e-summary.c			\
	e-summary.h			\
	e-summary-callbacks.c		\
	e-summary-callbacks.h		\
	e-summary-factory.c		\
	e-summary-factory.h		\
	e-summary-prefs.c		\
	e-summary-prefs.h		\
	e-summary-url.c			\
	e-summary-url.h			\
	e-summary-util.c		\
	e-summary-util.h		\
	main.c

evolution_executive_summary_LDADD = 							\
	$(top_builddir)/shell/libeshell.la						\
	$(top_builddir)/widgets/misc/libemiscwidgets.a					\
	$(top_builddir)/e-util/libeutil.la						\
	$(top_builddir)/executive-summary/evolution-services/libevolution-services.la	\
	$(BONOBO_VFS_GNOME_LIBS)							\
	$(EXTRA_GNOME_LIBS)								\
	-lgal										\
	$(GTKHTML_LIBS)									\
	$(DB3_LDADD)

gladedir = $(datadir)/evolution/glade
glade_DATA = executive-summary-config.glade

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-executive-summary.pure

evolution-executive-summary.pure: evolution-executive-summary
	@rm -f evolution-executive-summary.pure
	$(PLINK) $(evolution_executive_summary_OBJECTS) $(evolution_executive_summary_LDADD) $(LIBS)

endif

EXTRA_DIST = $(glade_DATA)
CLEANFILES += $(COMPOSER_GENERATED)
BUILT_SOURCES = $(COMPOSER_GENERATED)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
