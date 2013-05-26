NULL =

module_LTLIBRARIES = module-backup-restore.la

module_backup_restore_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-backup-restore\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"		\
	-DPREFIX=\""$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBDIR=\""$(libdir)"\"				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(NULL)

module_backup_restore_la_SOURCES = \
	evolution-backup-restore.c				\
	e-mail-config-restore-page.c				\
	e-mail-config-restore-page.h				\
	e-mail-config-restore-ready-page.c			\
	e-mail-config-restore-ready-page.h			\
	$(NULL)

module_backup_restore_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(NULL)

module_backup_restore_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

privlibexec_PROGRAMS = evolution-backup

evolution_backup_CPPFLAGS =					\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"		\
	-DPREFIX=\""$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBDIR=\""$(libdir)"\"				\
	-DDBUS_SERVICES_DIR=\"'${datadir}'/dbus-1/services\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(NULL)

evolution_backup_SOURCES =					\
	evolution-backup-tool.c					\
	$(NULL)

evolution_backup_LDADD =					\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(NULL)

if OS_WIN32
evolution_backup_LDFLAGS = -mwindows
endif

error_DATA = org-gnome-backup-restore.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

BUILT_SOURCES = $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =							\
	org-gnome-backup-restore.error.xml			\
	$(NULL)

-include $(top_srcdir)/git.mk
