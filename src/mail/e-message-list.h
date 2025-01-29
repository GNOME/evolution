/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_MESSAGE_LIST_H_
#define _E_MESSAGE_LIST_H_

#include <gtk/gtk.h>
#include <camel/camel.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define E_TYPE_MESSAGE_LIST \
	(e_message_list_get_type ())
#define E_MESSAGE_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MESSAGE_LIST, EMessageList))
#define E_MESSAGE_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MESSAGE_LIST, EMessageListClass))
#define E_IS_MESSAGE_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MESSAGE_LIST))
#define E_IS_MESSAGE_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MESSAGE_LIST))
#define E_MESSAGE_LIST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MESSAGE_LIST, EMessageListClass))

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

	COL_UID,
	COL_SENDER_MAIL,
	COL_RECIPIENTS_MAIL,
	COL_USER_HEADER_1,
	COL_USER_HEADER_2,
	COL_USER_HEADER_3,
	COL_BODY_PREVIEW,
	COL_CORRESPONDENTS,

	COL_LAST,

	/* Invisible columns */
	COL_DELETED,
	COL_DELETED_OR_JUNK,
	COL_JUNK,
	COL_JUNK_STRIKEOUT_COLOR,
	COL_UNREAD,
	COL_COLOUR,
	COL_ITALIC,
	COL_SUBJECT_WITH_BODY_PREVIEW
};

#define E_MESSAGE_LIST_COLUMN_IS_ACTIVE(col) (col == COL_MESSAGE_STATUS || \
					      col == COL_FLAGGED)

typedef struct _EMessageList EMessageList;
typedef struct _EMessageListClass EMessageListClass;
typedef struct _EMessageListPrivate EMessageListPrivate;

struct _EMessageList {
	ETree parent;

	EMessageListPrivate *priv;

	/* The table */
	ETableExtras *extras;

	GHashTable *uid_nodemap; /* uid (from info) -> tree node mapping */

	GHashTable *normalised_hash;

	/* Current search string, or %NULL */
	gchar *search;

	/* are we regenerating the message_list because set_folder
	 * was just called? */
	guint just_set_folder : 1;

	guint expand_all :1;
	guint collapse_all :1;

	/* frozen count */
	guint frozen : 16;

	/* Where the ETree cursor is. */
	gchar *cursor_uid;

	/* whether the last selection was on a single row or none/multi */
	gboolean last_sel_single;

	/* Row-selection and seen-marking timers */
	guint idle_id, seen_id;

	gchar *frozen_search;	/* to save search took place while we were frozen */
};

struct _EMessageListClass {
	ETreeClass parent_class;

	/* signals - select a message */
	void (*message_selected) (EMessageList *message_list, const gchar *uid);
	void (*message_list_built) (EMessageList *message_list);
	void (*message_list_scrolled) (EMessageList *message_list);
};

typedef enum {
	E_MESSAGE_LIST_SELECT_NEXT = 0,
	E_MESSAGE_LIST_SELECT_PREVIOUS = 1,
	E_MESSAGE_LIST_SELECT_DIRECTION = 1, /* direction mask */
	E_MESSAGE_LIST_SELECT_WRAP = 1 << 1, /* option bit */
	E_MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED = 1 << 2 /* whether to search collapsed nodes as well */
} EMessageListSelectDirection;

GType		e_message_list_get_type		(void);
GtkWidget *	e_message_list_new		(EMailSession *session);
EMailSession *	e_message_list_get_session	(EMessageList *message_list);
CamelFolder *	e_message_list_ref_folder	(EMessageList *message_list);
void		e_message_list_set_folder	(EMessageList *message_list,
						 CamelFolder *folder);
GtkTargetList *	e_message_list_get_copy_target_list
						(EMessageList *message_list);
GtkTargetList *	e_message_list_get_paste_target_list
						(EMessageList *message_list);
void		e_message_list_set_expanded_default
						(EMessageList *message_list,
						 gboolean expanded_default);
gboolean	e_message_list_get_group_by_threads
						(EMessageList *message_list);
void		e_message_list_set_group_by_threads
						(EMessageList *message_list,
						 gboolean group_by_threads);
gboolean	e_message_list_get_show_deleted	(EMessageList *message_list);
void		e_message_list_set_show_deleted	(EMessageList *message_list,
						 gboolean show_deleted);
gboolean	e_message_list_get_show_junk	(EMessageList *message_list);
void		e_message_list_set_show_junk	(EMessageList *message_list,
						 gboolean show_junk);
gboolean	e_message_list_get_thread_latest
						(EMessageList *message_list);
void		e_message_list_set_thread_latest
						(EMessageList *message_list,
						 gboolean thread_latest);
gboolean	e_message_list_get_thread_subject
						(EMessageList *message_list);
void		e_message_list_set_thread_subject
						(EMessageList *message_list,
						 gboolean thread_subject);
gboolean	e_message_list_get_thread_compress
						(EMessageList *message_list);
void		e_message_list_set_thread_compress
						(EMessageList *message_list,
						 gboolean thread_compress);
gboolean	e_message_list_get_thread_flat	(EMessageList *message_list);
void		e_message_list_set_thread_flat	(EMessageList *message_list,
						 gboolean thread_flat);
gboolean	e_message_list_get_regen_selects_unread
						(EMessageList *message_list);
void		e_message_list_set_regen_selects_unread
						(EMessageList *message_list,
						 gboolean regen_selects_unread);
void		e_message_list_freeze		(EMessageList *message_list);
void		e_message_list_thaw		(EMessageList *message_list);
GPtrArray *	e_message_list_get_selected	(EMessageList *message_list);
GPtrArray *	e_message_list_get_selected_with_collapsed_threads
						(EMessageList *message_list);
void		e_message_list_set_selected	(EMessageList *message_list,
						 GPtrArray *uids);
gboolean	e_message_list_select		(EMessageList *message_list,
						 EMessageListSelectDirection direction,
						 guint32 flags,
						 guint32 mask);
gboolean	e_message_list_can_select	(EMessageList *message_list,
						 EMessageListSelectDirection direction,
						 guint32 flags,
						 guint32 mask);
void		e_message_list_select_uid	(EMessageList *message_list,
						 const gchar *uid,
						 gboolean with_fallback);
void		e_message_list_select_next_thread
						(EMessageList *message_list);
void		e_message_list_select_prev_thread
						(EMessageList *message_list);
void		e_message_list_select_all	(EMessageList *message_list);
void		e_message_list_select_thread	(EMessageList *message_list);
void		e_message_list_select_subthread	(EMessageList *message_list);
void		e_message_list_copy		(EMessageList *message_list,
						 gboolean cut);
void		e_message_list_paste		(EMessageList *message_list);
guint		e_message_list_count		(EMessageList *message_list);
guint		e_message_list_selected_count	(EMessageList *message_list);
void		e_message_list_set_threaded_expand_all
						(EMessageList *message_list);
void		e_message_list_set_threaded_collapse_all
						(EMessageList *message_list);
void		e_message_list_set_search	(EMessageList *message_list,
						 const gchar *search);
void		e_message_list_save_state	(EMessageList *message_list);

void		e_message_list_sort_uids	(EMessageList *message_list,
						 GPtrArray *uids);
gboolean	e_message_list_contains_uid	(EMessageList *message_list,
						 const gchar *uid);
void		e_message_list_inc_setting_up_search_folder
						(EMessageList *message_list);
void		e_message_list_dec_setting_up_search_folder
						(EMessageList *message_list);
gboolean	e_message_list_is_setting_up_search_folder
						(EMessageList *message_list);

G_END_DECLS

#endif /* _E_MESSAGE_LIST_H_ */
