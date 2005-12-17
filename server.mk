%.server.in: %.server.in.in
	sed -e 's|\@BINDIR_IN_SERVER_FILE\@|$(bindir_in_server_file)|'\
	-e 's|\@PRIVLIBEXECDIR_IN_SERVER_FILE\@|$(privlibexecdir_in_server_file)|'\
	-e 's|\@COMPONENTDIR_IN_SERVER_FILE\@|$(componentdir_in_server_file)|'\
	-e 's|\@VERSION\@|$(BASE_VERSION)|' 			\
	-e 's|\@EXEEXT\@|$(EXEEXT)|'				\
	-e 's|\@SOEXT\@|$(SOEXT)|'				\
	-e 's|\@INTERFACE_VERSION\@|$(INTERFACE_VERSION)|' $< > $@

%_$(BASE_VERSION).server: %.server
	mv $< $@

