INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_builddir)/composer                      \
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-print-message.eplug org-gnome-print-message.xml
plugin_LTLIBRARIES = liborg-gnome-print-message.la

liborg_gnome_print_message_la_SOURCES = print-message.c
liborg_gnome_print_message_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_print_message_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)

EXTRA_DIST = 					\
	org-gnome-print-message.eplug.xml\
	org-gnome-print-message.xml

BUILT_SOURCES = org-gnome-print-message.eplug
CLEANFILES = $(BUILT_SOURCES)
