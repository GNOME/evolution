bin_PROGRAMS =	evolution-mail

INCLUDES = \
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(srcdir)/../widgets				\
	$(BONOBO_GNOME_CFLAGS)

evolution_mail_SOURCES =	\
	html-stream.c		\
	html-stream.h		\
	mail-display.h		\
	mail-display.c		\
	main.c

evolution_mail_LDADD = 					\
	../e-util/libeutil.a				\
	$(BONOBO_GNOME_LIBS)				
