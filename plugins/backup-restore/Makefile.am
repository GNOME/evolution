AM_CPPFLAGS =						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-I$(top_srcdir)					\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(SHELL_CFLAGS)					\
	$(E_UTIL_CFLAGS)				

@EVO_PLUGIN_RULE@

error_DATA = org-gnome-backup-restore.error
errordir = $(privdatadir)/errors

plugin_DATA = org-gnome-backup-restore.eplug org-gnome-backup-restore.xml
plugin_LTLIBRARIES = liborg-gnome-backup-restore.la

liborg_gnome_backup_restore_la_SOURCES = backup-restore.c
liborg_gnome_backup_restore_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_backup_restore_la_LIBADD =	\
	$(top_builddir)/e-util/libeutil.la

privlibexec_PROGRAMS = evolution-backup
evolution_backup_SOURCES = backup.c
evolution_backup_LDADD = $(SHELL_LIBS)		\
	$(top_builddir)/e-util/libeutil.la

if OS_WIN32
evolution_backup_LDFLAGS = -mwindows
endif


EXTRA_DIST = 					\
	org-gnome-backup-restore.eplug.xml	\
	org-gnome-backup-restore.error.xml	\
	org-gnome-backup-restore.xml

BUILT_SOURCES = org-gnome-backup-restore.eplug \
		org-gnome-backup-restore.error
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
