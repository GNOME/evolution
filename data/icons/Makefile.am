NULL =

public_icons = \
	hicolor_apps_16x16_evolution.png \
	hicolor_apps_16x16_evolution-mail.png \
	hicolor_apps_16x16_evolution-memos.png \
	hicolor_apps_16x16_evolution-tasks.png \
	hicolor_apps_22x22_evolution.png \
	hicolor_apps_22x22_evolution-mail.png \
	hicolor_apps_22x22_evolution-memos.png \
	hicolor_apps_22x22_evolution-tasks.png \
	hicolor_apps_24x24_evolution.png \
	hicolor_apps_24x24_evolution-mail.png \
	hicolor_apps_24x24_evolution-memos.png \
	hicolor_apps_24x24_evolution-tasks.png \
	hicolor_apps_32x32_evolution.png \
	hicolor_apps_32x32_evolution-mail.png \
	hicolor_apps_32x32_evolution-memos.png \
	hicolor_apps_32x32_evolution-tasks.png \
	hicolor_apps_48x48_evolution.png \
	hicolor_apps_48x48_evolution-mail.png \
	hicolor_apps_48x48_evolution-memos.png \
	hicolor_apps_48x48_evolution-tasks.png \
	hicolor_apps_scalable_evolution.svg \
	$(NULL)

private_icons = \
	hicolor_actions_16x16_folder-copy.png \
	hicolor_actions_16x16_folder-move.png \
	hicolor_actions_16x16_mail-copy.png \
	hicolor_actions_16x16_mail-move.png \
 	hicolor_actions_16x16_go-today.png \
	hicolor_actions_16x16_view-calendar-day.png \
	hicolor_actions_16x16_view-calendar-list.png \
	hicolor_actions_16x16_view-calendar-month.png \
	hicolor_actions_16x16_view-calendar-week.png \
	hicolor_actions_16x16_view-calendar-workweek.png \
 	hicolor_actions_22x22_go-today.png \
	hicolor_actions_22x22_view-calendar-day.png \
	hicolor_actions_22x22_view-calendar-list.png \
	hicolor_actions_22x22_view-calendar-month.png \
	hicolor_actions_22x22_view-calendar-week.png \
	hicolor_actions_22x22_view-calendar-workweek.png \
 	hicolor_actions_24x24_go-today.png \
	hicolor_actions_24x24_query-free-busy.png \
	hicolor_actions_24x24_view-calendar-day.png \
	hicolor_actions_24x24_view-calendar-list.png \
	hicolor_actions_24x24_view-calendar-month.png \
	hicolor_actions_24x24_view-calendar-week.png \
	hicolor_actions_24x24_view-calendar-workweek.png \
	hicolor_actions_32x32_view-calendar-day.png \
	hicolor_actions_32x32_view-calendar-list.png \
	hicolor_actions_32x32_view-calendar-month.png \
	hicolor_actions_32x32_view-calendar-week.png \
	hicolor_actions_32x32_view-calendar-workweek.png \
	hicolor_actions_scalable_view-calendar-day.svg \
	hicolor_actions_scalable_view-calendar-list.svg \
	hicolor_actions_scalable_view-calendar-month.svg \
	hicolor_actions_scalable_view-calendar-week.svg \
	hicolor_actions_scalable_view-calendar-workweek.svg \
	hicolor_categories_48x48_preferences-autocompletion.png \
	hicolor_categories_48x48_preferences-calendar-and-tasks.png \
	hicolor_categories_48x48_preferences-certificates.png \
	hicolor_categories_48x48_preferences-composer.png \
	hicolor_categories_48x48_preferences-mail.png \
	hicolor_categories_48x48_preferences-mail-accounts.png \
	hicolor_categories_48x48_preferences-system-network-proxy.png \
	hicolor_places_16x16_mail-inbox.png \
	hicolor_places_16x16_mail-outbox.png \
	hicolor_places_16x16_mail-sent.png \
	hicolor_places_22x22_mail-inbox.png \
	hicolor_places_22x22_mail-outbox.png \
	hicolor_places_22x22_mail-sent.png \
	hicolor_places_24x24_mail-inbox.png \
	hicolor_places_24x24_mail-outbox.png \
	hicolor_places_24x24_mail-sent.png \
	$(NULL)

noinst_DATA = \
 	hicolor_actions_16x16_go-today.svg \
	hicolor_actions_16x16_view-calendar-day.svg \
	hicolor_actions_16x16_view-calendar-list.svg \
	hicolor_actions_16x16_view-calendar-month.svg \
	hicolor_actions_16x16_view-calendar-week.svg \
	hicolor_actions_16x16_view-calendar-workweek.svg \
 	hicolor_actions_22x22_go-today.svg \
	hicolor_actions_22x22_view-calendar-day.svg \
	hicolor_actions_22x22_view-calendar-list.svg \
	hicolor_actions_22x22_view-calendar-month.svg \
	hicolor_actions_22x22_view-calendar-week.svg \
	hicolor_actions_22x22_view-calendar-workweek.svg \
	hicolor_actions_32x32_view-calendar-day.svg \
	hicolor_actions_32x32_view-calendar-list.svg \
	hicolor_actions_32x32_view-calendar-month.svg \
	hicolor_actions_32x32_view-calendar-week.svg \
	hicolor_actions_32x32_view-calendar-workweek.svg \
	hicolor_places_16x16_mail-inbox.svg \
	hicolor_places_16x16_mail-outbox.svg \
	hicolor_places_16x16_mail-sent.svg \
	hicolor_places_22x22_mail-inbox.svg \
	hicolor_places_22x22_mail-outbox.svg \
	hicolor_places_22x22_mail-sent.svg \
 	$(NULL)

EXTRA_DIST = \
	$(public_icons)		\
	$(private_icons)	\
	$(noinst_DATA)		\
	$(NULL)

###############################################################################

gtk_update_icon_cache = gtk-update-icon-cache -f -t

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache) $(datadir)/icons/hicolor; \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache) $(datadir)/icons/hicolor"; \
	fi

install-icons:
	for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(pkgdatadir)/$(BASE_VERSION)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(pkgdatadir)/$(BASE_VERSION)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(pkgdatadir)/$(BASE_VERSION)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons update-icon-cache

uninstall-local: uninstall-icons update-icon-cache
