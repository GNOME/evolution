icon16dir = $(datadir)/icons/hicolor/16x16/apps
icon16_DATA = 16x16/evolution.png

icon22dir = $(datadir)/icons/hicolor/22x22/apps
icon22_DATA = 22x22/evolution.png

icon24dir = $(datadir)/icons/hicolor/24x24/apps
icon24_DATA = 24x24/evolution.png

icon32dir = $(datadir)/icons/hicolor/32x32/apps
icon32_DATA = 32x32/evolution.png

svgicondir = $(datadir)/icons/hicolor/scalable/apps
svgicon_DATA = scalable/evolution.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

images_DATA = \
	evolution-contacts-mini.png	\
	offline.png			\
	online.png			\
	world_map-960.png		\
	query-free-busy.png		\
	plus.png			\
	minus.png

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = 				\
	ChangeLog			\
	ChangeLog.pre-1-4		\
	README				\
	broken-image-16.xpm		\
	broken-image-24.xpm		\
	empty.xpm			\
	jump.xpm			\
	$(icon16_DATA)			\
	$(icon22_DATA)			\
	$(icon24_DATA)			\
	$(icon32_DATA)			\
	$(svgicon_DATA)			\
	$(images_DATA)
