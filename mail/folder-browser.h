/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#ifndef _FOLDER_BROWSER_H_
#define _FOLDER_BROWSER_H_

#include "mail-types.h"
#include <gtk/gtktable.h>
#include "camel/camel-stream.h"
#include <bonobo/bonobo-property-bag.h>
#include "filter/filter-rule.h"
#include "filter/filter-context.h" /*eek*/
#include "message-list.h"
#include "mail-display.h"
#include "shell/Evolution.h"


#define FOLDER_BROWSER_TYPE        (folder_browser_get_type ())
#define FOLDER_BROWSER(o)          (GTK_CHECK_CAST ((o), FOLDER_BROWSER_TYPE, FolderBrowser))
#define FOLDER_BROWSER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), FOLDER_BROWSER_TYPE, FolderBrowserClass))
#define IS_FOLDER_BROWSER(o)       (GTK_CHECK_TYPE ((o), FOLDER_BROWSER_TYPE))
#define IS_FOLDER_BROWSER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), FOLDER_BROWSER_TYPE))

struct  _FolderBrowser {
	GtkTable parent;
	
	BonoboPropertyBag *properties;
	
	Evolution_Shell shell;
	
	/* This is a kludge for the toolbar problem. */
	int serial;
	
	/*
	 * The current URI being displayed by the FolderBrowser
	 */
	char        *uri;
	CamelFolder *folder;
	
	MessageList *message_list;
	GtkWidget   *message_list_w;
	MailDisplay *mail_display;
	GtkWidget   *vpaned;
	GtkWidget   *search_menu;
	GtkWidget   *search_entry;
	
	gboolean     preview_shown;

	/* Stuff to allow on-demand filtering */
	GSList        *filter_menu_paths;
	FilterContext *filter_context;
};


typedef struct {
	GtkTableClass parent_class;
} FolderBrowserClass;

struct fb_ondemand_closure {
	FilterRule *rule;
	FolderBrowser *fb;
	gchar *path;
};

GtkType    folder_browser_get_type             (void);
GtkWidget *folder_browser_new                  (const Evolution_Shell  shell);

gboolean   folder_browser_set_uri              (FolderBrowser         *folder_browser,
						const char            *uri);

void       folder_browser_set_message_preview  (FolderBrowser         *folder_browser,
						gboolean               show_message_preview);
void       folder_browser_clear_search         (FolderBrowser         *fb);

#endif /* _FOLDER_BROWSER_H_ */
