#ifndef _FOLDER_BROWSER_H_
#define _FOLDER_BROWSER_H_

#include <gtk/gtktable.h>
#include "camel/camel-stream.h"
#include <bonobo/bonobo-property-bag.h>
#include "message-list.h"
#include "mail-display.h"

#define FOLDER_BROWSER_TYPE        (folder_browser_get_type ())
#define FOLDER_BROWSER(o)          (GTK_CHECK_CAST ((o), FOLDER_BROWSER_TYPE, FolderBrowser))
#define FOLDER_BROWSER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), FOLDER_BROWSER_TYPE, FolderBrowserClass))
#define IS_FOLDER_BROWSER(o)       (GTK_CHECK_TYPE ((o), FOLDER_BROWSER_TYPE))
#define IS_FOLDER_BROWSER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), FOLDER_BROWSER_TYPE))

typedef struct {
	GtkTable parent;

	BonoboPropertyBag *properties;

	/*
	 * The current URI being displayed by the FolderBrowser
	 */
	char        *uri;
	MessageList *message_list;
	GtkWidget   *message_list_w;
	MailDisplay *mail_display;
	GtkWidget   *vpaned;
	gboolean     preview_shown;
} FolderBrowser;

typedef struct {
	GtkTableClass parent_class;
} FolderBrowserClass;

GtkType        folder_browser_get_type (void);
GtkWidget     *folder_browser_new      (void);
void           folder_browser_set_uri  (FolderBrowser *folder_browser,
					const char *uri);
void           folder_browser_set_message_preview (FolderBrowser *folder_browser,
						   gboolean show_message_preview);

#endif /* _FOLDER_BROWSER_H_ */
