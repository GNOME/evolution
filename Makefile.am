changelogs =			\
	ChangeLog

appicondir = $(datadir)/pixmaps
appicon_DATA = evolution.png

applicationdir = $(datadir)/gnome/apps/Applications
application_in_files = evolution.desktop.in
application_DATA = $(application_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

EXTRA_DIST =			\
	AUTHORS			\
	$(changelogs)		\
	COPYING-DOCS		\
	README			\
	HACKING			\
	MAINTAINERS		\
	NEWS			\
	$(appicon_DATA)		\
	$(application_in_files)	\
  	xml-i18n-merge.in	\
  	xml-i18n-update.in	\
  	xml-i18n-extract.in

SUBDIRS =			\
        intl                    \
        macros                  \
        data                    \
        e-util                  \
        widgets                 \
        shell                   \
	executive-summary	\
        libibex                 \
        camel                   \
        filter                  \
        composer                \
        mail                    \
        libical                 \
        libversit               \
        addressbook             \
        calendar                \
        wombat                  \
        art                     \
	ui			\
        default_user            \
        tools                   \
        po
