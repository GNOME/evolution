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
	-DG_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED			\
	-DLIBGNOME_DISABLE_DEPRECATED			\
	-DLIBGNOMEUI_DISABLE_DEPRECATED			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CERT_UI_CFLAGS)

noinst_LTLIBRARIES = libessmime.la

libessmime_la_SOURCES = 	\
	e-cert.c		\
	e-cert.h		\
	e-cert-trust.c		\
	e-cert-trust.h		\
	e-cert-db.c		\
	e-cert-db.h
