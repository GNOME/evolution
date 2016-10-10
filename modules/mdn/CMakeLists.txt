add_error_files(mdn evolution-mdn.error)

set(extra_deps
	email-engine
	evolution-mail
	evolution-shell
)
set(sources
	evolution-mdn.c
)
set(extra_defines)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_evolution_module(module-mdn
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)
