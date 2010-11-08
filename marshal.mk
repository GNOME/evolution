%.h: %.list
	( $(GLIB_GENMARSHAL) --prefix=$(subst -,_,$*) $< --header > $@.tmp \
	&& mv $@.tmp $@ ) || ( rm -f $@.tmp && exit 1 )

%.c: %.list %.h
	( (echo "#include \"$*.h\""; $(GLIB_GENMARSHAL) --prefix=$(subst -,_,$*) $(srcdir)/$*.list --body) > $@.tmp \
	&& mv $@.tmp $@ ) || ( rm -f $@.tmp && exit 1 )
