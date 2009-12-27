privsolib_LTLIBRARIES = libevolution-smime.la

libevolution_smime_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"evolution-smime\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_builddir)/smime/lib			\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)/widgets			\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DPREFIX=\""$(prefix)"\"			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CERT_UI_CFLAGS)

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


libevolution_smime_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/smime/lib/libessmime.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(GNOME_PLATFORM_LIBS)				\
	$(CERT_UI_LIBS)

libevolution_smime_la_LDFLAGS = $(NO_UNDEFINED)

ui_DATA = smime-ui.ui

EXTRA_DIST =			\
	$(ui_DATA)

-include $(top_srcdir)/git.mk
