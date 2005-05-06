%.eplug: %.eplug.in
	sed -e 's|\@PLUGINDIR\@|$(plugindir)|' $< > $@

%.eplug: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< - | \
	sed -e 's|\@PLUGINDIR\@|$(plugindir)|' > $@

%.error: %.error.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@
