bin_PROGRAMS =	evolution-mail test-mail test-sources

providerdir = $(libdir)/evolution/camel-providers/$(VERSION)

EXTRA_DIST = e-attchmt.png
pkgdata_DATA = e-attchmt.png

INCLUDES = \
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"

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
	folder-browser-factory.h	\
	html-stream.c			\
	html-stream.h			\
	mail-display.h			\
	mail-display.c			\
	main.c				\
	message-list.c			\
	message-list.h			\
	session.c			\
	session.h			


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
	$(BONOBO_HTML_GNOME_LIBS)			\
        -lunicode

test_mail_SOURCES = \
	test-mail.c

test_mail_LDADD = \
	$(BONOBO_HTML_GNOME_LIBS)


test_sources_SOURCES = \
	mail-sources.c

test_sources_LDADD = \
	$(BONOBO_HTML_GNOME_LIBS)

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = evolution-mail.gnorba

$(EVOLUTION_MAIL_CORBA_GENERATED): Mail.idl 
	orbit-idl -I`$(GNOME_CONFIG) --datadir`/idl -I$(srcdir) $(srcdir)/Mail.idl

EXTRA_DIST = Mail.idl evolution-mail.gnorba
