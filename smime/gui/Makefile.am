INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-smime\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/smime/lib			\
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
	-DLIBGNOME_DISABLE_DEPRECATED			\
	-DLIBGNOMEUI_DISABLE_DEPRECATED			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CERT_UI_CFLAGS)

noinst_LTLIBRARIES = libevolution-smime.la

libevolution_smime_la_SOURCES = 	\
	certificate-manager.c		\
	certificate-manager.h		\
	certificate-viewer.c		\
	certificate-viewer.h		\
	e-cert-selector.c		\
	e-cert-selector.h

libevolution_smime_la_LIBADD =			\
	$(top_builddir)/smime/lib/libessmime.la	\
	$(CERT_UI_LIBS)


glade_DATA = smime-ui.glade

EXTRA_DIST =			\
	$(glade_DATA)
