%.eplug: %.eplug.in
	sed -e 's|\@PLUGINDIR\@|$(plugindir)|' $< > $@
