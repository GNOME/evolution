images_DATA = \
	about-box.png			\
	bcg.png				\
	evolution-contacts-mini.png	\
	monkey-16.png			\
	offline.png			\
	online.png			\
	world_map-960.png

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/pixmaps
	$(INSTALL_DATA) $(srcdir)/evolution.png  $(DESTDIR)$(datadir)/pixmaps/evolution-$(BASE_VERSION).png
if DEFAULT_BINARY
	cd $(DESTDIR)$(datadir)/pixmaps && rm -f evolution.png && $(LN_S) evolution-$(BASE_VERSION).png evolution.png
endif

EXTRA_DIST = 				\
	ChangeLog			\
	ChangeLog.pre-1-4		\
	README				\
	broken-image-16.xpm		\
	broken-image-24.xpm		\
	empty.xpm			\
	evolution.png			\
	jump.xpm			\
	$(images_DATA)
