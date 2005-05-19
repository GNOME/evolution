INCLUDES =						\
	-I$(top_srcdir)\
	-I$(top_srcdir)/camel                           \
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mark-all-read.eplug
plugin_LTLIBRARIES = liborg-gnome-mark-all-read.la

liborg_gnome_mark_all_read_la_SOURCES = mark-all-read.c  
liborg_gnome_mark_all_read_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-mark-all-read.eplug.in
