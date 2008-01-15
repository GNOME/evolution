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
	org-gnome-face.eplug	\
	org-gnome-face-ui.xml  

plugin_LTLIBRARIES = liborg-gnome-face.la

liborg_gnome_face_la_SOURCES = 		\
	face.c
liborg_gnome_face_la_LIBADD = 			\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/mail/libevolution-mail.la 		\
	$(EVOLUTION_MAIL_LIBS)					\
	$(NO_UNDEFINED_REQUIRED_LIBS)				

liborg_gnome_face_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

error_DATA = 					\
	org-gnome-face.errors.xml

errordir = $(privdatadir)/errors

EXTRA_DIST = 						\
	org-gnome-face.eplug.xml			\
	org-gnome-face-ui.xml				\
	$(error_DATA)

BUILT_SOURCES = 				\
	org-gnome-face.eplug 			\
	$(error_i18n)

CLEANFILES = $(BUILT_SOURCES)
