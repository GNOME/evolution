macro(add_simple_webextension_module _name _sourcesvar _depsvar _defsvar _cflagsvar _incdirsvar _ldflagsvar _destdir)
	set(wex_deps
		${${_depsvar}}
	)
	set(wex_cflags
		${${_cflagsvar}}
		${WEB_EXTENSIONS_CFLAGS}
	)
	set(wex_incdirs
		${${_incdirsvar}}
		${WEB_EXTENSIONS_INCLUDE_DIRS}
	)
	set(wex_ldflags
		${${_ldflagsvar}}
		${WEB_EXTENSIONS_LDFLAGS}
	)

	add_simple_module(${_name} ${_sourcesvar} wex_deps ${_defsvar} wex_cflags wex_incdirs wex_ldflags ${_destdir})
endmacro(add_simple_webextension_module)

macro(add_webextension_editor_module _name _sourcesvar _depsvar _defsvar _cflagsvar _incdirsvar _ldflagsvar)
	add_simple_webextension_module(${_name} ${_sourcesvar} ${_depsvar} ${_defsvar} ${_cflagsvar} ${_incdirsvar} ${_ldflagsvar} "${webextensionswebkiteditordir}")
endmacro(add_webextension_editor_module)

set(extra_deps
	evolution-util
	evolution-mail
)
set(sources
	e-editor-web-extension.c
	e-editor-web-extension.h
	e-editor-web-extension-main.c
)
set(extra_defines
	-DEVOLUTION_WEBKITDATADIR=\"${webkitdatadir}\"
)

set(extra_cflags
	${EVOLUTION_DATA_SERVER_CFLAGS}
)
set(extra_incdirs
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
)
set(extra_ldflags
	${EVOLUTION_DATA_SERVER_LDFLAGS}
)

add_webextension_editor_module(module-webkit-editor-webextension
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)
