
#ifndef _EM_MESSAGE_BROWSER_H
#define _EM_MESSAGE_BROWSER_H

#include "em-folder-view.h"

typedef struct _EMMessageBrowser EMMessageBrowser;
typedef struct _EMMessageBrowserClass EMMessageBrowserClass;

struct _EMMessageBrowser {
	EMFolderView view;

	/* container, if setup */
	struct _GtkWidget *window;

	struct _EMMessageBrowserPrivate *priv;
};

struct _EMMessageBrowserClass {
	EMFolderViewClass parent_class;
};

GType em_message_browser_get_type(void);

GtkWidget *em_message_browser_new(void);

/* also sets up a bonobo container window w/ docks and so on */
GtkWidget *em_message_browser_window_new(void);

#endif /* ! _EM_MESSAGE_BROWSER_H */
