if ENABLE_LDAP
CONFIG_LDAP_DIR = book-config-ldap
endif

if ENABLE_WEATHER
CONFIG_WEATHER_DIR = cal-config-weather
endif

if ENABLE_MONO
MONO_DIR = plugin-mono
endif

if ENABLE_PYTHON
PYTHON_DIR = plugin-python
endif

if ENABLE_ONLINE_ACCOUNTS
ONLINE_ACCOUNTS_DIR = online-accounts
endif

SUBDIRS = \
	addressbook \
	bogofilter \
	calendar \
	mail \
	backup-restore \
	book-config-google \
	$(CONFIG_LDAP_DIR) \
	book-config-local \
	book-config-webdav \
	cal-config-caldav \
	cal-config-contacts \
	cal-config-google \
	cal-config-local \
	$(CONFIG_WEATHER_DIR) \
	cal-config-webcal \
	composer-autosave \
	mailto-handler \
	mdn \
	offline-alert \
	plugin-lib \
	plugin-manager \
	spamassassin \
	startup-wizard \
	web-inspector \
	$(MONO_DIR) \
	$(PYTHON_DIR) \
	$(ONLINE_ACCOUNTS_DIR)

-include $(top_srcdir)/git.mk
