XML_FILES =				\
	evolution.xml			\
	evolution-addressbook.xml	\
	evolution-addressbook-ldap.xml	\
	evolution-contact-editor.xml	\
	evolution-mail.xml		\
	evolution-calendar.xml		\
	evolution-subscribe.xml		\
	evolution-executive-summary.xml	\
	evolution-task-editor-dialog.xml

uidir = $(datadir)/gnome/ui
ui_DATA = $(XML_FILES)

EXTRA_DIST = $(XML_FILES)

BUILT_SOURCES=$(XML_FILES:.xml=.h)

%.h : %.xml
	bonobo-ui-extract $^ > $@

