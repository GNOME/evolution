evolution_helpdir = $(datadir)/gnome/help/evolution/C

GUIDE_SGML_FILES =		\
	evolution.sgml		\
	apx-authors.sgml	\
	apx-bugs.sgml		\
	apx-gloss.sgml		\
	config-prefs.sgml	\
	config-setupassist.sgml	\
	config-sync.sgml	\
	preface.sgml		\
	usage-calendar.sgml	\
	usage-contact.sgml	\
	usage-mail.sgml		\
	usage-mainwindow.sgml	\
	usage-sync.sgml

FAQ_SGML_FILES = 		\
	evolution-faq.sgml

EXTRA_DIST = 			\
	$(FAQ_SGML_FILES) 	\
	$(GUIDE_SGML_FILES)

all: evolution-guide evolution-faq

evolution-guide: $(GUIDE_SGML_FILES)
	-db2html $(srcdir)/evolution.sgml

evolution-faq: $(FAQ_SGML_FILES)
	-db2html $(srcdir)/evolution-faq.sgml

dist-hook:
	mkdir $(distdir)/evolution
	-cp evolution/*.html evolution/*.css $(distdir)/evolution
	mkdir $(distdir)/fig
	-cp fig/*.png $(distdir)/fig
	mkdir $(distdir)/evolution/stylesheet-images
	-cp evolution/stylesheet-images/* $(distdir)/evolution/stylesheet-images

install-data-local: evolution-guide evolution-faq
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(evolution_helpdir)
	-for file in $(srcdir)/evolution/*.html $(srcdir)/evolution/*.css; do \
		basefile=`basename $$file`; \
		$(INSTALL_DATA) $$file $(DESTDIR)$(evolution_helpdir)/$$basefile; \
	done

	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(evolution_helpdir)/fig
	-for file in $(srcdir)/fig/*.png; do \
		basefile=`basename $$file`; \
		$(INSTALL_DATA) $$file $(DESTDIR)$(evolution_helpdir)/fig/$$basefile; \
	done

	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(evolution_helpdir)/stylesheet-images
	-for file in $(srcdir)/evolution/stylesheet-images/*; do \
		basefile=`basename $$file`; \
		$(INSTALL_DATA) $$file $(DESTDIR)$(evolution_helpdir)/stylesheet-images/$$basefile; \
	done

evolution.ps: evolution.sgml
	-db2ps $<

evolution.rtf: evolution.sgml	
	-db2rtf $<
