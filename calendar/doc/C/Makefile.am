gnomecal_helpdir = $(datadir)/gnome/help/gnomecal/C

gnomecal_help_DATA = 	\
	gnomecal.html	\
	topic.dat

SGML_FILES =			\
	gnomecal.sgml

EXTRA_DIST = \
	topic.dat		\
	$(SGML_FILES)

all: 

gnomecal.html: gnomecal/gnomecal.html
	-cp gnomecal/gnomecal.html .

gnomecal/gnomecal.html: $(SGML_FILES)
	-db2html gnomecal.sgml

dist-hook:
	mkdir $(distdir)/gnomecal
	-cp gnomecal/*.html gnomecal/*.css $(distdir)/gnomecal
	-cp gnomecal.html $(distdir)
	mkdir $(distdir)/images
	-cp images/*.gif images/*.jpg $(distdir)/images

install-data-local: gnomecal.html
	$(mkinstalldirs) $(DESTDIR)$(gnomecal_helpdir)/images
	-for file in $(srcdir)/gnomecal/*.html $(srcdir)/gnomecal/*.css; do \
	basefile=`basename $$file`; \
	$(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(gnomecal_helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/images/*.jpg $(srcdir)/images/*.gif; do \
	$(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(gnomecal_helpdir)/images;\
	done

gnomecal.ps: gnomecal.sgml
	-db2ps $<

gnomecal.rtf: gnomecal.sgml	
	-db2rtf $<

