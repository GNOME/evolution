bin_PROGRAMS =	evolution-executive-summary

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/shell					\
	-I$(top_srcdir)/calendar				\
	-I$(top_builddir)/calendar/cal-client			\
	-I$(top_srcdir)/libical/src/libical			\
	-I$(top_builddir)/libical/src/libical			\
	$(EXTRA_GNOME_CFLAGS)					\
	$(GNOME_VFS_CFLAGS)					\
	$(GTKHTML_CFLAGS)					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            	\
	-DG_LOG_DOMAIN=\"evolution-executive-summary\"

IDLS =								\
	$(top_srcdir)/mail/Mail.idl

MAIL_GENERATED =						\
	Mail.h							\
	Mail-common.c						\
	Mail-skels.c						\
	Mail-stubs.c

$(MAIL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) `$(GNOME_CONFIG) --cflags idl` $(top_srcdir)/mail/Mail.idl

evolution_executive_summary_SOURCES =		\
	$(MAIL_GENERATED)			\
	component-factory.c		\
	component-factory.h		\
	e-summary.c			\
	e-summary.h			\
	e-summary-calendar.c		\
	e-summary-calendar.h		\
	e-summary-factory.c		\
	e-summary-factory.h		\
	e-summary-mail.c		\
	e-summary-mail.h		\
	e-summary-rdf.h			\
	e-summary-rdf.c			\
	e-summary-type.h		\
	e-summary-weather.c		\
	e-summary-weather.h		\
	main.c				\
	metar.c				\
	metar.h				\
	my-evolution-html.h

evolution_executive_summary_LDADD = 					\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/calendar/cal-client/libcal-client.la		\
	$(top_builddir)/calendar/cal-util/libcal-util.la		\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/libical/src/libical/libical.la	\
	$(BONOBO_VFS_GNOME_LIBS)			\
	$(EXTRA_GNOME_LIBS)				\
	-lgal						\
	$(GTKHTML_LIBS)

Locationdir = $(datadir)/evolution
Location_DATA = Locations

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Summary.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA) $(Location_DATA)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-executive-summary.pure

evolution-executive-summary.pure: evolution-executive-summary
	@rm -f evolution-executive-summary.pure
	$(PLINK) $(evolution_executive_summary_OBJECTS) $(evolution_executive_summary_LDADD) $(LIBS)

endif

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
