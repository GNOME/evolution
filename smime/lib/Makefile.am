privsolib_LTLIBRARIES = libessmime.la

libessmime_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"evolution-smime\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DPREFIX=\""$(prefix)"\"			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)				\
	$(CERT_UI_CFLAGS)

libessmime_la_SOURCES = 	\
	e-cert.c		\
	e-cert.h		\
	e-cert-trust.c		\
	e-cert-trust.h		\
	e-cert-db.c		\
	e-cert-db.h		\
	e-pkcs12.c		\
	e-pkcs12.h

libessmime_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la \
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)			\
	$(GTKHTML_LIBS)				\
	$(CERT_UI_LIBS)

libessmime_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
