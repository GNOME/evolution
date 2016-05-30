if WITH_BOGOFILTER
BOGOFILTER_DIR = bogofilter
endif

if ENABLE_LDAP
CONFIG_LDAP_DIR = book-config-ldap
endif

if ENABLE_WEATHER
CONFIG_WEATHER_DIR = cal-config-weather
endif

if WITH_SPAMASSASSIN
SPAMASSASSIN_DIR = spamassassin
endif

if ENABLE_TNEF
TNEF_ATTACHMENT_DIR = tnef-attachment
endif

if ENABLE_TEXT_HIGHLIGHT
TEXT_HIGHLIGHT_DIR = text-highlight
endif

SUBDIRS = \
	addressbook \
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
	contact-photos \
	gravatar \
	itip-formatter \
	mail-config \
	mailto-handler \
	mdn \
	offline-alert \
	plugin-lib \
	plugin-manager \
	prefer-plain \
	settings \
	startup-wizard \
	vcard-inline \
	web-inspector \
	webkit-content-editor \
	$(BOGOFILTER_DIR) \
	$(SPAMASSASSIN_DIR) \
	$(TNEF_ATTACHMENT_DIR) \
	$(TEXT_HIGHLIGHT_DIR)

-include $(top_srcdir)/git.mk
