/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _MESSAGE_LIST_H_
#define _MESSAGE_LIST_H_

#include "mail-types.h"
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-ui-handler.h>
#include "camel/camel-folder.h"
#include "e-table/e-table-scrolled.h"
#include "e-table/e-table-simple.h"
#include "e-table/e-tree-simple.h"
#include "e-table/e-cell-text.h"
#include "e-table/e-cell-toggle.h"
#include "e-table/e-cell-checkbox.h"
#include "e-table/e-cell-tree.h"
#include "folder-browser.h"


#define MESSAGE_LIST_TYPE        (message_list_get_type ())
#define MESSAGE_LIST(o)          (GTK_CHECK_CAST ((o), MESSAGE_LIST_TYPE, MessageList))
#define MESSAGE_LIST_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MESSAGE_LIST_TYPE, MessageListClass))
#define IS_MESSAGE_LIST(o)       (GTK_CHECK_TYPE ((o), MESSAGE_LIST_TYPE))
#define IS_MESSAGE_LIST_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MESSAGE_LIST_TYPE))

typedef struct _Renderer Renderer;


enum {
	COL_ONLINE_STATUS,
	COL_MESSAGE_STATUS,
	COL_PRIORITY,
	COL_ATTACHMENT,
	COL_FROM,
	COL_SUBJECT,
	COL_SENT,
	COL_RECEIVED,
	COL_TO,
	COL_SIZE,

	COL_LAST,

	/* Invisible columns */
	COL_DELETED,
	COL_UNREAD,
};

struct _MessageList {
	BonoboObject parent;

	/* the folder browser that contains the 
	 * this message list */
	FolderBrowser *parent_folder_browser;

	ETableModel  *table_model;
	ETableHeader *header_model;
	ETableCol    *table_cols [COL_LAST];

	ECell        *render_text;
	ECell        *render_date;
	ECell        *render_online_status;
	ECell        *render_message_status;
	ECell        *render_priority;
	ECell        *render_attachment;
	ECell	     *render_tree;

	ETreePath    *tree_root; /* for tree view */

	GtkWidget    *etable;

	CamelFolder  *folder;

	GHashTable *uid_rowmap;

	char *search;		/* search string */

	int cursor_row;
	const char *cursor_uid;

	/* row-selection and seen-marking timers */
	guint idle_id, seen_id;
};

typedef struct {
	BonoboObjectClass parent_class;
} MessageListClass;

typedef void (*MessageListForeachFunc) (MessageList *message_list,
					const char *uid,
					gpointer user_data);

typedef enum {
	MESSAGE_LIST_SELECT_NEXT = 1,
	MESSAGE_LIST_SELECT_PREVIOUS = -1
} MessageListSelectDirection;

GtkType        message_list_get_type   (void);
BonoboObject   *message_list_new        (FolderBrowser *parent_folder_browser);
void           message_list_set_folder (MessageList *message_list,
					CamelFolder *camel_folder);
void           message_list_regenerate (MessageList *message_list, const char *search);
GtkWidget     *message_list_get_widget (MessageList *message_list);

void           message_list_foreach    (MessageList *message_list,
					MessageListForeachFunc callback,
					gpointer user_data);

void           message_list_select     (MessageList *message_list,
					int base_row,
					MessageListSelectDirection direction,
					guint32 flags, guint32 mask);

extern gboolean threaded_view;
void           message_list_toggle_threads (BonoboUIHandler *uih,
					    void *user_data,
					    const char *path);

#endif /* _MESSAGE_LIST_H_ */

