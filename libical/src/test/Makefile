# Generated automatically from Makefile.in by configure.
INCLUDES = -I . -I../libical -I../libicalss
CFLAGS =  -Wall -g  $(INCLUDES) 
LIBS   =  -L../libical -L../libicalss -licalss -lical
CC	= gcc
LD	= ld

all: usecases copycluster regression parser findobj

parser: icaltestparser.o ../libical/libical.a ../libicalss/libicalss.a
	$(CC) -o icalparser icaltestparser.o $(LIBS)

copycluster: copycluster.o ../libical/libical.a ../libicalss/libicalss.a
	$(CC)  -o copycluster copycluster.o $(INCLUDES)  $(LIBS)	

findobj: findobj.o ../libical/libical.a ../libicalss/libicalss.a 
	$(CC)  -o findobj findobj.o $(INCLUDES)  $(LIBS)	

regression: regression.o ../libical/libical.a ../libicalss/libicalss.a
	$(CC)  -o regression regression.o $(INCLUDES)  $(LIBS)	

usecases: usecases.o ../libical/libical.a ../libicalss/libicalss.a
	$(CC)  -o usecases usecases.o $(INCLUDES)  $(LIBS)	

clean:
	-/bin/rm -f *.o
	-/bin/rm -f \#*
	-/bin/rm -f *~ Makefile.bak icalitip.tab.h icalitip.tab.c lex.yy.c
	-/bin/rm -rf icalparser core regression usecases copycluster findobj

ci: clean
	ci -u *.c *.h 

depend:
	@makedepend  -Y $(INCLUDES) $(SOURCES)

install: