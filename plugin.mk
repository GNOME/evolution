%.eplug: %.eplug.in
	sed -e 's|\@PLUGINDIR\@|$(plugindir)|' $< > $@

%.eplug.in: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.error: %.error.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@
