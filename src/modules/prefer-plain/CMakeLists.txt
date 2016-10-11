set(extra_deps
	evolution-mail
	evolution-mail-formatter
	evolution-shell
)
set(sources
	e-mail-parser-prefer-plain.c
	e-mail-parser-prefer-plain.h
	e-mail-display-popup-prefer-plain.c
	e-mail-display-popup-prefer-plain.h
	evolution-module-prefer-plain.c
)
set(extra_defines)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_evolution_module(module-prefer-plain
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)

add_subdirectory(plugin)
