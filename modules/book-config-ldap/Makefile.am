module_LTLIBRARIES = module-book-config-ldap.la

module_book_config_ldap_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-book-config-ldap\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(LDAP_CFLAGS)						\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

module_book_config_ldap_la_SOURCES = \
	evolution-book-config-ldap.c

module_book_config_ldap_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(LDAP_LIBS)						\
	$(NULL)

module_book_config_ldap_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk
