/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */












#ifndef _MAIL_DISPLAY_H_
#define _MAIL_DISPLAY_H_

#include <gtk/gtktable.h>
#include <gtkhtml/gtkhtml.h>
#include "camel/camel-stream.h"
#include "camel/camel-mime-message.h"
#include "folder-browser.h"


#define MAIL_DISPLAY_TYPE        (mail_display_get_type ())
#define MAIL_DISPLAY(o)          (GTK_CHECK_CAST ((o), MAIL_DISPLAY_TYPE, MailDisplay))
#define MAIL_DISPLAY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_DISPLAY_TYPE, MailDisplayClass))
#define IS_MAIL_DISPLAY(o)       (GTK_CHECK_TYPE ((o), MAIL_DISPLAY_TYPE))
#define IS_MAIL_DISPLAY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_DISPLAY_TYPE))

struct _MailDisplay {
	GtkTable parent;

	FolderBrowser *parent_folder_browser;

	GtkHTML *      headers_html_widget;
	GtkHTML *      body_html_widget;
	
	CamelMimeMessage *current_message;
};

typedef struct {
	GtkTableClass parent_class;
} MailDisplayClass;

GtkType        mail_display_get_type (void);
GtkWidget *    mail_display_new      (FolderBrowser *parent_folder_browser);

void           mail_display_set_message (MailDisplay *mail_display, 
					 CamelMedium *medium);



#endif /* _MAIL_DISPLAY_H_ */
