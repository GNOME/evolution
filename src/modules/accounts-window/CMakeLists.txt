set(extra_deps
	evolution-mail
	evolution-shell
)
set(sources
	accounts-window.c
	e-accounts-window-editors.c
	e-accounts-window-editors.h
	e-collection-wizard-page.c
	e-collection-wizard-page.h
	e-webdav-browser-page.c
	e-webdav-browser-page.h
)
set(extra_defines)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_evolution_module(module-accounts-window
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)
