
noinst_PROGRAMS = copycluster regression parser findobj storage stow recur testmime

LDADD = ../libical/libical.a ../libicalss/libicalss.a ../libicalvcal/libicalvcal.a 
INCLUDES = \
	-I . \
	-I $(srcdir) \
	-I../libical \
	-I $(srcdir)/../libical \
	-I../libicalss \
	-I $(srcdir)/../libicalss \
	-I../libicalvcal \
	-I $(srcdir)/../libicalvcal

findobj_SOURCES = findobj.c
copycluster_SOURCES = copycluster.c
regression_SOURCES = regression.c
parser_SOURCES = icaltestparser.c
storage_SOURCES = storage.c
stow_SOURCES = stow.c
testvcal_SOURCES =testvcal.c
recur_SOURCES = recur.c 
test_mime = testmime.c
