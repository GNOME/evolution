bin_PROGRAMS =	evolution-mail test-mail

providerdir = $(libdir)/evolution/camel-providers/$(VERSION)

#imagesdir = $(datadir)/images/evolution
#images_DATA = e-attchmt.png
#EXTRA_DIST = $(image_DATA)

INCLUDES = \
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/camel				\
	-I$(top_builddir)/shell				\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"	\
	-DG_LOG_DOMAIN=\"evolution-mail\"

EVOLUTION_MAIL_CORBA_GENERATED = \
	Mail.h			\
	Mail-common.c		\
	Mail-skels.c		\
	Mail-stubs.c

evolution_mail_SOURCES =		\
	$(EVOLUTION_MAIL_CORBA_GENERATED)	\
	folder-browser.c		\
	folder-browser.h		\
	folder-browser-factory.c	\
	mail-config.c			\
	mail-display.c			\
	mail-display.h			\
	mail-format.c			\
	mail-identify.c			\
	mail-ops.c			\
	mail-types.h			\
	main.c				\
	message-list.c			\
	message-list.h			\
	session.c			\
	mail.h				\
	pixmaps.h


evolution_mail_LDADD = 					\
	../shell/Evolution-common.o			\
	../shell/Evolution-stubs.o			\
	../shell/Evolution-skels.o			\
	../shell/evolution-service-repository.o 	\
	../composer/libcomposer.la		 	\
	../widgets/e-table/libetable.a			\
	../widgets/e-text/libetext.a			\
	../camel/libcamel.la				\
	../e-util/libeutil.la				\
	../libibex/libibex.la				\
	../filter/libfilter.la				\
	$(BONOBO_HTML_GNOME_LIBS)			\
	$(UNICODE_LIBS)

test_mail_SOURCES = \
	test-mail.c

test_mail_LDADD = \
	$(BONOBO_HTML_GNOME_LIBS)


GOAD_FILES = evolution-mail.gnorba
OAF_FILES = evolution-mail.oafinfo

if USING_OAF
oafdir = $(datadir)/oaf
oaf_DATA = $(OAF_FILES)
else
gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = $(GOAD_FILES)
endif

$(EVOLUTION_MAIL_CORBA_GENERATED): Mail.idl 
	$(ORBIT_IDL) -I$(datadir)/idl -I`$(GNOME_CONFIG) --datadir`/idl -I$(srcdir) $(srcdir)/Mail.idl

EXTRA_DIST = Mail.idl $(GOAD_FILES) $(OAF_FILES)

dist-hook:
	-mkdir $(distdir)/pixmaps
	cp $(srcdir)/pixmaps/*.xpm $(distdir)/pixmaps

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-mail.pure

evolution-mail.pure: evolution-mail
	@rm -f evolution-mail.pure
	$(PLINK) $(evolution_mail_LDFLAGS) $(evolution_mail_OBJECTS) $(evolution_mail_LDADD) $(LIBS)

endif

