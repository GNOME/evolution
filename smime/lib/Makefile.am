INCLUDES =						\
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

noinst_LTLIBRARIES = libessmime.la

libessmime_la_SOURCES = 	\
	$(MARSHAL_GENERATED)	\
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

MARSHAL_GENERATED = smime-marshal.c smime-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = smime-marshal.list
