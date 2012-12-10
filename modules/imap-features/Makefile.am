NULL =

module_LTLIBRARIES = module-imap-features.la

module_imap_features_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-imap-features\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(CHAMPLAIN_CFLAGS)					\
	$(GTKHTML_CFLAGS)					\
	$(NULL)

module_imap_features_la_SOURCES = \
	evolution-imap-features.c				\
	e-mail-config-header-manager.c				\
	e-mail-config-header-manager.h				\
	e-mail-config-imap-headers-page.c			\
	e-mail-config-imap-headers-page.h			\
	$(NULL)

module_imap_features_la_LIBADD = \
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(CHAMPLAIN_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(NULL)

module_imap_features_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
