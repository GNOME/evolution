bin_PROGRAMS =	evolution-mail test-mail

INCLUDES = \
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(srcdir)/../widgets				\
	$(BONOBO_HTML_GNOME_CFLAGS)

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
	message-list.h

evolution_mail_LDADD = 					\
	../widgets/e-table/libetable.a			\
	../camel/libcamel.la				\
	../e-util/libeutil.a				\
	$(BONOBO_HTML_GNOME_LIBS)				

test_mail_SOURCES = \
	test-mail.c

test_mail_LDADD = \
	$(BONOBO_HTML_GNOME_LIBS)

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = evolution-mail.gnorba

$(EVOLUTION_MAIL_CORBA_GENERATED): Mail.idl 
	orbit-idl -I`$(GNOME_CONFIG) --datadir`/idl -I$(srcdir) $(srcdir)/Mail.idl

EXTRA_DIST = Mail.idl evolution-mail.gnorba
