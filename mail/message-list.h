#ifndef _MESSAGE_LIST_H_
#define _MESSAGE_LIST_H_

#include <bonobo/gnome-main.h>
#include <bonobo/gnome-object.h>
#include "camel/camel-folder.h"

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

	COL_LAST
};

typedef struct {
	GnomeObject parent;

	ETableModel  *table_model;
	ETableHeader *header_model;
	ETableCol    *table_cols [COL_LAST];

	ECell        *render_text;
	ECell        *render_online_status;
	ECell        *render_message_status;
	ECell        *render_priority;
	ECell        *render_attachment;

	GtkWidget    *etable;

	CamelFolder  *folder;
	CamelFolderSummary *folder_summary;
} MessageList;

typedef struct {
	GnomeObjectClass parent_class;
} MessageListClass;

GtkType        message_list_get_type   (void);
GnomeObject   *message_list_new        (void);
void           message_list_set_folder (MessageList *message_list,
					CamelFolder *camel_folder);
GtkWidget     *message_list_get_widget (MessageList *message_list);

#endif /* _MESSAGE_LIST_H_ */
