INCLUDES = \
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	$(GNOME_VFS_CFLAGS)				\
	$(UNICODE_CFLAGS)				\
	$(GTKHTML_CFLAGS)				\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DG_LOG_DOMAIN=\"evolution-executive-summary\"

EXTRA_DIST = $(IDLS)

IDLS =					\
	Executive-Summary.idl		\
	HtmlView.idl                    \
	Summary.idl			\
	SummaryComponent.idl