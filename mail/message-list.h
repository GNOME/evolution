/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef _MESSAGE_LIST_H_
#define _MESSAGE_LIST_H_

#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>

#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-tree-scrolled.h>
#include "mail-types.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define MESSAGE_LIST_TYPE        (message_list_get_type ())
#define MESSAGE_LIST(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MESSAGE_LIST_TYPE, MessageList))
#define MESSAGE_LIST_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), MESSAGE_LIST_TYPE, MessageListClass))
#define IS_MESSAGE_LIST(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MESSAGE_LIST_TYPE))
#define IS_MESSAGE_LIST_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MESSAGE_LIST_TYPE))

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
	COL_FOLLOWUP_FLAG_STATUS,
	COL_FOLLOWUP_FLAG,
	COL_FOLLOWUP_DUE_BY,
	COL_LOCATION,		/* vfolder location? */
	
	/* normalised strings */
	COL_FROM_NORM,
	COL_SUBJECT_NORM,
	COL_TO_NORM,
	
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
	ETreeScrolled parent;

	struct _MessageListPrivate *priv;

	/* The table */
	ETreeModel   *model;
	ETree        *tree;
	ETreePath     tree_root;
	ETableExtras *extras;

	/* The folder & matching uri */
	CamelFolder  *folder;
	char *folder_uri;

	GHashTable *uid_nodemap; /* uid (from info) -> tree node mapping */
	
	GHashTable *normalised_hash;
	
	/* UID's to hide.  Keys in the mempool */
	/* IMPORTANT: You MUST have obtained the hide lock, to operate on this data */
	GHashTable	 *hidden;
	struct _EMemPool *hidden_pool;
	int hide_unhidden;           /* total length, before hiding */
	int hide_before, hide_after; /* hide ranges of messages */
	
	/* Current search string, or %NULL */
	char *search;
	
	/* are we regenerating the message_list because set_folder was just called? */
	guint just_set_folder : 1;
	
	/* Are we displaying threaded view? */
	guint threaded : 1;
	
	/* do we automatically hide deleted messages? */
	guint hidedeleted : 1;

	/* do we automatically hide junk messages? */
	guint hidejunk : 1;
	
	/* is the message-list object in a destroyed state? */
	guint destroyed : 1;

	/* frozen count */
	guint frozen:16;

	/* Where the ETree cursor is. */
	int cursor_row;
	char *cursor_uid;

	/* Row-selection and seen-marking timers */
	guint idle_id, seen_id;

	/* locks */
	GMutex *hide_lock;	/* for any 'hide' info above */

	/* list of outstanding regeneration requests */
	GList *regen;
	char *pending_select_uid; /* set if we were busy regnerating while we had a select come in */

	char *frozen_search;	/* to save search took place while we were frozen */

	/* the current camel folder thread tree, if any */
	struct _CamelFolderThread *thread_tree;

	/* for message/folder chagned event handling */
	struct _MailAsyncEvent *async_event;
};

typedef struct {
	ETreeScrolledClass parent_class;

	/* signals - select a message */
	void (*message_selected) (MessageList *ml, const char *uid);
	void (*message_list_built) (MessageList *ml);
	void (*message_list_scrolled) (MessageList *ml);
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
void           message_list_set_folder (MessageList *message_list, CamelFolder *camel_folder, const char *uri, gboolean outgoing);

void           message_list_foreach    (MessageList *message_list,
					MessageListForeachFunc callback,
					gpointer user_data);

void	       message_list_freeze(MessageList *ml);
void	       message_list_thaw(MessageList *ml);

GPtrArray     *message_list_get_selected(MessageList *ml);
void	       message_list_free_uids(MessageList *ml, GPtrArray *uids);

/* select next/prev message helpers */
gboolean       message_list_select     (MessageList *message_list,
					MessageListSelectDirection direction,
					guint32 flags,
					guint32 mask,
					gboolean wraparound);

void           message_list_select_uid (MessageList *message_list,
					const char *uid);

void           message_list_select_next_thread (MessageList *ml);

/* selection manipulation */
void           message_list_select_all (MessageList *ml);
void           message_list_select_thread (MessageList *ml);
void           message_list_invert_selection (MessageList *ml);

/* clipboard stuff */
void	       message_list_copy(MessageList *ml, gboolean cut);
void           message_list_paste (MessageList *ml);

/* info */
unsigned int   message_list_length (MessageList *ml);
unsigned int   message_list_hidden (MessageList *ml);

/* hide specific messages */
void	       message_list_hide_add (MessageList *ml, const char *expr, unsigned int lower, unsigned int upper);
void	       message_list_hide_uids (MessageList *ml, GPtrArray *uids);
void	       message_list_hide_clear (MessageList *ml);

void	       message_list_set_threaded (MessageList *ml, gboolean threaded);
void	       message_list_set_hidedeleted (MessageList *ml, gboolean hidedeleted);
void	       message_list_set_search (MessageList *ml, const char *search);

void           message_list_save_state (MessageList *ml);

double         message_list_get_scrollbar_position (MessageList *ml);
void           message_list_set_scrollbar_position (MessageList *ml, double pos);

#define MESSAGE_LIST_LOCK(m, l) g_mutex_lock(((MessageList *)m)->l)
#define MESSAGE_LIST_UNLOCK(m, l) g_mutex_unlock(((MessageList *)m)->l)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MESSAGE_LIST_H_ */
