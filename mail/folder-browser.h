#ifndef _FOLDER_BROWSER_H_
#define _FOLDER_BROWSER_H_

#include <gtk/gtktable.h>
#include "camel/camel-stream.h"

#define FOLDER_BROWSER_TYPE        (folder_browser_get_type ())
#define FOLDER_BROWSER(o)          (GTK_CHECK_CAST ((o), FOLDER_BROWSER_TYPE, FolderBrowser))
#define FOLDER_BROWSER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), FOLDER_BROWSER_TYPE, FolderBrowserClass))
#define IS_FOLDER_BROWSER(o)       (GTK_CHECK_TYPE ((o), FOLDER_BROWSER_TYPE))
#define IS_FOLDER_BROWSER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), FOLDER_BROWSER_TYPE))

typedef struct {
	GtkTable parent;

	GnomePropertyBag *properties;

	/*
	 * The current URI being displayed by the FolderBrowser
	 */
	char *uri;
} FolderBrowser;

typedef struct {
	GtkTableClass parent_class;
} FolderBrowserClass;

GtkType        folder_browser_get_type (void);
GtkWidget     *folder_browser_new      (void);

#endif /* _FOLDER_BROWSER_H_ */
