INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-smime\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_builddir)/smime/lib			\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CERT_UI_CFLAGS)

noinst_LTLIBRARIES = libevolution-smime.la

libevolution_smime_la_SOURCES = 	\
	ca-trust-dialog.c		\
	ca-trust-dialog.h		\
	cert-trust-dialog.c		\
	cert-trust-dialog.h		\
	certificate-manager.c		\
	certificate-manager.h		\
	certificate-viewer.c		\
	certificate-viewer.h		\
	e-cert-selector.c		\
	e-cert-selector.h		\
	component.c			\
	component.h


libevolution_smime_la_LIBADD =			\
	$(top_builddir)/smime/lib/libessmime.la	\
	$(CERT_UI_LIBS)


glade_DATA = smime-ui.glade

EXTRA_DIST =			\
	$(glade_DATA)
