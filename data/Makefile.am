desktopdir = $(datadir)/applications
desktop_in_in_file = evolution.desktop.in.in
desktop_file = $(desktop_in_in_file:.desktop.in.in=.desktop)
%.desktop.in: %.desktop.in.in
	sed -e "s|\@BASE_VERSION\@|$(BASE_VERSION)|" $< > $@

@INTLTOOL_DESKTOP_RULE@

keysdir = $(datadir)/mime-info
keys_in_file = evolution.keys.in.in
keys_file = $(keys_in_file:.keys.in.in=.keys) 
%.keys.in: %.keys.in.in
	sed -e "s|\@BASE_VERSION\@|$(BASE_VERSION)|" $< > $@

@INTLTOOL_KEYS_RULE@

mimedir = $(datadir)/mime-info
mime_file = evolution.mime

if HAVE_KDE_APPLNK

kdedesktopdir = $(KDE_APPLNK_DIR)/Applications
kdedesktop_file = $(desktop_in_in_file:.desktop.in.in=.desktop)

install-kde-applnk:
	$(mkinstalldirs) $(DESTDIR)$(kdedesktopdir);	\
	$(INSTALL_DATA) $(kdedesktop_file) \
		        $(DESTDIR)$(kdedesktopdir)/$(kdedesktop_file:.desktop=-$(BASE_VERSION).desktop)
else

install-kde-applnk:

endif

noinst_MANS = evolution.1
noinst_DATA = $(keys_file) $(desktop_file)

SUBDIRS = cde_app_root

install-data-local: install-kde-applnk $(desktop_file) $(keys_file) $(mime_file)
	$(mkinstalldirs) $(DESTDIR)$(desktopdir);							\
	$(INSTALL_DATA) $(desktop_file)									\
			$(DESTDIR)$(desktopdir)/$(desktop_file:.desktop=-$(BASE_VERSION).desktop) ;	\
	$(mkinstalldirs) $(DESTDIR)$(keysdir);								\
	$(INSTALL_DATA) $(keys_file) $(DESTDIR)$(keysdir)/$(keys_file:.keys=-$(BASE_VERSION).keys) ;	\
	$(mkinstalldirs) $(DESTDIR)$(mimedir);								\
	$(INSTALL_DATA) $(srcdir)/$(mime_file) $(DESTDIR)$(mimedir)/$(mime_file:.mime=-$(BASE_VERSION).mime) ;

# CDE Stuff

if HAVE_DTAPPINTEGRATE

# This installs Evolution into the Application Manager on CDE
dtappintegrate:
	$(DTAPPINTEGRATE) -s $(privdatadir)/cde_app_root

# ...And this uninstalls it
dtappunintegrate:
	$(DTAPPINTEGRATE) -u -s $(privdatadir)/cde_app_root

endif

EXTRA_DIST =					\
	$(desktop_in_in_file)			\
	$(mime_file)				\
	$(keys_file)				\
	$(keys_in_file)				\
	$(noinst_MANS)

