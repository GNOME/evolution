bin_PROGRAMS = test-service

CFLAGS += -g 
INCLUDES = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/executive-summary	\
	$(EXTRA_GNOME_CFLAGS)		\
	$(BONOBO_HTML_GNOME_CFLAGS)	\
	$(UNICODE_CFLAGS)

test_service_SOURCES = 			\
	main.c

test_service_LDADD = 			\
	$(top_builddir)/executive-summary/evolution-services/libevolution-services.la	\
	$(BONOBO_VFS_GNOME_LIBS)	\
	$(EXTRA_GNOME_LIBS)
oafdir = $(datadir)/oaf
oaf_DATA = test-service.oafinfo

EXTRA_DIST = $(oaf_DATA) 
