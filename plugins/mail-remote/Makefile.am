
INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-mail-remote.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-mail-remote.la

liborg_gnome_evolution_mail_remote_la_SOURCES = \
	$(IDL_GENERATED_C)			\
	$(IDL_GENERATED_H)			\
	e-corba-utils.c				\
	e-corba-utils.h				\
	evolution-mail-folder.c			\
	evolution-mail-folder.h			\
	evolution-mail-listener.c		\
	evolution-mail-listener.h		\
	evolution-mail-messageiterator.c	\
	evolution-mail-messageiterator.h	\
	evolution-mail-session.c		\
	evolution-mail-session.h		\
	evolution-mail-store.c			\
	evolution-mail-store.h			\
	mail-remote.c

liborg_gnome_evolution_mail_remote_la_LDFLAGS = -module -avoid-version

client_SOURCES = \
	client.c

client_LDADD = \
	$(EVOLUTION_MAIL_LIBS)			\
	evolution-mail-listener.o		\
	Evolution-DataServer-Mail-common.o	\
	Evolution-DataServer-Mail-stubs.o	\
	Evolution-DataServer-Mail-skels.o

noinst_PROGRAMS =				\
	client

# mail idl

IDL = Evolution-DataServer-Mail.idl

IDL_GENERATED_H = \
	Evolution-DataServer-Mail.h

IDL_GENERATED_C = \
	Evolution-DataServer-Mail-common.c	\
	Evolution-DataServer-Mail-skels.c	\
	Evolution-DataServer-Mail-stubs.c

$(IDL_GENERATED_H) $(IDL_GENERATED_C): $(IDL)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl $(IDL_INCLUDES) $^

# installed idls

#idl_DATA = $(IDL)

EXTRA_DIST = 				\
	$(IDL)

BUILT_SOURCES = $(IDL_GENERATED_H) $(IDL_GENERATED_C)
CLEANFILES = $(BUILT_SOURCES)
