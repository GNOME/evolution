/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef _MAIL_DISPLAY_H_
#define _MAIL_DISPLAY_H_

#include <gtk/gtkvbox.h>
#include <gtkhtml/gtkhtml.h>

#include "widgets/misc/e-scroll-frame.h"

#include "camel/camel-stream.h"
#include "camel/camel-mime-message.h"
#include "folder-browser.h"


#define MAIL_DISPLAY_TYPE        (mail_display_get_type ())
#define MAIL_DISPLAY(o)          (GTK_CHECK_CAST ((o), MAIL_DISPLAY_TYPE, MailDisplay))
#define MAIL_DISPLAY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_DISPLAY_TYPE, MailDisplayClass))
#define IS_MAIL_DISPLAY(o)       (GTK_CHECK_TYPE ((o), MAIL_DISPLAY_TYPE))
#define IS_MAIL_DISPLAY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_DISPLAY_TYPE))

struct _MailDisplay {
	GtkVBox parent;

	EScrollFrame *scroll;
	GtkHTML *html;

	FolderBrowser *parent_folder_browser;
	CamelMimeMessage *current_message;
};

typedef struct {
	GtkVBoxClass parent_class;
} MailDisplayClass;

GtkType        mail_display_get_type    (void);
GtkWidget *    mail_display_new         (FolderBrowser *parent_folder_browser);

void           mail_display_set_message (MailDisplay *mail_display, 
					 CamelMedium *medium);


#define HTML_HEADER "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<HTML>\n<HEAD>\n<META NAME=\"GENERATOR\" CONTENT=\"Evolution Mail Component\">\n</HEAD>\n"

void           mail_html_write          (GtkHTML *html,
					 GtkHTMLStream *stream,
					 const char *format, ...);
void           mail_text_write          (GtkHTML *html,
					 GtkHTMLStream *stream,
					 const char *format, ...);
void           mail_error_write         (GtkHTML *html,
					 GtkHTMLStream *stream,
					 const char *format, ...);

#endif /* _MAIL_DISPLAY_H_ */
