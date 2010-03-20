if ENABLE_MONO
MONO_DIR = plugin-mono
endif

if ENABLE_PYTHON
PYTHON_DIR = plugin-python
endif

if ENABLE_NETWORK_MANAGER
NETWORK_MANAGER_DIR = network-manager
endif

SUBDIRS = \
	addressbook \
	calendar \
	mail \
	plugin-lib \
	$(MONO_DIR) \
	$(PYTHON_DIR) \
	$(NETWORK_MANAGER_DIR)

-include $(top_srcdir)/git.mk
