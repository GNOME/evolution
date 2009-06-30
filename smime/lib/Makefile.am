AM_CPPFLAGS =						\
	-DG_LOG_DOMAIN=\"evolution-smime\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CERT_UI_CFLAGS)

privsolib_LTLIBRARIES = libessmime.la

libessmime_la_SOURCES = 	\
	e-asn1-object.c		\
	e-asn1-object.h		\
	e-cert.c		\
	e-cert.h		\
	e-cert-trust.c		\
	e-cert-trust.h		\
	e-cert-db.c		\
	e-cert-db.h		\
	e-pkcs12.c		\
	e-pkcs12.h

libessmime_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la	\
	$(CERT_UI_LIBS)

libessmime_la_LDFLAGS = $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
