changelogs =				\
	ChangeLog			\
	ChangeLog.pre-1-4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 				\
	evolution-shell.pc	\
	evolution-plugin.pc

EXTRA_DIST = 			\
	AUTHORS			\
	$(changelogs)		\
	COPYING-DOCS		\
	README			\
	HACKING			\
	MAINTAINERS		\
	NEWS			\
	NEWS-1.0		\
	marshal.mk		\
	iconv-detect.c		\
	plugin.mk		\
	server.mk		\
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	evolution-zip.in	\
	gnome-doc-utils.make    \
	$(pkgconfig_DATA:.pc=.pc.in)

if ENABLE_SMIME
SMIME_DIR=smime
endif

SUBDIRS = 			\
	win32			\
        data                    \
        e-util                  \
	a11y			\
        widgets                 \
        shell                   \
	filter			\
	$(SMIME_DIR)		\
	addressbook		\
	calendar		\
        art                     \
	composer		\
	mail			\
	plugins			\
	ui			\
	views			\
	tools			\
	help			\
	po

DISTCLEANFILES = 		\
	intltool-extract	\
	intltool-merge 		\
	intltool-update 	\
	iconv-detect.h 		\
	gnome-doc-utils.make 	\
	$(pkgconfig_DATA)

# Scrollkeeper must be disabled for distcheck to work because
# distcheck uses a custom prefix.  See GNOME bug #354357.
DISTCHECK_CONFIGURE_FLAGS =		\
	--disable-scrollkeeper		\
	--enable-test-component

# CDE Stuff

if HAVE_DTAPPINTEGRATE

# This installs Evolution into the Application Manager on CDE
dtappintegrate:
	( cd data && make dtappintegrate )

# ...And this uninstalls it
dtappunintegrate:
	( cd data ; make dtappunintegrate )

# Install/uninstall for CDE
cde-install: install dtappintegrate
cde-uninstall: dtappunintegrate uninstall

endif
