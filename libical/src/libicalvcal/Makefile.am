privlib_LTLIBRARIES = libicalvcal-evolution.la

INCLUDES =			\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/src

libicalvcal_evolution_la_LDFLAGS = -version-info 0:0:0

if DEV
libicalvcal_evolution_la_DEVSOURCES = vcc.y
else
libicalvcal_evolution_la_DEVSOURCES = vcc.c
endif


libicalvcal_evolution_la_SOURCES =	\
	icalvcal.h		\
	icalvcal.c		\
	port.h			\
	vcc.h			\
	vobject.h		\
	vobject.c		\
	vcaltmp.h		\
	vcaltmp.c		\
	$(libicalvcal_evolution_la_DEVSOURCES)

libicalvcal_evolutionincludedir = $(privincludedir)/libical

libicalvcal_evolutioninclude_HEADERS =	\
	icalvcal.h		\
	port.h			\
	vcc.h			\
	vobject.h		\
	vcaltmp.h

EXTRA_DIST =		\
	README.TXT	\
	vcaltest.c	\
	vctest.c
