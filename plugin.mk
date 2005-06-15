%.eplug: %.eplug.in
	sed -e 's|\@PLUGINDIR\@|$(plugindir)|' -e 's|\@SOEXT\@|$(SOEXT)|' $< > $@

%.eplug.in: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.error: %.error.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@
