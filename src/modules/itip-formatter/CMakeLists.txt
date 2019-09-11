add_error_files(itip-formatter org-gnome-itip-formatter.error)

set(extra_deps
	email-engine
	evolution-calendar
	evolution-mail
	evolution-mail-formatter
	evolution-shell
)
set(sources
	e-mail-formatter-itip.c
	e-mail-formatter-itip.h
	e-mail-parser-itip.c
	e-mail-parser-itip.h
	e-mail-part-itip.c
	e-mail-part-itip.h
	itip-view.c
	itip-view.h
	evolution-module-itip-formatter.c
	itip-view-elements-defines.h
)
set(extra_defines)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_evolution_module(module-itip-formatter
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)
