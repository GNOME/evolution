bin_PROGRAMS =	evolution-mail

INCLUDES = \
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(srcdir)/../widgets				\
	$(BONOBO_GNOME_CFLAGS)

EVOLUTION_CORBA_GENERATED = 	\
	Mail.h			\
	Mail-common.c		\
	Mail-skels.c		\
	Mail-stubs.c

evolution_mail_SOURCES =	\
	html-stream.c		\
	html-stream.h		\
	mail-display.h		\
	mail-display.c		\
	main.c			\
	message-list.c		\
	message-list.h		\
	$(EVOLUTION_CORBA_GENERATED)

evolution_mail_LDADD = 					\
	../e-util/libeutil.a				\
	$(BONOBO_GNOME_LIBS)				

$(EVOLUTION_MAIL_CORBA_GENERATED): Mail.idl 
	orbit-idl -I`$(GNOME_CONFIG) --datadir`/idl -I$(srcdir) $(srcdir)/Mail.idl

EXTRA_DIST = Mail.idl
