AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(SHELL_CFLAGS)					\
	$(E_UTIL_CFLAGS)				

@EVO_PLUGIN_RULE@

error_DATA = org-gnome-backup-restore.error
errordir = $(privdatadir)/errors

plugin_DATA = org-gnome-backup-restore.eplug
plugin_LTLIBRARIES = liborg-gnome-backup-restore.la

liborg_gnome_backup_restore_la_SOURCES = backup-restore.c
liborg_gnome_backup_restore_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_backup_restore_la_LIBADD =		\
	$(SHELL_LIBS)				\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/shell/libeshell.la

privlibexec_PROGRAMS = evolution-backup
evolution_backup_SOURCES = backup.c
evolution_backup_LDADD =			\
	$(SHELL_LIBS)				\
	$(top_builddir)/e-util/libeutil.la

if OS_WIN32
evolution_backup_LDFLAGS = -mwindows
endif


EXTRA_DIST = 					\
	org-gnome-backup-restore.eplug.xml	\
	org-gnome-backup-restore.error.xml

BUILT_SOURCES = org-gnome-backup-restore.eplug \
		org-gnome-backup-restore.error
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
