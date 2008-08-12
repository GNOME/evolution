INCLUDES = -I. 						\
	-I$(top_srcdir)	                                \
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_builddir)/mail                          \
	-I$(top_srcdir)/camel                           \
	-I$(top_builddir)/composer			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"          \
	-DCONNECTOR_GLADEDIR=\""$(gladedir)"\"


@EVO_PLUGIN_RULE@

plugin_DATA =	\
	org-gnome-email-custom-header.eplug

plugin_LTLIBRARIES = liborg-gnome-email-custom-header.la

liborg_gnome_email_custom_header_la_SOURCES = 		\
	email-custom-header.c					\
	email-custom-header.h										
liborg_gnome_email_custom_header_la_LIBADD = 			\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/mail/libevolution-mail.la 		\
	$(EVOLUTION_MAIL_LIBS)					\
	$(NO_UNDEFINED_REQUIRED_LIBS)				

liborg_gnome_email_custom_header_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_evolution_email_custom_header.schemas.in
schema_DATA     = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	if test -z "$(DESTDIR)" ; then											\
		for p in $(schema_DATA) ; do										\
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p;	\
		done													\
	fi

glade_DATA =							\
	org-gnome-email-custom-header.glade			\
	email-custom-header.glade

EXTRA_DIST = 							\
	org-gnome-email-custom-header.eplug.xml			\
	$(schema_in_files)					\
	$(glade_DATA)					

BUILT_SOURCES = 				\
	org-gnome-email-custom-header.eplug 			\
	$(error_i18n)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(schema_DATA)
