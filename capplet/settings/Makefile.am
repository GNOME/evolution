SUBDIRS = mail-autoconfig

privsolib_LTLIBRARIES = libevolution-mail-settings.la

libevolution_mail_settings_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/e-util				\
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
	-DANJAL_SETTINGS				\
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

libevolution_mail_settings_la_SOURCES = \
	mail-settings-view.c \
	mail-settings-view.h \
	mail-account-view.c \
	mail-account-view.h \
	mail-view.c \
	mail-view.h \
	mail-capplet-shell.c \
	mail-capplet-shell.h \
	mail-decoration.c \
	mail-decoration.h \
	anjal-mail-view.h \
	anjal-mail-view.c \
	mail-guess-servers.c \
	mail-guess-servers.h

libevolution_mail_settings_la_LIBADD = 				\
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
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/e-util/libeutil.la

libevolution_mail_settings_la_LDFLAGS =  $(NO_UNDEFINED)

