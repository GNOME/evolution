/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _MESSAGE_LIST_H_
#define _MESSAGE_LIST_H_

#include <gnome.h>
#include "mail-types.h"
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-tree-simple.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-checkbox.h>
#include <gal/e-table/e-cell-tree.h>

#define MESSAGE_LIST_TYPE        (message_list_get_type ())
#define MESSAGE_LIST(o)          (GTK_CHECK_CAST ((o), MESSAGE_LIST_TYPE, MessageList))
#define MESSAGE_LIST_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MESSAGE_LIST_TYPE, MessageListClass))
#define IS_MESSAGE_LIST(o)       (GTK_CHECK_TYPE ((o), MESSAGE_LIST_TYPE))
#define IS_MESSAGE_LIST_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MESSAGE_LIST_TYPE))

enum {
	COL_MESSAGE_STATUS,
	COL_FLAGGED,
	COL_SCORE,
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
	COL_COLOUR,
};

#define MESSAGE_LIST_COLUMN_IS_ACTIVE(col) (col == COL_MESSAGE_STATUS || \
					    col == COL_FLAGGED)

#define ML_HIDE_NONE_START (0)
#define ML_HIDE_NONE_END (2147483647)
/* dont change */
#define ML_HIDE_SAME (2147483646)

struct _MessageList {
	ETableScrolled parent;

	/* The table */
	ETableModel  *table_model;
	ETable       *table;
	ETreePath    *tree_root;

	/* The folder */
	CamelFolder  *folder;

	/* UID to model row hash table. Keys owned by the mempool. */
	GHashTable       *uid_rowmap;
	struct _EMemPool *uid_pool;

	/* UID's to hide.  Keys in the mempool */
	/* IMPORTANT: You MUST have obtained the camel lock, to operate on these structures */
	GHashTable	 *hidden;
	struct _EMemPool *hidden_pool;
	int hide_unhidden, /* total length, before hiding */
		hide_before, hide_after; /* hide ranges of messages */

	/* Current search string, or %NULL */
	char *search;

	/* Are we displaying threaded view? */
	gboolean threaded;

	/* Where the ETable cursor is. */
	int cursor_row;
	const char *cursor_uid;

	/* Row-selection and seen-marking timers */
	guint idle_id, seen_id;
};

typedef struct {
	ETableScrolledClass parent_class;

	/* signals - select a message */
	void (*message_selected) (MessageList *ml, const char *uid);
} MessageListClass;

typedef void (*MessageListForeachFunc) (MessageList *message_list,
					const char *uid,
					gpointer user_data);

typedef enum {
	MESSAGE_LIST_SELECT_NEXT = 1,
	MESSAGE_LIST_SELECT_PREVIOUS = -1
} MessageListSelectDirection;

GtkType        message_list_get_type   (void);
GtkWidget     *message_list_new        (void);
void           message_list_set_folder (MessageList *message_list,
					CamelFolder *camel_folder);

void           message_list_foreach    (MessageList *message_list,
					MessageListForeachFunc callback,
					gpointer user_data);

void           message_list_select     (MessageList *message_list,
					int base_row,
					MessageListSelectDirection direction,
					guint32 flags, guint32 mask);

/* info */
unsigned int   message_list_length(MessageList *ml);

/* hide specific messages */
void	       message_list_hide_add(MessageList *ml, const char *expr, unsigned int lower, unsigned int upper);
void	       message_list_hide_uids(MessageList *ml, GPtrArray *uids);
void	       message_list_hide_clear(MessageList *ml);

void	       message_list_set_threaded(MessageList *ml, gboolean threaded);
void	       message_list_set_search(MessageList *ml, const char *search);

#endif /* _MESSAGE_LIST_H_ */
