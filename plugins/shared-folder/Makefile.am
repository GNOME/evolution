INCLUDES =						\
	-I$(top_srcdir)\
	-I$(top_srcdir)/camel                           \
		$(EVOLUTION_MAIL_CFLAGS)\
		$(CAMEL_GROUPWISE_CFLAGS)\
		-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-shared-folder.eplug
plugin_LTLIBRARIES = liborg-gnome-shared-folder.la

liborg_gnome_shared_folder_la_SOURCES = share-folder-common.c share-folder.c install-shared.c share-folder.h 
liborg_gnome_shared_folder_la_LIBADD= $(CAMEL_GROUPWISE_LIBS)


liborg_gnome_shared_folder_la_LDFLAGS = -module -avoid-version

glade_DATA =properties.glade 

error_DATA = org-gnome-shared-folder-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

BUILT_SOURCES = $(error_i18n)
EXTRA_DIST = 						\
	$(error_DATA)					\
	$(error_i18n)					\
       	$(glade_DATA) 					\
	org-gnome-shared-folder.eplug.in
