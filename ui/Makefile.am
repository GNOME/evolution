XML_FILES =				\
	evolution.xml			\
	evolution-addressbook.xml	\
	evolution-addressbook-ldap.xml	\
	evolution-calendar.xml		\
	evolution-contact-editor.xml	\
	evolution-event-editor.xml	\
	evolution-executive-summary.xml	\
	evolution-mail.xml		\
	evolution-message-composer.xml	\
	evolution-subscribe.xml		\
	evolution-task-editor-dialog.xml

H_FILES = \
	evolution.h			\
	evolution-addressbook.h	\
	evolution-addressbook-ldap.h	\
	evolution-contact-editor.h	\
	evolution-mail.h		\
	evolution-calendar.h		\
	evolution-subscribe.h		\
	evolution-executive-summary.h

uidir = $(datadir)/gnome/ui
ui_DATA = $(XML_FILES)

EXTRA_DIST = $(XML_FILES) $(H_FILES)

BUILT_SOURCES=$(XML_FILES:.xml=.h)

%.h : %.xml
	bonobo-ui-extract $^ > $@

