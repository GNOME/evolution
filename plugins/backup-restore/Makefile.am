INCLUDES =						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-I$(top_srcdir)					\
	$(SHELL_CFLAGS)					\
	$(E_UTIL_CFLAGS)				

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-backup-restore.eplug org-gnome-backup-restore.xml
plugin_LTLIBRARIES = liborg-gnome-backup-restore.la

liborg_gnome_backup_restore_la_SOURCES = backup-restore.c
liborg_gnome_backup_restore_la_LDFLAGS = -module -avoid-version

privlibexec_PROGRAMS = backup
backup_SOURCES = backup.c
backup_LDADD = $(SHELL_LIBS)

EXTRA_DIST = 					\
	org-gnome-backup-restore.eplug.xml	\
	org-gnome-backup-restore.xml