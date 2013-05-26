module_LTLIBRARIES = module-book-config-ldap.la

module_book_config_ldap_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-book-config-ldap\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(LDAP_CFLAGS)

module_book_config_ldap_la_SOURCES = \
	evolution-book-config-ldap.c				\
	e-source-ldap.c						\
	e-source-ldap.h

module_book_config_ldap_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(LDAP_LIBS)

module_book_config_ldap_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
