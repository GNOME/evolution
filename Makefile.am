changelogs =			\
	ChangeLog

EXTRA_DIST = config.rpath 			\
	AUTHORS			\
	$(changelogs)		\
	COPYING-DOCS		\
	README			\
	HACKING			\
	MAINTAINERS		\
	NEWS			\
	xml-i18n-merge.in	\
	xml-i18n-update.in	\
	xml-i18n-extract.in

SUBDIRS = m4 			\
                           \
        macros                  \
        data                    \
        e-util                  \
	libical			\
        widgets                 \
        shell                   \
        libibex                 \
        camel                   \
        filter                  \
        libversit               \
	libwombat		\
	addressbook             \
        composer                \
        mail                    \
        calendar                \
        wombat                  \
	importers		\
	my-evolution		\
        art                     \
	ui			\
        default_user            \
	views			\
        tools                   \
	doc			\
	help			\
        po			\
	omf-install

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
