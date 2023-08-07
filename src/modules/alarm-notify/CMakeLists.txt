pkg_check_variable(edsuimoduledir libedataserverui-1.2 uimoduledir)

set(extra_deps
	evolution-util
)
set(sources
	alarm-notify.c
)
set(extra_defines)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_simple_module(module-evolution-alarm-notify
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
	${edsuimoduledir}
)
