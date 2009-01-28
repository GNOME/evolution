if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)				\
	$(top_builddir)/e-util/libeutil.la
endif

INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-save-attachments.eplug org-gnome-save-attachments.xml
plugin_LTLIBRARIES = liborg-gnome-save-attachments.la

liborg_gnome_save_attachments_la_SOURCES = save-attachments.c
liborg_gnome_save_attachments_la_LDFLAGS = -module \
	-avoid-version $(NO_UNDEFINED)

liborg_gnome_save_attachments_la_LIBADD =	\
	$(NO_UNDEFINED_REQUIRED_LIBS)


EXTRA_DIST = 					\
	org-gnome-save-attachments.eplug.xml	\
	org-gnome-save-attachments.xml

BUILT_SOURCES = org-gnome-save-attachments.eplug
CLEANFILES = $(BUILT_SOURCES)
