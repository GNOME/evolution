SUBDIRS = settings

bin_PROGRAMS = evolution-settings

evolution_settings_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/em-format			\
	-I$(top_srcdir)/mail				\
	-I$(top_srcdir)/composer			\
	-I$(top_builddir)/composer			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_srcdir)/smime/gui			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(CERT_UI_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_ICONSDIR=\""$(imagesdir)"\"		\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"		\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_BUTTONSDIR=\""$(buttonsdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DCAMEL_PROVIDERDIR=\""$(camel_providerdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DG_LOG_DOMAIN=\"evolution-mail\"		\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 

evolution_settings_SOURCES  = \
	anjal-settings-main.c 

if OS_WIN32
evolution_settings_LDFLAGS = -mwindows
endif

evolution_settings_LDADD = 				\
	$(EVOLUTION_MAIL_LIBS)				\
	$(CERT_UI_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(GTKHTML_LIBS)					\
	$(E_WIDGETS_LIBS)				\
	$(SMIME_LIBS)					\
	$(GNOME_PLATFORM_LIBS)				\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(top_builddir)/filter/libfilter.la		\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/capplet/settings/libevolution-mail-settings.la \
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/e-util/libeutil.la \
	$(top_builddir)/em-format/libemformat.la

