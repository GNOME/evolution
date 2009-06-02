desktopdir = $(datadir)/applications
desktop_in_in_file = evolution.desktop.in.in
desktop_file = $(desktop_in_in_file:.desktop.in.in=.desktop)
%.desktop.in: %.desktop.in.in
	sed -e "s|\@BASE_VERSION\@|$(BASE_VERSION)|" -e "s|\@DATASERVER_EXEC_VERSION\@|$(DATASERVER_EXEC_VERSION)|" $< > $@

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
		        $(DESTDIR)$(kdedesktopdir)/$(kdedesktop_file)

uninstall-kde-applnk:
	rm -rf $(DESTDIR)$(kdedesktopdir)/$(kdedesktop_file)

else

install-kde-applnk:
uninstall-kde-applnk:

endif

noinst_MANS = evolution.1
noinst_DATA = $(keys_file) $(desktop_file)

SUBDIRS = icons

install-data-local: install-kde-applnk $(desktop_file) $(keys_file) $(mime_file)
	$(mkinstalldirs) $(DESTDIR)$(desktopdir);							\
	$(INSTALL_DATA) $(desktop_file)									\
			$(DESTDIR)$(desktopdir)/$(desktop_file) ;	\
	$(mkinstalldirs) $(DESTDIR)$(keysdir);								\
	$(INSTALL_DATA) $(keys_file) $(DESTDIR)$(keysdir)/$(keys_file) ;	\
	$(mkinstalldirs) $(DESTDIR)$(mimedir);								\
	$(INSTALL_DATA) $(srcdir)/$(mime_file) $(DESTDIR)$(mimedir)/$(mime_file) ;

uninstall-local: uninstall-kde-applnk
	rm -rf $(DESTDIR)$(desktopdir)/$(desktop_file) ; 	\
	rm -rf $(DESTDIR)$(keysdir)/$(keys_file) ;	\
	rm -rf $(DESTDIR)$(mimedir)/$(mime_file) ;

CLEANFILES =					\
	$(desktop_file)				\
	$(keys_file)

EXTRA_DIST =					\
	$(desktop_in_in_file)			\
	$(mime_file)				\
	$(keys_in_file)				\
	$(noinst_MANS)

-include $(top_srcdir)/git.mk
