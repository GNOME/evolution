
man_MANS = compose.1

bin_PROGRAMS = compose

INCLUDES = 			\
	-I$(top_builddir)	\
	$(COMPOSE_CFLAGS) 	\
	$(INTLLIBS)

compose_LDADD =				\
	../composer/libcomposer.a	\
	$(COMPOSE_LIBS)

compose_SOURCES =	\
	compose.h	\
	compose.c	
	../composer/Composer-stubs.c \
	../composer/Composer-common.c