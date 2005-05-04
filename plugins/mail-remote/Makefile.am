

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

idl_DATA = $(IDL)

_SOURCES = \
	$(IDL_GENERATED_C)			\
	$(IDL_GENERATED_H)			\
	evolution-mail-folder.c			\
	evolution-mail-folder.h			\
	evolution-mail-session.c		\
	evolution-mail-session.h		\
	evolution-mail-store.c			\
	evolution-mail-store.h

EXTRA_DIST = 				\
	$(IDL)

BUILT_SOURCES = $(IDL_GENERATED_H) $(IDL_GENERATED_C)
