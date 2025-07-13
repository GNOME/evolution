set(filedeps)
set(desktopdir ${SHARE_INSTALL_PREFIX}/applications)

option(WITH_WMCLASS_OVERRIDE "Override WMClass in the .desktop file" "")

if("${WITH_WMCLASS_OVERRIDE}" STREQUAL "" OR "${WITH_WMCLASS_OVERRIDE}" STREQUAL "OFF")
	set(WMCLASS_OVERRIDE "")
else("${WITH_WMCLASS_OVERRIDE}" STREQUAL "" OR "${WITH_WMCLASS_OVERRIDE}" STREQUAL "OFF")
	set(WMCLASS_OVERRIDE "StartupWMClass=${WITH_WMCLASS_OVERRIDE}\n")
endif("${WITH_WMCLASS_OVERRIDE}" STREQUAL "" OR "${WITH_WMCLASS_OVERRIDE}" STREQUAL "OFF")

set(DESKTOP_MIME_TYPES)
if(HAVE_LDAP)
	set(DESKTOP_MIME_TYPES "x-scheme-handler/ldap;x-scheme-handler/ldaps;")
endif(HAVE_LDAP)

configure_file(org.gnome.Evolution.desktop.in.in
	org.gnome.Evolution.desktop.in
	@ONLY
)

intltool_merge(${CMAKE_CURRENT_BINARY_DIR}/org.gnome.Evolution.desktop.in org.gnome.Evolution.desktop --desktop-style --utf8)

list(APPEND filedeps ${CMAKE_CURRENT_BINARY_DIR}/org.gnome.Evolution.desktop)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.gnome.Evolution.desktop
	DESTINATION ${desktopdir}
)

if("${VERSION_SUBSTRING}" STREQUAL "")
	set(APPDATA_RELEASE_VERSION "${PROJECT_VERSION}")
else("${VERSION_SUBSTRING}" STREQUAL "")
	set(APPDATA_RELEASE_VERSION "${PROJECT_VERSION} ${VERSION_SUBSTRING}")
endif("${VERSION_SUBSTRING}" STREQUAL "")

string(TIMESTAMP APPDATA_RELEASE_BUILD_DATE "%Y-%m-%d")

if(EXISTS "${CMAKE_BINARY_DIR}/appdata-releases.txt")
	file(READ ${CMAKE_BINARY_DIR}/appdata-releases.txt APPDATA_RELEASES)
endif(EXISTS "${CMAKE_BINARY_DIR}/appdata-releases.txt")

if("${APPDATA_RELEASES}" STREQUAL "")
	message(WARNING "Failed to extract release information from the NEWS file, falling back to generic information")
	set(APPDATA_RELEASES "    <release version=\"${APPDATA_RELEASE_VERSION}\" date=\"${APPDATA_RELEASE_BUILD_DATE}\" type=\"${APPDATA_RELEASE_TYPE}\"/>\n")
endif("${APPDATA_RELEASES}" STREQUAL "")

configure_file(org.gnome.Evolution.metainfo.xml.in.in
	org.gnome.Evolution.metainfo.xml.in
	@ONLY
)

add_appdata_file(${CMAKE_CURRENT_BINARY_DIR}/org.gnome.Evolution.metainfo.xml.in org.gnome.Evolution.metainfo.xml)

set(SCHEMAS
	org.gnome.evolution.gschema.xml
	org.gnome.evolution.addressbook.gschema.xml
	org.gnome.evolution.calendar.gschema.xml
	org.gnome.evolution.importer.gschema.xml
	org.gnome.evolution.mail.gschema.xml
	org.gnome.evolution.shell.gschema.xml
	org.gnome.evolution.bogofilter.gschema.xml
	org.gnome.evolution.spamassassin.gschema.xml
	org.gnome.evolution.text-highlight.gschema.xml
	org.gnome.evolution.plugin.attachment-reminder.gschema.xml
	org.gnome.evolution.plugin.autocontacts.gschema.xml
	org.gnome.evolution.plugin.email-custom-header.gschema.xml
	org.gnome.evolution.plugin.external-editor.gschema.xml
	org.gnome.evolution.plugin.face-picture.gschema.xml
	org.gnome.evolution.plugin.itip.gschema.xml
	org.gnome.evolution.plugin.mail-notification.gschema.xml
	org.gnome.evolution.plugin.prefer-plain.gschema.xml
	org.gnome.evolution.plugin.publish-calendar.gschema.xml
	org.gnome.evolution.plugin.sender-validator.gschema.xml
	org.gnome.evolution.plugin.templates.gschema.xml
)

set(BUILT_SCHEMAS)
foreach(_schema IN LISTS SCHEMAS)
	intltool_merge(${_schema}.in ${_schema} --no-translations --xml-style --utf8)
	list(APPEND BUILT_SCHEMAS ${CMAKE_CURRENT_BINARY_DIR}/${_schema})
endforeach(_schema)

add_custom_command(OUTPUT gschemas.compiled
	COMMAND ${GLIB_COMPILE_SCHEMAS} .
)

add_custom_target(data-files ALL
	DEPENDS gschemas.compiled
		${filedeps}
)

add_gsettings_schemas(data-files ${BUILT_SCHEMAS})

install(FILES evolution.convert
	DESTINATION ${SHARE_INSTALL_PREFIX}/GConf/gsettings
)

add_subdirectory(icons)
add_subdirectory(images)
add_subdirectory(sounds)
add_subdirectory(ui)
add_subdirectory(views)
add_subdirectory(webkit)
