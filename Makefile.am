changelogs =			\
	ChangeLog

confexecdir = $(libdir)
confexec_DATA = 			\
	camelConf.sh			\
	evolution_shellConf.sh		\
	evolution_addressbookConf.sh	\
	evolution_calendarConf.sh

EXTRA_DIST =			\
	AUTHORS			\
	$(changelogs)		\
	COPYING-DOCS		\
	README			\
	HACKING			\
	MAINTAINERS		\
	NEWS			\
	NEWS-1.0		\
	xml-i18n-merge.in	\
	xml-i18n-update.in	\
	xml-i18n-extract.in	\
	$(confexec_DATA:.sh=.sh.in)

SUBDIRS =			\
        data                    \
        e-util                  \
	libical			\
        widgets                 \
	calendar		\
        art                     \
	ui			\
        default_user            \
	views			\
        tools

#	omf-install

%Conf.sh: %Conf.sh.in Makefile
	sed -e 's?\@EVOLUTION_LIBDIR\@?$(EVOLUTION_LIBDIR)?g' \
	    -e 's?\@EVOLUTION_DATADIR\@?$(EVOLUTION_DATADIR)?g' \
	    -e 's?\@EVOLUTION_INCLUDEDIR\@?$(EVOLUTION_INCLUDEDIR)?g' \
	    -e 's?\@CAMEL_INCLUDEDIR\@?$(CAMEL_INCLUDEDIR)?g' \
	    -e 's?\@VERSION\@?$(VERSION)?g' \
	    -e 's?\@privlibdir\@?$(privlibdir)?g' \
	    -e 's?\@SHELL_LIBS\@?$(SHELL_LIBS)?g' \
	    -e 's?\@SHELL_CFLAGS\@?$(SHELL_CFLAGS)?g' \
	    -e 's?\@CAMEL_LIBS\@?$(CAMEL_LIBS)?g' \
	    -e 's?\@CAMEL_CFLAGS\@?$(CAMEL_CFLAGS)?g' \
	    -e 's?\@LDAP_LIBS\@?$(LDAP_LIBS)?g' \
	    -e 's?\@LDAP_CFLAGS\@?$(LDAP_CFLAGS)?g' \
	    -e 's?\@EVOLUTION_ADDRESSBOOK_LIBS\@?$(EVOLUTION_ADDRESSBOOK_LIBS)?g' \
	    -e 's?\@EVOLUTION_ADDRESSBOOK_CFLAGS\@?$(EVOLUTION_ADDRESSBOOK_CFLAGS)?g' \
	    -e 's?\@EVOLUTION_CALENDAR_LIBS\@?$(EVOLUTION_CALENDAR_LIBS)?g' \
	    -e 's?\@EVOLUTION_CALENDAR_CFLAGS\@?$(EVOLUTION_CALENDAR_CFLAGS)?g' \
	      < $(srcdir)/$< > $@.tmp \
	  && mv $@.tmp $@

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
