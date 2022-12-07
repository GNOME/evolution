macro(add_simple_module _name _sourcesvar _depsvar _defsvar _cflagsvar _incdirsvar _ldflagsvar _destination)
	set(DEPENDENCIES
		evolution-util
	)

	set(SOURCES
		${${_sourcesvar}}
	)

	add_library(${_name} MODULE
		${SOURCES}
	)

	set_target_properties(${_name} PROPERTIES
		PREFIX ""
	)

	add_dependencies(${_name}
		${DEPENDENCIES}
		${${_depsvar}}
	)

	target_compile_definitions(${_name} PRIVATE
		-DG_LOG_DOMAIN=\"${_name}\"
		${${_defsvar}}
	)

	target_compile_options(${_name} PUBLIC
		${EVOLUTION_DATA_SERVER_CFLAGS}
		${GNOME_PLATFORM_CFLAGS}
		${${_cflagsvar}}
	)

	target_include_directories(${_name} PUBLIC
		${CMAKE_BINARY_DIR}
		${CMAKE_BINARY_DIR}/src
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}
		${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
		${GNOME_PLATFORM_INCLUDE_DIRS}
		${${_incdirsvar}}
	)

	target_link_libraries(${_name}
		${DEPENDENCIES}
		${${_depsvar}}
		${EVOLUTION_DATA_SERVER_LDFLAGS}
		${GNOME_PLATFORM_LDFLAGS}
		${${_ldflagsvar}}
	)

	install(TARGETS ${_name}
		DESTINATION ${_destination}
	)
endmacro(add_simple_module)

macro(add_evolution_module _name _sourcesvar _depsvar _defsvar _cflagsvar _incdirsvar _ldflagsvar)
	add_simple_module(${_name} ${_sourcesvar} ${_depsvar} ${_defsvar} ${_cflagsvar} ${_incdirsvar} ${_ldflagsvar} ${moduledir})
endmacro(add_evolution_module)

add_subdirectory(addressbook)
add_subdirectory(calendar)
add_subdirectory(mail)
add_subdirectory(accounts-window)
add_subdirectory(appearance-settings)
add_subdirectory(backup-restore)
add_subdirectory(book-config-carddav)
add_subdirectory(book-config-google)
add_subdirectory(book-config-local)
add_subdirectory(cal-config-caldav)
add_subdirectory(cal-config-contacts)
add_subdirectory(cal-config-google)
add_subdirectory(cal-config-local)
add_subdirectory(cal-config-webcal)
add_subdirectory(cal-config-webdav-notes)
add_subdirectory(composer-autosave)
add_subdirectory(composer-to-meeting)
add_subdirectory(config-lookup)
add_subdirectory(contact-photos)
add_subdirectory(gravatar)
add_subdirectory(itip-formatter)
add_subdirectory(mail-config)
add_subdirectory(mailto-handler)
add_subdirectory(mdn)
add_subdirectory(offline-alert)
add_subdirectory(plugin-lib)
add_subdirectory(plugin-manager)
add_subdirectory(prefer-plain)
add_subdirectory(rss)
add_subdirectory(settings)
add_subdirectory(startup-wizard)
add_subdirectory(vcard-inline)
add_subdirectory(webkit-editor)
add_subdirectory(webkit-inspector)

if(ENABLE_ALARM_NOTIFY_MODULE)
add_subdirectory(alarm-notify)
endif(ENABLE_ALARM_NOTIFY_MODULE)

if(ENABLE_TEXT_HIGHLIGHT)
	add_subdirectory(text-highlight)
endif(ENABLE_TEXT_HIGHLIGHT)

if(ENABLE_WEATHER)
	add_subdirectory(cal-config-weather)
endif(ENABLE_WEATHER)

if(ENABLE_YTNEF)
	add_subdirectory(tnef-attachment)
endif(ENABLE_YTNEF)

if(HAVE_LDAP)
	add_subdirectory(book-config-ldap)
endif(HAVE_LDAP)

if(WITH_BOGOFILTER)
	add_subdirectory(bogofilter)
endif(WITH_BOGOFILTER)

if(WITH_SPAMASSASSIN)
	add_subdirectory(spamassassin)
endif(WITH_SPAMASSASSIN)
