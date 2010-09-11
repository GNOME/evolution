/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _MESSAGE_LIST_H_
#define _MESSAGE_LIST_H_

#include <gtk/gtk.h>
#include <camel/camel.h>

#include <table/e-tree.h>
#include <shell/e-shell-backend.h>

/* Standard GObject macros */
#define MESSAGE_LIST_TYPE \
	(message_list_get_type ())
#define MESSAGE_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), MESSAGE_LIST_TYPE, MessageList))
#define MESSAGE_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), MESSAGE_LIST_TYPE, MessageListClass))
#define IS_MESSAGE_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), MESSAGE_LIST_TYPE))
#define IS_MESSAGE_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), MESSAGE_LIST_TYPE))
#define MESSAGE_LIST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), MESSAGE_LIST_TYPE, MessageListClass))

G_BEGIN_DECLS

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
	COL_SENDER,
	COL_RECIPIENTS,
	COL_MIXED_SENDER,
	COL_MIXED_RECIPIENTS,
	COL_LABELS,

	/* subject with junk removed */
	COL_SUBJECT_TRIMMED,

	/* normalised strings */
	COL_FROM_NORM,
	COL_SUBJECT_NORM,
	COL_TO_NORM,

	COL_LAST,

	/* Invisible columns */
	COL_DELETED,
	COL_UNREAD,
	COL_COLOUR
};

#define MESSAGE_LIST_COLUMN_IS_ACTIVE(col) (col == COL_MESSAGE_STATUS || \
					    col == COL_FLAGGED)

typedef struct _MessageList MessageList;
typedef struct _MessageListClass MessageListClass;
typedef struct _MessageListPrivate MessageListPrivate;

struct _MessageList {
	ETree parent;

	MessageListPrivate *priv;

	/* The table */
	ETreeModel   *model;
	ETreePath     tree_root;
	ETableExtras *extras;

	/* The folder & matching uri */
	CamelFolder  *folder;
	gchar *folder_uri;

	GHashTable *uid_nodemap; /* uid (from info) -> tree node mapping */

	GHashTable *normalised_hash;

	/* Current search string, or %NULL */
	gchar *search;

	/* which message uid should be left in the list even not in a search after rebuild;
	   rebuild will clear the value to NULL */
	gchar *ensure_uid;

	/* are we regenerating the message_list because set_folder was just called? */
	guint just_set_folder : 1;

	/* Are we displaying threaded view? */
	guint threaded : 1;

	guint expand_all :1;
	guint collapse_all :1;

	/* do we automatically hide deleted messages? */
	guint hidedeleted : 1;

	/* do we automatically hide junk messages? */
	guint hidejunk : 1;

	/* frozen count */
	guint frozen:16;

	/* Where the ETree cursor is. */
	gchar *cursor_uid;

	/* whether the last selection was on a single row or none/multi */
	gboolean last_sel_single;

	/* Row-selection and seen-marking timers */
	guint idle_id, seen_id;

	/* list of outstanding regeneration requests */
	GList *regen;
	GMutex *regen_lock; /* when writing to the regen, guard with this lock too */
	gchar *pending_select_uid;	/* set if we were busy regnerating
					 * while we had a select come in */
	gboolean pending_select_fallback;
	guint regen_timeout_id;
	gpointer regen_timeout_msg;

	gchar *frozen_search;	/* to save search took place while we were frozen */

	/* the current camel folder thread tree, if any */
	CamelFolderThread *thread_tree;

	/* for message/folder chagned event handling */
	struct _MailAsyncEvent *async_event;
};

struct _MessageListClass {
	ETreeClass parent_class;

	/* signals - select a message */
	void (*message_selected) (MessageList *message_list, const gchar *uid);
	void (*message_list_built) (MessageList *message_list);
	void (*message_list_scrolled) (MessageList *message_list);
};

typedef enum {
	MESSAGE_LIST_SELECT_NEXT = 0,
	MESSAGE_LIST_SELECT_PREVIOUS = 1,
	MESSAGE_LIST_SELECT_DIRECTION = 1, /* direction mask */
	MESSAGE_LIST_SELECT_WRAP = 1<<1 /* option bit */
} MessageListSelectDirection;

GType		message_list_get_type		(void);
GtkWidget *	message_list_new		(EShellBackend *shell_backend);
EShellBackend *	message_list_get_shell_backend	(MessageList *message_list);
void		message_list_set_folder		(MessageList *message_list,
						 CamelFolder *camel_folder,
						 const gchar *uri,
						 gboolean outgoing);
GtkTargetList *	message_list_get_copy_target_list
						(MessageList *message_list);
GtkTargetList *	message_list_get_paste_target_list
						(MessageList *message_list);
void		message_list_freeze		(MessageList *message_list);
void		message_list_thaw		(MessageList *message_list);
GPtrArray *	message_list_get_uids		(MessageList *message_list);
GPtrArray *	message_list_get_selected	(MessageList *message_list);
void		message_list_set_selected	(MessageList *message_list,
						 GPtrArray *uids);
gboolean	message_list_select		(MessageList *message_list,
						 MessageListSelectDirection direction,
						 guint32 flags,
						 guint32 mask);
gboolean	message_list_can_select		(MessageList *message_list,
						 MessageListSelectDirection direction,
						 guint32 flags,
						 guint32 mask);
void		message_list_select_uid		(MessageList *message_list,
						 const gchar *uid,
						 gboolean with_fallback);
void		message_list_select_next_thread	(MessageList *message_list);
void		message_list_select_all		(MessageList *message_list);
void		message_list_select_thread	(MessageList *message_list);
void		message_list_select_subthread	(MessageList *message_list);
void		message_list_invert_selection	(MessageList *message_list);
void		message_list_copy		(MessageList *message_list,
						 gboolean cut);
void		message_list_paste		(MessageList *message_list);
guint		message_list_count		(MessageList *message_list);
void		message_list_set_threaded	(MessageList *message_list,
						 gboolean threaded);
void		message_list_set_threaded_expand_all
						(MessageList *message_list);
void		message_list_set_threaded_collapse_all
						(MessageList *message_list);
void		message_list_set_hidedeleted	(MessageList *message_list,
						 gboolean hidedeleted);
void		message_list_set_search		(MessageList *message_list,
						 const gchar *search);
void		message_list_ensure_message	(MessageList *message_list,
						 const gchar *uid);
void		message_list_save_state		(MessageList *message_list);

#define MESSAGE_LIST_LOCK(m, l) g_mutex_lock(((MessageList *)m)->l)
#define MESSAGE_LIST_UNLOCK(m, l) g_mutex_unlock(((MessageList *)m)->l)

G_END_DECLS

#endif /* _MESSAGE_LIST_H_ */
