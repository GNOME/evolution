INCLUDES =						\
	-I$(top_srcdir)\
	-I$(top_srcdir)/camel                           \
		$(EVOLUTION_MAIL_CFLAGS)\
		$(CAMEL_GROUPWISE_CFLAGS)\
		-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-shared-folder.eplug
plugin_LTLIBRARIES = liborg-gnome-shared-folder.la

liborg_gnome_shared_folder_la_SOURCES = share-folder-common.c share-folder.c share-folder.h
liborg_gnome_shared_folder_la_LIBADD= $(prefix)/lib/libegroupwise-1.2.la


liborg_gnome_shared_folder_la_LDFLAGS = -module -avoid-version

 glade_DATA =properties.glade



EXTRA_DIST =$(glade_DATA)
