CC      = gcc
PROF    = -g
C_FLAGS =  -Wall $(PROF) -L/usr/local/include -DDEBUG -I../apps -I../lib
L_FLAGS =  $(PROF) -L/usr/X11R6/lib -L/usr/local/lib 
L_POSTFLAGS = -lgnomeui -lgnome -lgtk -lgdk -lglib -lXext -lX11 -lm
PROGNAME = gncal

O_FILES = gncal.o menus.o calcs.o

$(PROGNAME): $(O_FILES)
	rm -f $(PROGNAME)
	$(CC) $(L_FLAGS) -o $(PROGNAME) $(O_FILES) $(L_POSTFLAGS)

.c.o: 
	$(CC) -c $(C_FLAGS) $<

clean: 
	rm -f core *.o $(PROGNAME) nohup.out
distclean: clean 
	rm -f *~
