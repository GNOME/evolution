INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-account-disable.eplug
plugin_LTLIBRARIES = libmail-account-disable.la

libmail_account_disable_la_SOURCES = mail-account-disable.c
libmail_account_disable_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-mail-account-disable.eplug.xml
