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
