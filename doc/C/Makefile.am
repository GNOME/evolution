evolution_helpdir = $(datadir)/gnome/help/evolution/C

SGML_FILES =			\
	apx-authors.sgml	\
	apx-bugs.sgml		\
	apx-fdl.sgml		\
	apx-gloss.sgml		\
	config-prefs.sgml	\
	config-setupassist.sgml	\
	config-sync.sgml	\
	evolution-guide.sgml	\
	preface.sgml		\
	usage-calendar.sgml	\
	usage-contact.sgml	\
	usage-mail.sgml		\
	usage-mainwindow.sgml	\
	usage-sync.sgml


EXTRA_DIST = \
	$(SGML_FILES)

all: evolution-guide

evolution-guide: $(SGML_FILES)
	-db2html evolution-guide.sgml

dist-hook:
	mkdir $(distdir)/evolution-guide
	-cp evolution-guide/*.html evolution-guide/*.css \
		$(distdir)/evolution-guide
	mkdir $(distdir)/fig
	-cp fig/*.png $(distdir)/fig

install-data-local: evolution-guide
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(evolution_helpdir)/fig
	-for file in $(srcdir)/evolution-guide/*.html $(srcdir)/evolution-guide/*.css; do \
		basefile=`basename $$file`; \
		$(INSTALL_DATA) $$file $(DESTDIR)$(evolution_helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/fig/*.png; do \
		basefile=`basename $$file`; \
		$(INSTALL_DATA) $$file $(DESTDIR)$(evolution_helpdir)/fig/$$basefile; \
	done

evolution.ps: evolution.sgml
	-db2ps $<

evolution.rtf: evolution.sgml	
	-db2rtf $<

