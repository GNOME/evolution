changelogs =				\
	ChangeLog			\
	ChangeLog.pre-1-4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 			\
	camel.pc			\
	evolution-shell.pc		\
	evolution-addressbook.pc	\
	evolution-calendar.pc

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
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	$(pkgconfig_DATA:.pc=.pc.in)

SUBDIRS = 			\
        data                    \
        e-util                  \
	libical			\
	libversit		\
	a11y			\
        widgets                 \
        shell                   \
	camel			\
	filter			\
	addressbook		\
	calendar		\
        art                     \
	composer		\
	mail			\
	ui			\
        default_user            \
	views			\
	wombat			\
	tools			\
	help			\
	po


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

ACLOCAL_AMFLAGS = -I m4
