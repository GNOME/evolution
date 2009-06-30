AM_CPPFLAGS =					\
	-I$(top_srcdir)                         \
	-I$(top_srcdir)/widgets                 \
	$(GNOME_PLATFORM_CFLAGS)		\
        -DG_LOG_DOMAIN=\"e-text\"

noinst_LTLIBRARIES = libgal-a11y-etext.la

libgal_a11y_etext_la_SOURCES =			\
	gal-a11y-e-text-factory.c		\
	gal-a11y-e-text.c			\
	gal-a11y-e-text-factory.h		\
	gal-a11y-e-text.h

-include $(top_srcdir)/git.mk
