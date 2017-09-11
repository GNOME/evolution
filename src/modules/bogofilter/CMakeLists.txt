add_appdata_file(org.gnome.Evolution-bogofilter.metainfo.xml.in org.gnome.Evolution-bogofilter.metainfo.xml)

set(extra_deps
	email-engine
)
set(sources
	evolution-bogofilter.c
)
set(extra_defines
	-DWELCOME_MESSAGE=\"${privdatadir}/default/C/mail/local/Inbox\"
)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_evolution_module(module-bogofilter
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)
