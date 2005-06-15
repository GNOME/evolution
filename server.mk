%.server.in: %.server.in.in
	sed -e 's|\@BINDIR\@|$(bindir)|' 			\
	-e 's|\@LIBEXECDIR\@|$(privlibexecdir)|' 		\
	-e 's|\@COMPONENTDIR\@|$(componentdir)|' 		\
	-e 's|\@IMPORTERSDIR\@|$(importersdir)|' 		\
	-e 's|\@VERSION\@|$(BASE_VERSION)|' 			\
	-e 's|\@EXEEXT\@|$(EXEEXT)|'				\
	-e 's|\@SOEXT\@|$(SOEXT)|'				\
	-e 's|\@INTERFACE_VERSION\@|$(INTERFACE_VERSION)|' $< > $@

%_$(BASE_VERSION).server: %.server
	mv $< $@

