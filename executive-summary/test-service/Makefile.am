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

#test_bonobo_service_SOURCES =		\
#	test-bonobo.c

#test_bonobo_service_LDADD =		\
#        $(top_builddir)/executive-summary/evolution-services/libevolution-services.la   \
#        $(BONOBO_VFS_GNOME_LIBS)        \
#        $(EXTRA_GNOME_LIBS)

#rdf_summary_SOURCES =			\
#	rdf-summary.c

#rdf_summary_LDADD =			\
#	$(top_builddir)/executive-summary/evolution-services/libevolution-services.la	\
#	$(BONOBO_VFS_GNOME_LIBS)	\
#	$(EXTRA_GNOME_LIBS)

oafdir = $(datadir)/oaf
oaf_DATA = \
	GNOME_Evolution_Summary_test.oafinfo \
	GNOME_Evolution_Summary_rdf.oafinfo

EXTRA_DIST = $(oaf_DATA) 
