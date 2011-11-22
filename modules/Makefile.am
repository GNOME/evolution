if ENABLE_MONO
MONO_DIR = plugin-mono
endif

if ENABLE_PYTHON
PYTHON_DIR = plugin-python
endif

if ENABLE_ONLINE_ACCOUNTS
ONLINE_ACCOUNTS_DIR = online-accounts
endif

if ENABLE_NETWORK_MANAGER
NETWORK_MANAGER_DIR = network-manager
endif

if ENABLE_CONNMAN
CONNMAN_DIR = connman
endif

if ENABLE_WINDOWS_SENS
WINDOWS_SENS_DIR = windows-sens
endif

SUBDIRS = \
	addressbook \
	bogofilter \
	calendar \
	mail \
	composer-autosave \
	mailto-handler \
	mdn \
	offline-alert \
	plugin-lib \
	plugin-manager \
	spamassassin \
	startup-wizard \
	$(MONO_DIR) \
	$(PYTHON_DIR) \
	$(ONLINE_ACCOUNTS_DIR) \
	$(NETWORK_MANAGER_DIR) \
	$(WINDOWS_SENS_DIR) \
	$(CONNMAN_DIR)

-include $(top_srcdir)/git.mk
