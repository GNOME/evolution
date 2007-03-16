INCLUDES =						\
	-I$(top_srcdir)\
	-I$(top_srcdir)/camel                           \
	-I$(top_srcdir)/widgets/misc			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	-DEVOLUTION_ICONSDIR=\""$(imagesdir)"\"		\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"		\
	-DEVOLUTION_BUTTONSDIR=\""$(buttonsdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DCAMEL_PROVIDERDIR=\""$(camel_providerdir)"\"	\
	-DPLUGINDIR=\""$(plugindir)"\"			\
	-DPREFIX=\""$(prefix)"\"			

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-mail-attachments-import-ics.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-mail-attachments-import-ics.la

liborg_gnome_evolution_mail_attachments_import_ics_la_SOURCES = icsimporter.c
liborg_gnome_evolution_mail_attachments_import_ics_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_evolution_mail_attachments_import_ics_la_LIBADD = 			\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)

EXTRA_DIST = org-gnome-evolution-mail-attachments-import-ics.eplug.xml
