/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MESSAGE_LIST_H
#define E_MESSAGE_LIST_H

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <e-util/e-util.h>

typedef struct _EMailSession EMailSession;

G_BEGIN_DECLS

typedef enum {
	E_MESSAGE_LIST_COLUMN_STATUS,
	E_MESSAGE_LIST_COLUMN_ATTACHMENT,
	E_MESSAGE_LIST_COLUMN_FLAGGED,
	E_MESSAGE_LIST_COLUMN_FROM,
	E_MESSAGE_LIST_COLUMN_SUBJECT,
	E_MESSAGE_LIST_COLUMN_DATE_SENT,
	E_MESSAGE_LIST_COLUMN_SIZE,
	E_MESSAGE_LIST_COLUMN_TO,
	E_MESSAGE_LIST_COLUMN_CC,
	E_MESSAGE_LIST_COLUMN_DATE_RECEIVED,
	E_MESSAGE_LIST_COLUMN_SENDER,
	E_MESSAGE_LIST_COLUMN_SENDER_MAIL,
	E_MESSAGE_LIST_COLUMN_RECIPIENTS,
	E_MESSAGE_LIST_COLUMN_RECIPIENTS_MAIL,
	E_MESSAGE_LIST_COLUMN_CORRESPONDENTS,
	E_MESSAGE_LIST_COLUMN_SUBJECT_TRIMMED,
	E_MESSAGE_LIST_COLUMN_LABELS,
	E_MESSAGE_LIST_COLUMN_MLIST,
	E_MESSAGE_LIST_COLUMN_FOLLOWUP_FLAG_STATUS,
	E_MESSAGE_LIST_COLUMN_FOLLOWUP_FLAG,
	E_MESSAGE_LIST_COLUMN_FOLLOWUP_DUE_BY,
	E_MESSAGE_LIST_COLUMN_SCORE,
	E_MESSAGE_LIST_COLUMN_LOCATION,
	E_MESSAGE_LIST_COLUMN_PREVIEW,
	E_MESSAGE_LIST_COLUMN_USER_HEADER_1,
	E_MESSAGE_LIST_COLUMN_USER_HEADER_2,
	E_MESSAGE_LIST_COLUMN_USER_HEADER_3,
	E_MESSAGE_LIST_COLUMN_UID,
	E_MESSAGE_LIST_COLUMN_COMPOSITE,
	E_MESSAGE_LIST_COLUMN_COMPOSITE_TO,
	E_MESSAGE_LIST_N_COLUMNS
} EMessageListColumn;

typedef enum {
	E_MESSAGE_LIST_SELECT_NEXT		= 0,
	E_MESSAGE_LIST_SELECT_PREVIOUS		= 1,
	E_MESSAGE_LIST_SELECT_DIRECTION		= 1,
	E_MESSAGE_LIST_SELECT_WRAP		= 1 << 1,
	E_MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED	= 1 << 2
} EMessageListSelectDirection;

#define E_TYPE_MESSAGE_LIST (e_message_list_get_type ())
G_DECLARE_FINAL_TYPE (EMessageList, e_message_list, E, MESSAGE_LIST, GtkBox)

GtkWidget *		e_message_list_new		(EMailSession *session);
EMailSession *		e_message_list_get_session	(EMessageList *self);

void			e_message_list_set_folder	(EMessageList *self,
							 CamelFolder *folder);
CamelFolder *		e_message_list_ref_folder	(EMessageList *self);

void			e_message_list_set_show_deleted	(EMessageList *self,
							 gboolean show_deleted);
gboolean		e_message_list_get_show_deleted	(EMessageList *self);

void			e_message_list_set_show_junk	(EMessageList *self,
							 gboolean show_junk);
gboolean		e_message_list_get_show_junk	(EMessageList *self);
void			e_message_list_set_search_sexp	(EMessageList *self,
							 const gchar *sexp);
const gchar *		e_message_list_get_search_sexp	(EMessageList *self);

void			e_message_list_set_ensure_uid	(EMessageList *self,
							 const gchar *uid);
const gchar *		e_message_list_get_ensure_uid	(EMessageList *self);

void			e_message_list_set_threading	(EMessageList *self,
							 CamelFolderViewThreading mode);
CamelFolderViewThreading
			e_message_list_get_threading	(EMessageList *self);

void			e_message_list_set_thread_subject
							(EMessageList *self,
							 gboolean enabled);
gboolean		e_message_list_get_thread_subject
							(EMessageList *self);

void			e_message_list_set_group_by	(EMessageList *self,
							 CamelFolderViewGroupBy group_by);
CamelFolderViewGroupBy	e_message_list_get_group_by	(EMessageList *self);

void			e_message_list_set_empty_message
							(EMessageList *self,
							 const gchar *message);
const gchar *		e_message_list_get_empty_message
							(EMessageList *self);

EVirtualTree *		e_message_list_get_virtual_tree	(EMessageList *self);
GtkTreeViewColumn *	e_message_list_get_column	(EMessageList *self,
							 EMessageListColumn column);
const gchar * const *	e_message_list_get_legacy_etable_column_ids
							(guint *out_n_column_ids);

const gchar *		e_message_list_get_cursor_uid	(EMessageList *self);
guint			e_message_list_get_seen_id	(EMessageList *self);
void			e_message_list_set_seen_id	(EMessageList *self,
							 guint seen_id);
gboolean		e_message_list_get_last_sel_single
							(EMessageList *self);

gboolean		e_message_list_select		(EMessageList *self,
							 EMessageListSelectDirection direction,
							 guint32 flags,
							 guint32 mask);
void			e_message_list_select_uid	(EMessageList *self,
							 const gchar *uid,
							 gboolean with_fallback);
void			e_message_list_select_next_thread
							(EMessageList *self);
void			e_message_list_select_prev_thread
							(EMessageList *self);
GPtrArray *		e_message_list_get_selected	(EMessageList *self);
GPtrArray *		e_message_list_get_selected_with_collapsed_threads
							(EMessageList *self);
guint			e_message_list_selected_count	(EMessageList *self);
guint			e_message_list_count		(EMessageList *self);
gboolean		e_message_list_contains_uid	(EMessageList *self,
							 const gchar *uid);

void			e_message_list_freeze		(EMessageList *self);
void			e_message_list_thaw		(EMessageList *self);
void			e_message_list_set_regen_selects_unread
							(EMessageList *self,
							 gboolean selects_unread);
void			e_message_list_inc_setting_up_search_folder
							(EMessageList *self);
void			e_message_list_dec_setting_up_search_folder
							(EMessageList *self);
gboolean		e_message_list_is_setting_up_search_folder
							(EMessageList *self);
void			e_message_list_expand_all_threads
							(EMessageList *self);
void			e_message_list_collapse_all_threads
							(EMessageList *self);
void			e_message_list_select_thread	(EMessageList *self);
void			e_message_list_select_subthread	(EMessageList *self);
void			e_message_list_save_state	(EMessageList *self);
gboolean		e_message_list_get_just_set_folder
							(EMessageList *self);
gint			e_message_list_get_cursor_row	(EMessageList *self);
void			e_message_list_sort_uids	(EMessageList *self,
							 GPtrArray *uids);
void			e_message_list_ensure_cursor_visible
							(EMessageList *self);
void			e_message_list_apply_sort_from_vtree
							(EMessageList *self);
void			e_message_list_copy_state_from	(EMessageList *self,
							 EMessageList *source);

G_END_DECLS

#endif /* E_MESSAGE_LIST_H */
