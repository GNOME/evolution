INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_builddir)/mail				\
	$(EVOLUTION_MAIL_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-account-disable.eplug
plugin_LTLIBRARIES = libmail-account-disable.la

libmail_account_disable_la_SOURCES = mail-account-disable.c
libmail_account_disable_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
libmail_account_disable_la_LIBADD = $(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)

EXTRA_DIST = org-gnome-mail-account-disable.eplug.xml
