INCLUDES =					\
	-I$(top_srcdir)                         \
	-I$(top_srcdir)/widgets/                \
	$(EXTRA_GNOME_CFLAGS)			\
	$(GNOME_BONOBO_CFLAGS)			\
	$(GNOME_INCLUDEDIR)			\
	$(ICONV_CFLAGS)				\
	$(GNOME_FULL_CFLAGS)			\
	$(GNOME_EXTRA_CFLAGS)			\
        -DG_LOG_DOMAIN=\"e-text\"

noinst_LTLIBRARIES = libgal-a11y-etext.la

libgal_a11y_etext_la_SOURCES =			\
	gal-a11y-e-text-factory.c		\
	gal-a11y-e-text.c

libgal_a11y_etextincludedir = $(privincludedir)/a11y/e-text

libgal_a11y_etextinclude_HEADERS =		\
	gal-a11y-e-text-factory.h		\
	gal-a11y-e-text.h
