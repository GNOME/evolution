
OBJS = filter-arg-types.o filter-arg.o filter-xml.o filter-sexp.o filter-driver.o
SRCS = filter-arg-types.c filter-arg.c filter-xml.c filter-sexp.c filter-driver.c

CFLAGS = `gnome-config --cflags xml gnome gtk gtkhtml gnomeui` -g -I../camel -I .. -I../libibex
LDFLAGS = `gnome-config --libs xml gnome gtk gtkhtml gnomeui` -L ../camel/.libs -lcamel

all: filter-driver

filter-driver: $(OBJS)

