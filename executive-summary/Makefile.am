SUBDIRS = idl evolution-services component test-service

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Summary.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

htmldir = $(datadir)/evolution
html_DATA = summary.html

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA) $(html_DATA)
