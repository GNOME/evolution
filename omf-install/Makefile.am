omf_dest_dir=$(datadir)/omf/evolution
scrollkeeper_localstate_dir = $(SCROLLKEEPER_LOCALSTATE_DIR)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(omf_dest_dir)
	-for file in $(srcdir)/*.omf; do 			\
		$(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(omf_dest_dir); \
	done
	-scrollkeeper-update -p $(scrollkeeper_localstate_dir) 

uninstall-local:
	-for file in $(srcdir)/*.omf; do 			\
		basefile=`basename $$file`; 			\
		rm -f $(omf_dest_dir)/$$basefile; 		\
        done
	-rmdir $(omf_dest_dir)
	-scrollkeeper-update -p $(scrollkeeper_localstate_dir) 
