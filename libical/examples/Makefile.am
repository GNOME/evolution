
noinst_PROGRAMS = doesnothing

LDADD = ../src/libical/libical.a ../src/libicalss/libicalss.a ../src/libicalvcal/libicalvcal.a
INCLUDES = -I . -I../src/libical -I$(srcdir)/../src/libical -I../src/libicalss -I../src/libicalvcal

doesnothing_SOURCES = \
	access_components.c \
	access_properties_and_parameters.c \
	errors.c \
	main.c \
	parse_text.c 

