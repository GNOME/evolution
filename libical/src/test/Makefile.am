
noinst_PROGRAMS = usecases copycluster regression parser findobj storage stow

LDADD = ../libical/libical.a ../libicalss/libicalss.a
INCLUDES = \
	-I . \
	-I $(srcdir) \
	-I../libical \
	-I $(srcdir)/../libical \
	-I../libicalss \
	-I $(srcdir)/../libicalss

findobj_SOURCES = findobj.c
usecases_SOURCES = usecases.c
copycluster_SOURCES = copycluster.c
regression_SOURCES = regression.c
parser_SOURCES = icaltestparser.c
storage_SOURCES = storage.c
stow_SOURCES = stow.c