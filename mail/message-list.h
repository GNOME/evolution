/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _MESSAGE_LIST_H_
#define _MESSAGE_LIST_H_

#include "mail-types.h"
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include "camel/camel-folder.h"
#include "e-table/e-table.h"
#include "e-table/e-table-simple.h"
#include "e-table/e-cell-text.h"
#include "e-table/e-cell-toggle.h"
#include "e-table/e-cell-checkbox.h"
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
	COL_RECEIVE,
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

	GtkWidget    *etable;

	CamelFolder  *folder;

	GPtrArray *summary_table; /* the summary of all messages */

	char *search;		/* search string */
	/* FIXME: This should use a better format ... */
	GList *matches;		/* when a search has been performed ... */
	int match_count;
	GPtrArray *summary_search_cache; /* summary info cache for searches */

	/* used by the idle-call to select a row */
	int row_to_select;
	guint idle_id;
} ;

typedef struct {
	BonoboObjectClass parent_class;
} MessageListClass;

GtkType        message_list_get_type   (void);
BonoboObject   *message_list_new        (FolderBrowser *parent_folder_browser);
void           message_list_set_folder (MessageList *message_list,
					CamelFolder *camel_folder);
void           message_list_set_search (MessageList *message_list, const char *search);
GtkWidget     *message_list_get_widget (MessageList *message_list);

#endif /* _MESSAGE_LIST_H_ */
