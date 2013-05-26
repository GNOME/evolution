NULL =

module_LTLIBRARIES = module-mail-config.la

module_mail_config_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-mail-config\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(NULL)

module_mail_config_la_SOURCES = \
	evolution-mail-config.c					\
	e-mail-config-local-accounts.c				\
	e-mail-config-remote-accounts.c				\
	e-mail-config-sendmail-backend.c			\
	e-mail-config-sendmail-backend.h			\
	e-mail-config-smtp-backend.c				\
	e-mail-config-smtp-backend.h				\
	e-mail-config-google-summary.c				\
	e-mail-config-google-summary.h				\
	e-mail-config-yahoo-summary.c				\
	e-mail-config-yahoo-summary.h				\
	$(NULL)

module_mail_config_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(NULL)

module_mail_config_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
