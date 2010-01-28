AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-to-task.eplug org-gnome-mail-to-task.xml
plugin_LTLIBRARIES = liborg-gnome-mail-to-task.la

liborg_gnome_mail_to_task_la_SOURCES = mail-to-task.c
liborg_gnome_mail_to_task_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_mail_to_task_la_LIBADD =	\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/calendar/common/libevolution-calendarprivate.la	\
	$(top_builddir)/calendar/gui/libevolution-cal-shared.la	\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(EVOLUTION_CALENDAR_LIBS)					\
	$(EVOLUTION_MAIL_LIBS)

EXTRA_DIST = org-gnome-mail-to-task.eplug.xml \
	     org-gnome-mail-to-task.xml

BUILT_SOURCES = org-gnome-mail-to-task.eplug
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
