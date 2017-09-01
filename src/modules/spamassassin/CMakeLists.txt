add_appdata_file(org.gnome.Evolution-spamassassin.metainfo.xml.in org.gnome.Evolution-spamassassin.metainfo.xml)

set(extra_deps
	email-engine
	evolution-mail
	evolution-shell
)
set(sources
	evolution-spamassassin.c
)
set(extra_defines)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_evolution_module(module-spamassassin
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)
