set(HELP_ID ${PROJECT_NAME})

set(HELP_FIGURES_DIR figures)

set(HELP_MEDIA
	${HELP_FIGURES_DIR}/color-000000.png
	${HELP_FIGURES_DIR}/color-204a87.png
	${HELP_FIGURES_DIR}/color-2e3436.png
	${HELP_FIGURES_DIR}/color-4e9a06.png
	${HELP_FIGURES_DIR}/color-5c3566.png
	${HELP_FIGURES_DIR}/color-8f5902.png
	${HELP_FIGURES_DIR}/color-a40000.png
	${HELP_FIGURES_DIR}/color-c4a000.png
	${HELP_FIGURES_DIR}/color-ccccccffffff.png
	${HELP_FIGURES_DIR}/color-ce5c00.png
	${HELP_FIGURES_DIR}/evolution.svg
	${HELP_FIGURES_DIR}/html-composer-insert-image.png
	${HELP_FIGURES_DIR}/html-composer-insert-link.png
	${HELP_FIGURES_DIR}/html-composer-insert-rule.png
	${HELP_FIGURES_DIR}/html-composer-insert-table.png
	${HELP_FIGURES_DIR}/minus-icon.png
	${HELP_FIGURES_DIR}/new-mail-switcher.png
	${HELP_FIGURES_DIR}/plus-icon.png
	${HELP_FIGURES_DIR}/search-icon.png
	${HELP_FIGURES_DIR}/window-overview-layers.png
)

set(HELP_FILES
	backup-restore.page
	calendar-alarms-and-reminders.page
	calendar-caldav.page
	calendar-classifications.page
	calendar-free-busy.page
	calendar-google.page
	calendar-layout-appointment-display.page
	calendar-layout-general-formatting.page
	calendar-layout.page
	calendar-layout-views.page
	calendar-local.page
	calendar-marcus-bains-line.page
	calendar-meetings-delegating.page
	calendar-meetings.page
	calendar-meetings-replying-to-invitation.page
	calendar-meetings-sending-invitation.page
	calendar-organizing.page
	calendar-publishing.page
	calendar-recurrence.page
	calendar-searching.page
	calendar-sharing-information.page
	calendar-timezones.page
	calendar-usage-add-appointment.page
	calendar-usage-delete-appointment.page
	calendar-usage-edit-appointment.page
	calendar-usage.page
	calendar-using-several-calendars.page
	calendar-weather.page
	calendar-webdav.page
	contacts-add-automatically.page
	contacts-autocompletion.page
	contacts-carddav.page
	contacts-google.page
	contacts-ldap.page
	contacts-libreoffice.page
	contacts-local.page
	contacts-organizing.page
	contacts-searching.page
	contacts-usage-add-contact.page
	contacts-usage-delete-contact.page
	contacts-usage-edit-contact.page
	contacts-usage.page
	contacts-using-contact-lists.page
	contacts-using-several-addressbooks.page
	credits.page
	customize-ui-items.page
	data-storage.page
	default-browser.page
	deleting-appointments.page
	deleting-emails.page
	deleting-to-free-disk-space.page
	exporting-data-calendar.page
	exporting-data-contacts.page
	exporting-data-mail.page
	exporting-data.page
	flatpak-limitations.page
	google-services.page
	import-apps-mozilla.page
	import-apps-kmail.page
	import-apps-outlook.page
	import-data.page
	import-single-files.page
	import-supported-file-formats.page
	index.page
	intro-account-types.page
	intro-first-run.page
	intro-keyboard-shortcuts.page
	intro-main-window.page
	legal.xml
	mail-access-gmail-imap-account.page
	mail-access-gmail-pop-account.page
	mail-account-manage-imap-plus.page
	mail-account-manage-local-delivery.page
	mail-account-manage-maildir-format-directories.page
	mail-account-management.page
	mail-account-manage-mh-format-directories.page
	mail-account-manage-microsoft-exchange-evo-ews.page
	mail-account-manage-microsoft-exchange-evo-mapi.page
	mail-account-manage-microsoft-exchange.page
	mail-account-manage-pop.page
	mail-account-manage-unix-mbox-spool-directory.page
	mail-account-manage-unix-mbox-spool-file.page
	mail-account-manage-usenet-news.page
	mail-attachments.page
	mail-attachments-received.page
	mail-attachments-sending.page
	mail-calendar-sending-invitations.page
	mail-cannot-see.page
	mail-change-columns-in-message-list.page
	mail-change-time-format.page
	mail-composer-change-quotation-string.page
	mail-composer-custom-header-lines.page
	mail-composer-enable-html-format.page
	mail-composer-external-editor.page
	mail-composer-forward.page
	mail-composer-forward-as-attachment.page
	mail-composer-from-field-override.page
	mail-composer-html-image.page
	mail-composer-html-link.page
	mail-composer-html.page
	mail-composer-html-rule.page
	mail-composer-html-table.page
	mail-composer-html-text.page
	mail-composer-mail-signatures-manage.page
	mail-composer-mail-signatures.page
	mail-composer-mail-signatures-per-account.page
	mail-composer-message-templates.page
	mail-composer-message-templates-reply.page
	mail-composer-message-templates-save.page
	mail-composer-message-templates-variables.page
	mail-composer-plain-text.page
	mail-composer-priority.page
	mail-composer-reply.page
	mail-composer-search.page
	mail-composer-send-account-overrides.page
	mail-composer-several-recipients.page
	mail-composer-spellcheck.page
	mail-composer-write-new-message.page
	mail-default-CC-and-BCC.page
	mail-default-folder-locations.page
	mail-delete-and-undelete.page
	mail-displaying-character-encodings.page
	mail-displaying-collapsible-headers.page
	mail-displaying-images-in-html.page
	mail-displaying-message.page
	mail-displaying-sender-photograph.page
	mail-display-message-source.page
	mail-duplicates.page
	mail-encryption-gpg-create-key.page
	mail-encryption-gpg-decrypting.page
	mail-encryption-gpg-getting-keys.page
	mail-encryption-gpg-set-up.page
	mail-encryption-gpg-signing-encrypting.page
	mail-encryption.page
	mail-encryption-s-mime-manage.page
	mail-encryption-s-mime-signing-encrypting.page
	mail-filters-actions.page
	mail-filters-conditions.page
	mail-filters-not-working.page
	mail-filters.page
	mail-folders.page
	mail-follow-up-flag.page
	mail-imap-change-send-method.page
	mail-imap-subscriptions.page
	mail-labels.page
	mail-layout-changing.page
	mail-localized-re-subjects.page
	mail-moving-emails.page
	mail-not-sent.page
	mail-organizing.page
	mail-read-receipts.page
	mail-received-notification.page
	mail-receiving-options-exchange-ews.page
	mail-receiving-options-exchange-mapi.page
	mail-receiving-options-imap-plus.page
	mail-receiving-options-local-delivery.page
	mail-receiving-options-maildir-format-directories.page
	mail-receiving-options-mh-format-directories.page
	mail-receiving-options.page
	mail-receiving-options-pop.page
	mail-receiving-options-unix-mbox-spool-directory.page
	mail-receiving-options-unix-mbox-spool-file.page
	mail-receiving-options-usenet-news.page
	mail-refresh-folders.page
	mail-save-as-pdf.page
	mail-search-folders-add.page
	mail-search-folders-conditions.page
	mail-search-folders.page
	mail-searching-attachment-type.page
	mail-searching.page
	mail-send-and-receive-automatically.page
	mail-send-and-receive-manual.page
	mail-send-and-receive.page
	mail-sending-options-smtp.page
	mail-several-pop-accounts.page
	mail-sorting-folder-list.page
	mail-sorting-message-list.page
	mail-spam-marking.page
	mail-spam.page
	mail-spam-settings.page
	mail-two-trash-folders.page
	mail-usenet-subscriptions.page
	mail-vertical-view.page
	mail-word-wrap.page
	mail-working-offline.page
	memos-searching.page
	memos-usage-add-memo.page
	memos-usage-delete-memo.page
	memos-usage-edit-memo.page
	memos-usage.page
	offline.page
	organizing.page
	problems-debug-how-to.page
	problems-getting-help.page
	problems-reporting-bugs.page
	searching-items.page
	start-command-line-parameters.page
	sync-with-other-devices.page
	tasks-caldav.page
	tasks-display-settings.page
	tasks-local.page
	tasks-organizing.page
	tasks-searching.page
	tasks-usage-add-task.page
	tasks-usage-delete-task.page
	tasks-usage-edit-task.page
	tasks-usage.page
	tasks-using-several-tasklists.page
	tasks-webdav.page
	using-categories.page
	xinclude-filter-vfolder-conditions.xml
	xinclude-searching.xml
)

add_custom_target(help-files ALL)

macro(create_help_files_for_lang _lang _helpfilesvar)
	set(pofile "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${_lang}.po")
	set(mofile "${CMAKE_CURRENT_BINARY_DIR}/${dir}/${_lang}.mo")

	if(NOT EXISTS ${pofile})
		message(FATAL_ERROR "Passed incorrect language '${_lang}' to create_help_files_for_lang()")
	endif(NOT EXISTS ${pofile})

	set(installed)
	set(deps)
	foreach(_file ${HELP_FILES})
		list(APPEND installed ${CMAKE_CURRENT_BINARY_DIR}/${_lang}/${_file})
		list(APPEND deps ${CMAKE_CURRENT_SOURCE_DIR}/C/${_file})
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}/${_file})
			list(APPEND deps ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}/${_file})
		endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}/${_file})
	endforeach(_file)
	foreach(_file ${HELP_MEDIA})
		list(APPEND deps ${CMAKE_CURRENT_SOURCE_DIR}/C/${_file})
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}/${_file})
			list(APPEND deps ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}/${_file})
		endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}/${_file})
	endforeach(_file)

	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_lang}/index.page
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${_lang}"
		COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o "${mofile}" "${pofile}"
		COMMAND ${CMAKE_COMMAND} -E env d="${CMAKE_CURRENT_SOURCE_DIR}/C"
			itstool -m "${mofile}" ${${_helpfilesvar}} --output ${CMAKE_CURRENT_BINARY_DIR}/${_lang}/
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/C
		COMMENT "Generating '${_lang}' help files"
	)

	add_custom_target(${_lang}-help-files
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_lang}/index.page
			${pofile}
			${deps}
	)

	add_dependencies(help-files ${_lang}-help-files)

	install(FILES ${installed}
		DESTINATION ${SHARE_INSTALL_PREFIX}/help/${_lang}/${HELP_ID}/
	)
endmacro(create_help_files_for_lang)

file(GLOB directories RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
foreach(dir ${directories})
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${dir}.po")
		list(APPEND helpdeps "${CMAKE_CURRENT_BINARY_DIR}/${dir}/index.page")
		create_help_files_for_lang(${dir} HELP_FILES)

		foreach(_media ${HELP_MEDIA})
			if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${_media}")
				install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${_media}
					DESTINATION ${SHARE_INSTALL_PREFIX}/help/${dir}/${HELP_ID}/${HELP_FIGURES_DIR}/
				)
			else(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${_media}")
				# Pity, no symlink ability here
				install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/C/${_media}
					DESTINATION ${SHARE_INSTALL_PREFIX}/help/${dir}/${HELP_ID}/${HELP_FIGURES_DIR}/
				)
			endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${_media}")
		endforeach(_media)
	endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${dir}.po")
endforeach(dir)

set(cinstalled)
foreach(_file ${HELP_FILES})
	list(APPEND cinstalled "${CMAKE_CURRENT_SOURCE_DIR}/C/${_file}")
endforeach(_file)

install(FILES ${cinstalled}
	DESTINATION ${SHARE_INSTALL_PREFIX}/help/C/${HELP_ID}/
)

set(cinstalled)
foreach(_file ${HELP_MEDIA})
	list(APPEND cinstalled "${CMAKE_CURRENT_SOURCE_DIR}/C/${_file}")
endforeach(_file)

install(FILES ${cinstalled}
	DESTINATION ${SHARE_INSTALL_PREFIX}/help/C/${HELP_ID}/${HELP_FIGURES_DIR}
)
