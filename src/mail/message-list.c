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
 *		Miguel de Icaza (miguel@ximian.com)
 *      Bertrand Guiheneuf (bg@aful.org)
 *      And just about everyone else in evolution ...
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "e-mail-label-list-store.h"
#include "e-mail-notes.h"
#include "e-mail-ui-session.h"
#include "em-utils.h"

/*#define TIMEIT */

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef G_OS_WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif
#ifdef localtime_r
#undef localtime_r
#endif

/* The gmtime() and localtime() in Microsoft's C library are MT-safe */
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#define localtime_r(tp,tmp) (localtime(tp)?(*(tmp)=*localtime(tp),(tmp)):0)
#endif

#include "message-list.h"

#define d(x)
#define t(x)
#define dd(x) G_STMT_START { if (camel_debug ("message-list")) { x; } } G_STMT_END

/* Common search expression segments. */
#define EXCLUDE_DELETED_MESSAGES_EXPR	"(not (system-flag \"deleted\"))"
#define EXCLUDE_JUNK_MESSAGES_EXPR	"(not (system-flag \"junk\"))"

typedef struct _ExtendedGNode ExtendedGNode;
typedef struct _RegenData RegenData;

struct _MLSelection {
	GPtrArray *uids;
	CamelFolder *folder;
};

struct _MessageListPrivate {
	GtkWidget *invisible;	/* 4 selection */

	EMailSession *session;

	CamelFolder *folder;
	gulong folder_changed_handler_id;

	/* For message list regeneration. */
	GMutex regen_lock;
	GTask *regen_task;
	GSource *regen_idle_source;

	gboolean thaw_needs_regen;

	struct _MLSelection clipboard;
	gboolean destroyed;

	gboolean expanded_default;
	gboolean group_by_threads;
	gboolean show_deleted;
	gboolean show_junk;
	gboolean thread_latest;
	gboolean thread_subject;
	gboolean thread_compress;
	gboolean thread_flat;
	gboolean any_row_changed; /* save state before regen list when this is set to true */
	gboolean regen_selects_unread;

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;

	/* XXX Not sure if we really need a separate frozen counter
	 *     for the tree model but the old ETreeMemory class had
	 *     its own frozen counter so we preserve it here. */
	GNode *tree_model_root;
	gint tree_model_frozen;

	/* This aids in automatic message selection. */
	time_t newest_read_date;
	const gchar *newest_read_uid;
	time_t oldest_unread_date;
	const gchar *oldest_unread_uid;

	GSettings *mail_settings;
	gchar **re_prefixes;
	gchar **re_separators;
	GMutex re_prefixes_lock;

	GdkRGBA *new_mail_bg_color;
	gchar *new_mail_fg_color;

	guint update_actions_idle_id;

	volatile gint setting_up_search_folder;

	GSettings *eds_settings; /* references org.gnome.evolution-data-server schema */
	gchar *user_headers[CAMEL_UTILS_MAX_USER_HEADERS + 1];
	guint user_headers_count; /* how many are set */
};

/* XXX Plain GNode suffers from O(N) tail insertions, and that won't
 *     do for large mail folders.  This structure extends GNode with
 *     a pointer to its last child, so we get O(1) tail insertions. */
struct _ExtendedGNode {
	GNode gnode;
	GNode *last_child;
};

struct _RegenData {
	EActivity *activity;
	ETableSortInfo *sort_info;
	ETableHeader *full_header;

	gchar *search;

	gboolean group_by_threads;
	gboolean thread_subject;
	gboolean thread_flat;
	gboolean thread_latest;
	gboolean select_unread;

	CamelFolderThread *thread_tree;

	/* This indicates we're regenerating the message list because
	 * we received a "folder-changed" signal from our CamelFolder. */
	gboolean folder_changed;
	GHashTable *removed_uids; /* gchar *~>NULL */

	CamelFolder *folder;
	GPtrArray *summary;

	gint last_row; /* last selected (cursor) row */

	xmlDoc *expand_state; /* expanded state to be restored */

	/* These may be set during a regen operation.  Use the
	 * select_lock to ensure consistency and thread-safety.
	 * These are applied after the operation is finished. */
	GMutex select_lock;
	gchar *select_uid;
	gboolean select_all;
	gboolean select_use_fallback;
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_FOLDER,
	PROP_GROUP_BY_THREADS,
	PROP_PASTE_TARGET_LIST,
	PROP_SESSION,
	PROP_SHOW_DELETED,
	PROP_SHOW_JUNK,
	PROP_THREAD_LATEST,
	PROP_THREAD_SUBJECT,
	PROP_THREAD_COMPRESS,
	PROP_THREAD_FLAT
};

/* Forward Declarations */
static void	message_list_selectable_init
					(ESelectableInterface *iface);
static void	message_list_tree_model_init
					(ETreeModelInterface *iface);
static gboolean	message_list_get_hide_deleted
					(MessageList *message_list,
					 CamelFolder *folder);

G_DEFINE_TYPE_WITH_CODE (MessageList, message_list, E_TYPE_TREE,
	G_ADD_PRIVATE (MessageList)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, message_list_selectable_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_TREE_MODEL, message_list_tree_model_init))

static struct {
	const gchar *target;
	GdkAtom atom;
	guint32 actions;
} ml_drag_info[] = {
	{ "x-uid-list", NULL, GDK_ACTION_MOVE | GDK_ACTION_COPY },
	{ "message/rfc822", NULL, GDK_ACTION_COPY },
	{ "text/uri-list", NULL, GDK_ACTION_COPY },
};

enum {
	DND_X_UID_LIST,		/* x-uid-list */
	DND_MESSAGE_RFC822,	/* message/rfc822 */
	DND_TEXT_URI_LIST	/* text/uri-list */
};

/* What we send */
static GtkTargetEntry ml_drag_types[] = {
	{ (gchar *) "x-uid-list", 0, DND_X_UID_LIST },
	{ (gchar *) "text/uri-list", 0, DND_TEXT_URI_LIST },
};

/* What we accept */
static GtkTargetEntry ml_drop_types[] = {
	{ (gchar *) "x-uid-list", 0, DND_X_UID_LIST },
	{ (gchar *) "message/rfc822", 0, DND_MESSAGE_RFC822 },
	{ (gchar *) "text/uri-list", 0, DND_TEXT_URI_LIST },
};

/*
 * Default sizes for the ETable display
 *
 */
#define N_CHARS(x) (CHAR_WIDTH * (x))

#define COL_ICON_WIDTH         (16)
#define COL_ATTACH_WIDTH       (16)
#define COL_CHECK_BOX_WIDTH    (16)
#define COL_FROM_EXPANSION     (24.0)
#define COL_FROM_WIDTH_MIN     (32)
#define COL_SUBJECT_EXPANSION  (30.0)
#define COL_SUBJECT_WIDTH_MIN  (32)
#define COL_SENT_EXPANSION     (24.0)
#define COL_SENT_WIDTH_MIN     (32)
#define COL_RECEIVED_EXPANSION (20.0)
#define COL_RECEIVED_WIDTH_MIN (32)
#define COL_TO_EXPANSION       (24.0)
#define COL_TO_WIDTH_MIN       (32)
#define COL_SIZE_EXPANSION     (6.0)
#define COL_SIZE_WIDTH_MIN     (32)
#define COL_SENDER_EXPANSION     (24.0)
#define COL_SENDER_WIDTH_MIN     (32)

enum {
	NORMALISED_SUBJECT,
	NORMALISED_FROM,
	NORMALISED_TO,
	NORMALISED_LAST
};

static void	on_cursor_activated_cmd		(ETree *tree,
						 gint row,
						 GNode *node,
						 gpointer user_data);
static void	on_selection_changed_cmd	(ETree *tree,
						 MessageList *message_list);
static gint	on_click			(ETree *tree,
						 gint row,
						 GNode *node,
						 gint col,
						 GdkEvent *event,
						 MessageList *message_list);

static void	mail_regen_list			(MessageList *message_list,
						 const gchar *search,
						 CamelFolderChangeInfo *folder_changes);
static void	mail_regen_cancel		(MessageList *message_list);

static void	clear_info			(gchar *key,
						 GNode *node,
						 MessageList *message_list);

enum {
	MESSAGE_SELECTED,
	MESSAGE_LIST_BUILT,
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };

static const gchar *status_map[] = {
	N_("Unseen"),
	N_("Seen"),
	N_("Answered"),
	N_("Forwarded"),
	N_("Answered"), /* and unread */
	N_("Forwarded") /* and unread */
};

static const gchar *status_icons[] = {
	"mail-unread",
	"mail-read",
	"mail-replied",
	"mail-forward",
	"mail-replied", /* and unread */
	"mail-forward" /* and unread */
};

static const gchar *score_map[] = {
	N_("Lowest"),
	N_("Lower"),
	N_("Low"),
	N_("Normal"),
	N_("High"),
	N_("Higher"),
	N_("Highest"),
};

static const gchar *score_icons[] = {
	"stock_score-lowest",
	"stock_score-lower",
	"stock_score-low",
	"stock_score-normal",
	"stock_score-high",
	"stock_score-higher",
	"stock_score-highest"
};

static const gchar *attachment_icons[] = {
	NULL,  /* empty icon */
	"mail-attachment",
	"stock_people",
	"evolution-memos",
	"mail-mark-junk"
};

static const gchar *flagged_icons[] = {
	NULL,  /* empty icon */
	"emblem-important"
};

static const gchar *followup_icons[] = {
	NULL,  /* empty icon */
	"stock_mail-flag-for-followup",
	"stock_mail-flag-for-followup-done"
};

static GNode *
extended_g_node_new (gpointer data)
{
	GNode *node;

	node = (GNode *) g_slice_new0 (ExtendedGNode);
	node->data = data;

	return node;
}

static void
extended_g_node_unlink (GNode *node)
{
	g_return_if_fail (node != NULL);

	/* Update the last_child pointer before we unlink. */
	if (node->parent != NULL) {
		ExtendedGNode *ext_parent;

		ext_parent = (ExtendedGNode *) node->parent;
		if (ext_parent->last_child == node) {
			g_warn_if_fail (node->next == NULL);
			ext_parent->last_child = node->prev;
		}
	}

	g_node_unlink (node);
}

static void
extended_g_nodes_free (GNode *node)
{
	while (node != NULL) {
		GNode *next = node->next;
		if (node->children != NULL)
			extended_g_nodes_free (node->children);
		g_slice_free (ExtendedGNode, (ExtendedGNode *) node);
		node = next;
	}
}

static void
extended_g_node_destroy (GNode *root)
{
	g_return_if_fail (root != NULL);

	if (!G_NODE_IS_ROOT (root))
		extended_g_node_unlink (root);

	extended_g_nodes_free (root);
}

static GNode *
extended_g_node_insert_before (GNode *parent,
                               GNode *sibling,
                               GNode *node)
{
	ExtendedGNode *ext_parent;

	g_return_val_if_fail (parent != NULL, node);
	g_return_val_if_fail (node != NULL, node);
	g_return_val_if_fail (G_NODE_IS_ROOT (node), node);
	if (sibling != NULL)
		g_return_val_if_fail (sibling->parent == parent, node);

	ext_parent = (ExtendedGNode *) parent;

	/* This is where tracking the last child pays off. */
	if (sibling == NULL && ext_parent->last_child != NULL) {
		node->parent = parent;
		node->prev = ext_parent->last_child;
		ext_parent->last_child->next = node;
	} else {
		g_node_insert_before (parent, sibling, node);
	}

	if (sibling == NULL)
		ext_parent->last_child = node;

	return node;
}

static GNode *
extended_g_node_insert (GNode *parent,
                        gint position,
                        GNode *node)
{
	GNode *sibling;

	g_return_val_if_fail (parent != NULL, node);
	g_return_val_if_fail (node != NULL, node);
	g_return_val_if_fail (G_NODE_IS_ROOT (node), node);

	if (position > 0)
		sibling = g_node_nth_child (parent, position);
	else if (position == 0)
		sibling = parent->children;
	else /* if (position < 0) */
		sibling = NULL;

	return extended_g_node_insert_before (parent, sibling, node);
}

static RegenData *
regen_data_new (MessageList *message_list,
                GCancellable *cancellable)
{
	RegenData *regen_data;
	EActivity *activity;
	EMailSession *session;
	ETreeTableAdapter *adapter;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));

	activity = e_activity_new ();
	e_activity_set_cancellable (activity, cancellable);
	e_activity_set_text (activity, _("Generating message list"));

	regen_data = g_slice_new0 (RegenData);
	regen_data->activity = g_object_ref (activity);
	regen_data->folder = message_list_ref_folder (message_list);
	regen_data->last_row = -1;

	if (adapter) {
		regen_data->sort_info = e_tree_table_adapter_get_sort_info (adapter);
		regen_data->full_header = e_tree_table_adapter_get_header (adapter);

		if (regen_data->sort_info)
			g_object_ref (regen_data->sort_info);
		if (regen_data->full_header)
			g_object_ref (regen_data->full_header);
	}

	if (message_list->just_set_folder)
		regen_data->select_uid = g_strdup (message_list->cursor_uid);

	g_mutex_init (&regen_data->select_lock);

	session = message_list_get_session (message_list);
	e_mail_ui_session_add_activity (E_MAIL_UI_SESSION (session), activity);

	g_object_unref (activity);

	return regen_data;
}

static void
regen_data_free (RegenData *regen_data)
{
	g_return_if_fail (regen_data != NULL);

	g_clear_object (&regen_data->activity);
	g_clear_object (&regen_data->sort_info);
	g_clear_object (&regen_data->full_header);

	g_clear_pointer (&regen_data->search, g_free);
	g_clear_object (&regen_data->thread_tree);

	if (regen_data->summary != NULL) {
		guint ii, length;

		length = regen_data->summary->len;

		for (ii = 0; ii < length; ii++)
			g_clear_object (&regen_data->summary->pdata[ii]);

		g_clear_pointer (&regen_data->summary, g_ptr_array_unref);
	}

	g_clear_pointer (&regen_data->removed_uids, g_hash_table_unref);
	g_clear_object (&regen_data->folder);
	g_clear_pointer (&regen_data->expand_state, xmlFreeDoc);
	g_mutex_clear (&regen_data->select_lock);
	g_clear_pointer (&regen_data->select_uid, g_free);
	g_free (regen_data->select_uid);

	g_slice_free (RegenData, regen_data);
}

static void
message_list_tree_model_freeze (MessageList *message_list)
{
	if (message_list->priv->tree_model_frozen == 0)
		e_tree_model_pre_change (E_TREE_MODEL (message_list));

	message_list->priv->tree_model_frozen++;
}

static void
message_list_tree_model_thaw (MessageList *message_list)
{
	if (message_list->priv->tree_model_frozen > 0)
		message_list->priv->tree_model_frozen--;

	if (message_list->priv->tree_model_frozen == 0)
		e_tree_model_node_changed (
			E_TREE_MODEL (message_list),
			message_list->priv->tree_model_root);
}

static GNode *
message_list_tree_model_insert (MessageList *message_list,
                                GNode *parent,
                                gint position,
                                gpointer data)
{
	ETreeModel *tree_model;
	GNode *node;
	gboolean tree_model_frozen;

	if (parent == NULL)
		g_return_val_if_fail (
			message_list->priv->tree_model_root == NULL, NULL);

	tree_model = E_TREE_MODEL (message_list);
	tree_model_frozen = (message_list->priv->tree_model_frozen > 0);

	if (!tree_model_frozen)
		e_tree_model_pre_change (tree_model);

	node = extended_g_node_new (data);

	if (parent != NULL) {
		extended_g_node_insert (parent, position, node);
		if (!tree_model_frozen)
			e_tree_model_node_inserted (tree_model, parent, node);
	} else {
		message_list->priv->tree_model_root = node;
		if (!tree_model_frozen)
			e_tree_model_node_changed (tree_model, node);
	}

	return node;
}

static void
message_list_tree_model_remove (MessageList *message_list,
                                GNode *node)
{
	ETreeModel *tree_model;
	GNode *parent = node->parent;
	gboolean tree_model_frozen;
	gint old_position = 0;

	g_return_if_fail (node != NULL);

	tree_model = E_TREE_MODEL (message_list);
	tree_model_frozen = (message_list->priv->tree_model_frozen > 0);

	if (!tree_model_frozen) {
		e_tree_model_pre_change (tree_model);
		old_position = g_node_child_position (parent, node);
	}

	extended_g_node_unlink (node);

	if (!tree_model_frozen)
		e_tree_model_node_removed (
			tree_model, parent, node, old_position);

	extended_g_node_destroy (node);

	if (node == message_list->priv->tree_model_root)
		message_list->priv->tree_model_root = NULL;

	if (!tree_model_frozen)
		e_tree_model_node_deleted (tree_model, node);
}

static gint
address_compare (gconstpointer address1,
                 gconstpointer address2,
                 gpointer cmp_cache)
{
	gint retval;

	g_return_val_if_fail (address1 != NULL, 1);
	g_return_val_if_fail (address2 != NULL, -1);

	retval = g_ascii_strcasecmp ((gchar *) address1, (gchar *) address2);

	return retval;
}

static gint
mail_status_compare (gconstpointer pstatus1,
		     gconstpointer pstatus2,
		     gpointer cmp_cache)
{
	gint status1 = GPOINTER_TO_INT (pstatus1);
	gint status2 = GPOINTER_TO_INT (pstatus2);
	gboolean is_unread1 = status1 == 0 || status1 == 4 || status1 == 5;
	gboolean is_unread2 = status2 == 0 || status2 == 4 || status2 == 5;

	if ((is_unread1 ? 1 : 0) == (is_unread2 ? 1 : 0))
		return e_int_compare (pstatus1, pstatus2);

	return is_unread1 ? -1 : 1;
}

static gchar *
filter_size (gint size)
{
	gfloat fsize;

	if (size < 1024) {
		return g_strdup_printf ("%d", size);
	} else {
		fsize = ((gfloat) size) / 1024.0;
		if (fsize < 1024.0) {
			return g_strdup_printf ("%.2f K", fsize);
		} else {
			fsize /= 1024.0;
			return g_strdup_printf ("%.2f M", fsize);
		}
	}
}

/* Gets the uid of the message displayed at a given view row */
static const gchar *
get_message_uid (MessageList *message_list,
                 GNode *node)
{
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (node->data != NULL, NULL);

	return camel_message_info_get_uid (node->data);
}

/* Gets the CamelMessageInfo for the message displayed at the given
 * view row.
 */
static CamelMessageInfo *
get_message_info (MessageList *message_list,
                  GNode *node)
{
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (node->data != NULL, NULL);

	return node->data;
}

static const gchar *
get_normalised_string (MessageList *message_list,
                       CamelMessageInfo *info,
                       gint col)
{
	const gchar *string, *str;
	gchar *normalised;
	EPoolv *poolv;
	gint index;

	switch (col) {
	case COL_SUBJECT_NORM:
		string = camel_message_info_get_subject (info);
		index = NORMALISED_SUBJECT;
		break;
	case COL_FROM_NORM:
		string = camel_message_info_get_from (info);
		index = NORMALISED_FROM;
		break;
	case COL_TO_NORM:
		string = camel_message_info_get_to (info);
		index = NORMALISED_TO;
		break;
	default:
		string = NULL;
		index = NORMALISED_LAST;
		g_warning ("Should not be reached\n");
	}

	/* slight optimisation */
	if (string == NULL || string[0] == '\0')
		return "";

	poolv = g_hash_table_lookup (message_list->normalised_hash, camel_message_info_get_uid (info));
	if (poolv == NULL) {
		poolv = e_poolv_new (NORMALISED_LAST);
		g_hash_table_insert (message_list->normalised_hash, (gchar *) camel_message_info_get_uid (info), poolv);
	} else {
		str = e_poolv_get (poolv, index);
		if (*str)
			return str;
	}

	if (col == COL_SUBJECT_NORM) {
		gint skip_len;
		const gchar *subject;
		gboolean found_re = TRUE;

		subject = string;
		while (found_re) {
			g_mutex_lock (&message_list->priv->re_prefixes_lock);
			found_re = em_utils_is_re_in_subject (
				subject, &skip_len, (const gchar * const *) message_list->priv->re_prefixes,
				(const gchar * const *) message_list->priv->re_separators) && skip_len > 0;
			g_mutex_unlock (&message_list->priv->re_prefixes_lock);

			if (found_re)
				subject += skip_len;

			/* jump over any spaces */
			while (*subject && isspace ((gint) *subject))
				subject++;
		}

		/* jump over any spaces */
		while (*subject && isspace ((gint) *subject))
			subject++;

		string = subject;
		normalised = g_utf8_collate_key (string, -1);
	} else {
		/* because addresses require strings, not collate keys */
		normalised = g_strdup (string);
	}

	e_poolv_set (poolv, index, normalised, TRUE);

	return e_poolv_get (poolv, index);
}

static void
clear_selection (MessageList *message_list,
                 struct _MLSelection *selection)
{
	g_clear_pointer (&selection->uids, g_ptr_array_unref);
	g_clear_object (&selection->folder);
}

static GNode *
ml_get_next_node (GNode *node,
		  GNode *subroot)
{
	GNode *next;

	if (!node)
		return NULL;

	next = g_node_first_child (node);

	if (!next && node != subroot) {
		next = g_node_next_sibling (node);
	}

	if (!next && node != subroot) {
		next = node->parent;
		while (next) {
			GNode *sibl = g_node_next_sibling (next);

			if (next == subroot)
				return NULL;

			if (sibl) {
				next = sibl;
				break;
			} else {
				next = next->parent;
			}
		}
	}

	return next;
}

static GNode *
ml_get_prev_node (GNode *node,
		  GNode *subroot)
{
	GNode *prev;

	if (!node)
		return NULL;

	if (node == subroot)
		prev = NULL;
	else
		prev = g_node_prev_sibling (node);

	if (!prev) {
		prev = node->parent;

		if (prev == subroot)
			return NULL;

		if (prev)
			return prev;
	}

	if (prev) {
		GNode *child = g_node_last_child (prev);
		while (child) {
			prev = child;
			child = g_node_last_child (child);
		}
	}

	return prev;
}

static GNode *
ml_get_last_tree_node (GNode *node,
		       GNode *subroot)
{
	GNode *child;

	if (!node)
		return NULL;

	while (node->parent && node->parent != subroot)
		node = node->parent;

	if (node == subroot)
		child = node;
	else
		child = g_node_last_sibling (node);

	if (!child)
		child = node;

	while (child = g_node_last_child (child), child) {
		node = child;
	}

	return node;
}

static GNode *
ml_search_forward (MessageList *message_list,
                   gint start,
                   gint end,
                   guint32 flags,
                   guint32 mask,
		   gboolean include_collapsed,
		   gboolean skip_first)
{
	GNode *node;
	gint row;
	CamelMessageInfo *info;
	ETreeTableAdapter *etta;

	etta = e_tree_get_table_adapter (E_TREE (message_list));

	for (row = start; row <= end; row++) {
		node = e_tree_table_adapter_node_at_row (etta, row);
		if (node != NULL && !skip_first
		    && (info = get_message_info (message_list, node))
		    && (camel_message_info_get_flags (info) & mask) == flags)
			return node;

		skip_first = FALSE;

		if (node && include_collapsed && !e_tree_table_adapter_node_is_expanded (etta, node) && g_node_first_child (node)) {
			GNode *subnode = node;

			while (subnode = ml_get_next_node (subnode, node), subnode && subnode != node) {
				if ((info = get_message_info (message_list, subnode)) &&
				    (camel_message_info_get_flags (info) & mask) == flags)
					return subnode;
			}
		}
	}

	return NULL;
}

static GNode *
ml_search_backward (MessageList *message_list,
                    gint start,
                    gint end,
                    guint32 flags,
                    guint32 mask,
		    gboolean include_collapsed,
		    gboolean skip_first)
{
	GNode *node;
	gint row;
	CamelMessageInfo *info;
	ETreeTableAdapter *etta;

	etta = e_tree_get_table_adapter (E_TREE (message_list));

	for (row = start; row >= end; row--) {
		node = e_tree_table_adapter_node_at_row (etta, row);
		if (node != NULL && !skip_first
		    && (info = get_message_info (message_list, node))
		    && (camel_message_info_get_flags (info) & mask) == flags) {
			if (include_collapsed && !e_tree_table_adapter_node_is_expanded (etta, node) && g_node_first_child (node)) {
				GNode *subnode = ml_get_last_tree_node (g_node_first_child (node), node);

				while (subnode && subnode != node) {
					if ((info = get_message_info (message_list, subnode)) &&
					    (camel_message_info_get_flags (info) & mask) == flags)
						return subnode;

					subnode = ml_get_prev_node (subnode, node);
				}
			}

			return node;
		}

		if (node && include_collapsed && !skip_first && !e_tree_table_adapter_node_is_expanded (etta, node) && g_node_first_child (node)) {
			GNode *subnode = ml_get_last_tree_node (g_node_first_child (node), node);

			while (subnode && subnode != node) {
				if ((info = get_message_info (message_list, subnode)) &&
				    (camel_message_info_get_flags (info) & mask) == flags)
					return subnode;

				subnode = ml_get_prev_node (subnode, node);
			}
		}

		skip_first = FALSE;
	}

	return NULL;
}

static GNode *
ml_search_path (MessageList *message_list,
                MessageListSelectDirection direction,
                guint32 flags,
                guint32 mask)
{
	ETreeTableAdapter *adapter;
	gboolean include_collapsed;
	GNode *node;
	gint row_count;
	gint row;

	if (message_list->cursor_uid == NULL)
		return NULL;

	node = g_hash_table_lookup (
		message_list->uid_nodemap,
		message_list->cursor_uid);
	if (node == NULL)
		return NULL;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));
	row_count = e_table_model_row_count (E_TABLE_MODEL (adapter));

	row = e_tree_table_adapter_row_of_node (adapter, node);
	if (row == -1)
		return NULL;

	include_collapsed = (direction & MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED) != 0;

	if ((direction & MESSAGE_LIST_SELECT_DIRECTION) == MESSAGE_LIST_SELECT_NEXT)
		node = ml_search_forward (
			message_list, row, row_count - 1, flags, mask, include_collapsed, TRUE);
	else
		node = ml_search_backward (
			message_list, row, 0, flags, mask, include_collapsed, TRUE);

	if (node == NULL && (direction & MESSAGE_LIST_SELECT_WRAP)) {
		if ((direction & MESSAGE_LIST_SELECT_DIRECTION) == MESSAGE_LIST_SELECT_NEXT)
			node = ml_search_forward (
				message_list, 0, row, flags, mask, include_collapsed, FALSE);
		else
			node = ml_search_backward (
				message_list, row_count - 1, row, flags, mask, include_collapsed, FALSE);
	}

	return node;
}

static void
select_node (MessageList *message_list,
             GNode *node)
{
	ETree *tree;
	ETreeTableAdapter *etta;
	ETreeSelectionModel *etsm;

	tree = E_TREE (message_list);
	etta = e_tree_get_table_adapter (tree);
	etsm = (ETreeSelectionModel *) e_tree_get_selection_model (tree);

	g_free (message_list->cursor_uid);
	message_list->cursor_uid = NULL;

	e_tree_table_adapter_show_node (etta, node);
	e_tree_set_cursor (tree, node);
	e_tree_selection_model_select_single_path (etsm, node);
}

/**
 * message_list_select:
 * @message_list: a MessageList
 * @direction: the direction to search in
 * @flags: a set of flag values
 * @mask: a mask for comparing against @flags
 *
 * This moves the message list selection to a suitable row. @flags and
 * @mask combine to specify what constitutes a suitable row. @direction is
 * %MESSAGE_LIST_SELECT_NEXT if it should find the next matching
 * message, or %MESSAGE_LIST_SELECT_PREVIOUS if it should find the
 * previous. %MESSAGE_LIST_SELECT_WRAP is an option bit which specifies the
 * search should wrap.
 *
 * If no suitable row is found, the selection will be
 * unchanged.
 *
 * Returns %TRUE if a new message has been selected or %FALSE otherwise.
 **/
gboolean
message_list_select (MessageList *message_list,
                     MessageListSelectDirection direction,
                     guint32 flags,
                     guint32 mask)
{
	GNode *node;

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	node = ml_search_path (message_list, direction, flags, mask);
	if (node != NULL) {
		select_node (message_list, node);

		/* This function is usually called in response to a key
		 * press, so grab focus if the message list is visible. */
		if (gtk_widget_is_visible (GTK_WIDGET (message_list)))
			gtk_widget_grab_focus (GTK_WIDGET (message_list));

		return TRUE;
	} else
		return FALSE;
}

/**
 * message_list_can_select:
 * @message_list:
 * @direction:
 * @flags:
 * @mask:
 *
 * Returns true if the selection specified is possible with the current view.
 *
 * Return value:
 **/
gboolean
message_list_can_select (MessageList *message_list,
                         MessageListSelectDirection direction,
                         guint32 flags,
                         guint32 mask)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return ml_search_path (message_list, direction, flags, mask) != NULL;
}

/**
 * message_list_select_uid:
 * @message_list:
 * @uid:
 *
 * Selects the message with the given UID.
 **/
void
message_list_select_uid (MessageList *message_list,
                         const gchar *uid,
                         gboolean with_fallback)
{
	MessageListPrivate *priv;
	GHashTable *uid_nodemap;
	GNode *node = NULL;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	priv = message_list->priv;
	uid_nodemap = message_list->uid_nodemap;

	if (message_list->priv->folder == NULL)
		return;

	/* Try to find the requested message UID. */
	if (uid != NULL)
		node = g_hash_table_lookup (uid_nodemap, uid);

	g_mutex_lock (&message_list->priv->regen_lock);

	/* If we're busy or waiting to regenerate the message list, cache
	 * the UID so we can try again when we're done.  Otherwise if the
	 * requested message UID was not found and 'with_fallback' is set,
	 * try a couple fallbacks:
	 *
	 * 1) Oldest unread message in the list, by date received.
	 * 2) Newest read message in the list, by date received.
	 */
	if (message_list->priv->regen_task) {
		RegenData *regen_data = g_task_get_task_data (message_list->priv->regen_task);
		g_mutex_lock (&regen_data->select_lock);
		g_free (regen_data->select_uid);
		regen_data->select_uid = g_strdup (uid);
		regen_data->select_use_fallback = with_fallback;
		g_mutex_unlock (&regen_data->select_lock);

	} else if (with_fallback) {
		if (node == NULL && priv->oldest_unread_uid != NULL)
			node = g_hash_table_lookup (
				uid_nodemap, priv->oldest_unread_uid);
		if (node == NULL && priv->newest_read_uid != NULL)
			node = g_hash_table_lookup (
				uid_nodemap, priv->newest_read_uid);
	}

	g_mutex_unlock (&message_list->priv->regen_lock);

	if (node) {
		ETree *tree;
		GNode *old_cur;

		tree = E_TREE (message_list);
		old_cur = e_tree_get_cursor (tree);

		/* This will emit a changed signal that we'll pick up */
		e_tree_set_cursor (tree, node);

		if (old_cur == node)
			g_signal_emit (
				message_list,
				signals[MESSAGE_SELECTED],
				0, message_list->cursor_uid);
	} else if (message_list->just_set_folder) {
		g_free (message_list->cursor_uid);
		message_list->cursor_uid = g_strdup (uid);
		g_signal_emit (
			message_list,
			signals[MESSAGE_SELECTED], 0, message_list->cursor_uid);
	} else {
		g_free (message_list->cursor_uid);
		message_list->cursor_uid = NULL;
		g_signal_emit (
			message_list,
			signals[MESSAGE_SELECTED],
			0, NULL);
	}
}

void
message_list_select_next_thread (MessageList *message_list)
{
	ETreeTableAdapter *adapter;
	GNode *node;
	gint row_count;
	gint row;
	gint ii;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (message_list->cursor_uid == NULL)
		return;

	node = g_hash_table_lookup (
		message_list->uid_nodemap,
		message_list->cursor_uid);
	if (node == NULL)
		return;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));
	row_count = e_table_model_row_count ((ETableModel *) adapter);

	row = e_tree_table_adapter_row_of_node (adapter, node);
	if (row == -1)
		return;

	/* find the next node which has a root parent (i.e. toplevel node) */
	for (ii = row + 1; ii < row_count - 1; ii++) {
		node = e_tree_table_adapter_node_at_row (adapter, ii);
		if (node != NULL && G_NODE_IS_ROOT (node->parent)) {
			select_node (message_list, node);
			return;
		}
	}
}

void
message_list_select_prev_thread (MessageList *message_list)
{
	ETreeTableAdapter *adapter;
	GNode *node;
	gboolean skip_first;
	gint row;
	gint ii;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (message_list->cursor_uid == NULL)
		return;

	node = g_hash_table_lookup (
		message_list->uid_nodemap,
		message_list->cursor_uid);
	if (node == NULL)
		return;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));

	row = e_tree_table_adapter_row_of_node (adapter, node);
	if (row == -1)
		return;

	/* skip first found if in the middle of the thread */
	skip_first = !G_NODE_IS_ROOT (node->parent);

	/* find the previous node which has a root parent (i.e. toplevel node) */
	for (ii = row - 1; ii >= 0; ii--) {
		node = e_tree_table_adapter_node_at_row (adapter, ii);
		if (node != NULL && G_NODE_IS_ROOT (node->parent)) {
			if (skip_first) {
				skip_first = FALSE;
				continue;
			}

			select_node (message_list, node);
			return;
		}
	}
}

/**
 * message_list_select_all:
 * @message_list: Message List widget
 *
 * Selects all messages in the message list.
 **/
void
message_list_select_all (MessageList *message_list)
{
	RegenData *regen_data = NULL;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	g_mutex_lock (&message_list->priv->regen_lock);
	if (message_list->priv->regen_task)
		regen_data = g_task_get_task_data (message_list->priv->regen_task);

	if (regen_data != NULL && regen_data->group_by_threads) {
		regen_data->select_all = TRUE;
	} else {
		ETree *tree;
		ESelectionModel *selection_model;

		tree = E_TREE (message_list);
		selection_model = e_tree_get_selection_model (tree);
		e_selection_model_select_all (selection_model);
	}

	g_mutex_unlock (&message_list->priv->regen_lock);
}

typedef struct thread_select_info {
	MessageList *message_list;
	GPtrArray *paths;
} thread_select_info_t;

static gboolean
select_thread_node (ETreeModel *model,
                    GNode *node,
                    gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;

	g_ptr_array_add (tsi->paths, node);

	return FALSE; /*not done yet */
}

static void
select_thread (MessageList *message_list,
               ETreeForeachFunc selector)
{
	ETree *tree;
	ETreeSelectionModel *etsm;
	thread_select_info_t tsi;

	tsi.message_list = message_list;
	tsi.paths = g_ptr_array_new ();

	tree = E_TREE (message_list);
	etsm = (ETreeSelectionModel *) e_tree_get_selection_model (tree);

	e_tree_selection_model_foreach (etsm, selector, &tsi);

	e_tree_selection_model_select_paths (etsm, tsi.paths);

	g_ptr_array_free (tsi.paths, TRUE);
}

static void
thread_select_foreach (ETreePath path,
                       gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;
	ETreeModel *tree_model;
	GNode *last, *node = path;

	tree_model = E_TREE_MODEL (tsi->message_list);

	do {
		last = node;
		node = node->parent;
	} while (node && !G_NODE_IS_ROOT (node));

	g_ptr_array_add (tsi->paths, last);

	e_tree_model_node_traverse (
		tree_model, last,
		(ETreePathFunc) select_thread_node, tsi);
}

/**
 * message_list_select_thread:
 * @message_list: Message List widget
 *
 * Selects all messages in the current thread (based on cursor).
 **/
void
message_list_select_thread (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	select_thread (message_list, thread_select_foreach);
}

static void
subthread_select_foreach (ETreePath path,
                          gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;
	ETreeModel *tree_model;

	tree_model = E_TREE_MODEL (tsi->message_list);

	e_tree_model_node_traverse (
		tree_model, path,
		(ETreePathFunc) select_thread_node, tsi);
}

/**
 * message_list_select_subthread:
 * @message_list: Message List widget
 *
 * Selects all messages in the current subthread (based on cursor).
 **/
void
message_list_select_subthread (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	select_thread (message_list, subthread_select_foreach);
}

void
message_list_copy (MessageList *message_list,
                   gboolean cut)
{
	MessageListPrivate *priv;
	GPtrArray *uids;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	priv = message_list->priv;

	clear_selection (message_list, &priv->clipboard);

	uids = message_list_get_selected_with_collapsed_threads (message_list);

	if (uids->len > 0) {
		if (cut) {
			CamelFolder *folder;
			gint i;

			folder = message_list_ref_folder (message_list);

			camel_folder_freeze (folder);

			for (i = 0; i < uids->len; i++)
				camel_folder_set_message_flags (
					folder, uids->pdata[i],
					CAMEL_MESSAGE_SEEN |
					CAMEL_MESSAGE_DELETED,
					CAMEL_MESSAGE_SEEN |
					CAMEL_MESSAGE_DELETED);

			camel_folder_thaw (folder);

			g_object_unref (folder);
		}

		priv->clipboard.uids = g_ptr_array_ref (uids);
		priv->clipboard.folder = message_list_ref_folder (message_list);

		gtk_selection_owner_set (
			priv->invisible,
			GDK_SELECTION_CLIPBOARD,
			gtk_get_current_event_time ());
	} else {
		gtk_selection_owner_set (
			NULL, GDK_SELECTION_CLIPBOARD,
			gtk_get_current_event_time ());
	}

	g_ptr_array_unref (uids);
}

void
message_list_paste (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	gtk_selection_convert (
		message_list->priv->invisible,
		GDK_SELECTION_CLIPBOARD,
		gdk_atom_intern ("x-uid-list", FALSE),
		GDK_CURRENT_TIME);
}

static void
for_node_and_subtree_if_collapsed (MessageList *message_list,
                                   GNode *node,
                                   CamelMessageInfo *mi,
                                   ETreePathFunc func,
                                   gpointer data)
{
	ETreeModel *tree_model;
	ETreeTableAdapter *adapter;
	GNode *child = NULL;

	tree_model = E_TREE_MODEL (message_list);
	adapter = e_tree_get_table_adapter (E_TREE (message_list));

	func (NULL, (ETreePath) mi, data);

	if (node != NULL)
		child = g_node_first_child (node);

	if (child && !e_tree_table_adapter_node_is_expanded (adapter, node))
		e_tree_model_node_traverse (tree_model, node, func, data);
}

static gboolean
unread_foreach (ETreeModel *etm,
                ETreePath path,
                gpointer data)
{
	gboolean *saw_unread = data;
	CamelMessageInfo *info;

	if (!etm)
		info = (CamelMessageInfo *) path;
	else
		info = ((GNode *) path)->data;
	g_return_val_if_fail (info != NULL, FALSE);

	if (!(camel_message_info_get_flags (info) & CAMEL_MESSAGE_SEEN))
		*saw_unread = TRUE;

	return FALSE;
}

struct LatestData {
	gboolean sent;
	time_t latest;
};

static gboolean
latest_foreach (ETreeModel *etm,
                ETreePath path,
                gpointer data)
{
	struct LatestData *ld = data;
	CamelMessageInfo *info;
	time_t date;

	if (!etm)
		info = (CamelMessageInfo *) path;
	else
		info = ((GNode *) path)->data;
	g_return_val_if_fail (info != NULL, FALSE);

	date = ld->sent ? camel_message_info_get_date_sent (info)
			: camel_message_info_get_date_received (info);

	if (ld->latest == 0 || date > ld->latest)
		ld->latest = date;

	return FALSE;
}

static void
ml_add_name_or_email (GString *addresses,
		      const gchar *address,
		      gint addr_start,
		      gboolean use_name)
{
	g_return_if_fail (addresses != NULL);

	if (!address || !*address)
		return;

	while (*address == ' ') {
		if (addr_start >= 0)
			addr_start--;

		address++;
	}

	if (addresses->len)
		g_string_append_c (addresses, ' ');

	if (addr_start < 0) {
		g_string_append (addresses, address);
	} else if (use_name) {
		g_string_append_len (addresses, address, addr_start - 1);
	} else {
		const gchar *addr_end = strrchr (address + addr_start, '>');

		if (addr_end)
			g_string_append_len (addresses, address + addr_start, addr_end - address - addr_start);
		else
			g_string_append (addresses, address + addr_start);
	}
}

static gchar *
sanitize_addresses (const gchar *string,
		    gboolean return_names)
{
	GString  *gstring;
	gboolean quoted = FALSE;
	const gchar *p;
	gint addr_start = -1;
	GString *addresses = g_string_new ("");

	if (!string || !*string)
		return g_string_free (addresses, FALSE);

	gstring = g_string_new ("");

	for (p = string; *p; p = g_utf8_next_char (p)) {
		gunichar c = g_utf8_get_char (p);

		if (c == '"') {
			quoted = ~quoted;
		} else if (c == '<' && !quoted && addr_start == -1) {
			addr_start = gstring->len + 1;
		} else if (c == ',' && !quoted) {
			ml_add_name_or_email (addresses, gstring->str, addr_start, return_names);
			g_string_append_c (addresses, ',');
			g_string_truncate (gstring, 0);
			addr_start = -1;
			continue;
		}

		g_string_append_unichar (gstring, c);
	}

	ml_add_name_or_email (addresses, gstring->str, addr_start, return_names);
	g_string_free (gstring, TRUE);

	return g_string_free (addresses, FALSE);
}

struct LabelsData {
	EMailLabelListStore *store;
	GHashTable *labels_tag2iter;
	guint n_messages;
};

static void
add_label_if_known (struct LabelsData *ld,
                    const gchar *tag)
{
	GtkTreeIter label_defn;

	if (e_mail_label_list_store_lookup (ld->store, tag, &label_defn)) {
		g_hash_table_insert (
			ld->labels_tag2iter,
			/* Should be the same as the "tag" arg */
			e_mail_label_list_store_get_tag (ld->store, &label_defn),
			gtk_tree_iter_copy (&label_defn));
	}
}

static gboolean
add_all_labels_foreach (ETreeModel *etm,
                        ETreePath path,
                        gpointer data)
{
	struct LabelsData *ld = data;
	CamelMessageInfo *msg_info;
	const gchar *old_label;
	gchar *new_label;
	const CamelNamedFlags *flags;
	guint ii, len;

	if (!etm)
		msg_info = (CamelMessageInfo *) path;
	else
		msg_info = ((GNode *) path)->data;
	g_return_val_if_fail (msg_info != NULL, FALSE);

	ld->n_messages++;

	camel_message_info_property_lock (msg_info);
	flags = camel_message_info_get_user_flags (msg_info);
	len = camel_named_flags_get_length (flags);

	for (ii = 0; ii < len; ii++)
		add_label_if_known (ld, camel_named_flags_get (flags, ii));

	old_label = camel_message_info_get_user_tag (msg_info, "label");
	if (old_label != NULL) {
		/* Convert old-style labels ("<name>") to "$Label<name>". */
		new_label = g_alloca (strlen (old_label) + 10);
		g_stpcpy (g_stpcpy (new_label, "$Label"), old_label);

		add_label_if_known (ld, new_label);
	}

	camel_message_info_property_unlock (msg_info);

	return FALSE;
}

static const gchar *
get_trimmed_subject (CamelMessageInfo *info,
		     MessageList *message_list)
{
	const gchar *subject;
	const gchar *mlist;
	gint mlist_len = 0;
	gboolean found_mlist;

	subject = camel_message_info_get_subject (info);
	if (!subject || !*subject)
		return subject;

	mlist = camel_message_info_get_mlist (info);

	if (mlist && *mlist) {
		const gchar *mlist_end;

		mlist_end = strchr (mlist, '@');
		if (mlist_end)
			mlist_len = mlist_end - mlist;
		else
			mlist_len = strlen (mlist);
	}

	do {
		gint skip_len;
		gboolean found_re = TRUE;

		found_mlist = FALSE;

		while (found_re) {
			g_mutex_lock (&message_list->priv->re_prefixes_lock);
			found_re = em_utils_is_re_in_subject (
				subject, &skip_len, (const gchar * const *) message_list->priv->re_prefixes,
				(const gchar * const *) message_list->priv->re_separators) && skip_len > 0;
			g_mutex_unlock (&message_list->priv->re_prefixes_lock);
			if (found_re)
				subject += skip_len;

			/* jump over any spaces */
			while (*subject && isspace ((gint) *subject))
				subject++;
		}

		if (mlist_len &&
		    *subject == '[' &&
				!g_ascii_strncasecmp ((gchar *) subject + 1, mlist, mlist_len) &&
				subject[1 + mlist_len] == ']') {
			subject += 1 + mlist_len + 1;  /* jump over "[mailing-list]" */
			found_mlist = TRUE;

			/* jump over any spaces */
			while (*subject && isspace ((gint) *subject))
				subject++;
		}
	} while (found_mlist);

	/* jump over any spaces */
	while (*subject && isspace ((gint) *subject))
		subject++;

	return subject;
}

static gpointer
ml_tree_value_at_ex (ETreeModel *etm,
                     GNode *node,
                     gint col,
                     CamelMessageInfo *msg_info,
                     MessageList *message_list)
{
	EMailSession *session;
	const gchar *str;
	guint32 flags;

	session = message_list_get_session (message_list);

	g_return_val_if_fail (msg_info != NULL, NULL);

	switch (col) {
	case COL_MESSAGE_STATUS:
		flags = camel_message_info_get_flags (msg_info);
		if (!(flags & CAMEL_MESSAGE_SEEN) && (flags & CAMEL_MESSAGE_ANSWERED) != 0)
			return GINT_TO_POINTER (4);
		else if (!(flags & CAMEL_MESSAGE_SEEN) && (flags & CAMEL_MESSAGE_FORWARDED) != 0)
			return GINT_TO_POINTER (5);
		else if (flags & CAMEL_MESSAGE_ANSWERED)
			return GINT_TO_POINTER (2);
		else if (flags & CAMEL_MESSAGE_FORWARDED)
			return GINT_TO_POINTER (3);
		else if (flags & CAMEL_MESSAGE_SEEN)
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);
	case COL_FLAGGED:
		return GINT_TO_POINTER ((camel_message_info_get_flags (msg_info) & CAMEL_MESSAGE_FLAGGED) != 0);
	case COL_SCORE: {
		const gchar *tag;
		gint score = 0;

		tag = camel_message_info_get_user_tag (msg_info, "score");
		if (tag)
			score = atoi (tag);

		return GINT_TO_POINTER (score);
	}
	case COL_FOLLOWUP_FLAG_STATUS: {
		const gchar *tag, *cmp;

		/* FIXME: this all should be methods off of message-tag-followup class,
		 * FIXME: the tag names should be namespaced :( */
		tag = camel_message_info_get_user_tag (msg_info, "follow-up");
		cmp = camel_message_info_get_user_tag (msg_info, "completed-on");
		if (tag && tag[0]) {
			if (cmp && cmp[0])
				return GINT_TO_POINTER (2);
			else
				return GINT_TO_POINTER (1);
		} else
			return GINT_TO_POINTER (0);
	}
	case COL_FOLLOWUP_DUE_BY: {
		const gchar *tag;
		time_t due_by;

		tag = camel_message_info_get_user_tag (msg_info, "due-by");
		if (tag && *tag) {
			gint64 *res;

			due_by = camel_header_decode_date (tag, NULL);
			res = g_new0 (gint64, 1);
			*res = (gint64) due_by;

			return res;
		} else {
			return NULL;
		}
	}
	case COL_FOLLOWUP_FLAG:
		str = camel_message_info_get_user_tag (msg_info, "follow-up");
		return (gpointer)(str ? str : "");
	case COL_ATTACHMENT:
		if (camel_message_info_get_flags (msg_info) & CAMEL_MESSAGE_JUNK)
			return GINT_TO_POINTER (4);
		if (camel_message_info_get_user_flag (msg_info, E_MAIL_NOTES_USER_FLAG))
			return GINT_TO_POINTER (3);
		if (camel_message_info_get_user_flag (msg_info, "$has_cal"))
			return GINT_TO_POINTER (2);
		return GINT_TO_POINTER ((camel_message_info_get_flags (msg_info) & CAMEL_MESSAGE_ATTACHMENTS) != 0);
	case COL_FROM:
		str = camel_message_info_get_from (msg_info);
		return (gpointer)(str ? str : "");
	case COL_FROM_NORM:
		return (gpointer) get_normalised_string (message_list, msg_info, col);
	case COL_SUBJECT:
		str = camel_message_info_get_subject (msg_info);
		return (gpointer)(str ? str : "");
	case COL_SUBJECT_TRIMMED:
		str = get_trimmed_subject (msg_info, message_list);
		return (gpointer)(str ? str : "");
	case COL_SUBJECT_NORM:
		return (gpointer) get_normalised_string (message_list, msg_info, col);
	case COL_SENT: {
		struct LatestData ld;
		gint64 *res;

		ld.sent = TRUE;
		ld.latest = 0;

		for_node_and_subtree_if_collapsed (message_list, node, msg_info, latest_foreach, &ld);

		res = g_new0 (gint64, 1);
		*res = (gint64) ld.latest;

		return res;
	}
	case COL_RECEIVED: {
		struct LatestData ld;
		gint64 *res;

		ld.sent = FALSE;
		ld.latest = 0;

		for_node_and_subtree_if_collapsed (message_list, node, msg_info, latest_foreach, &ld);

		res = g_new0 (gint64, 1);
		*res = (gint64) ld.latest;

		return res;
	}
	case COL_TO:
		str = camel_message_info_get_to (msg_info);
		return (gpointer)(str ? str : "");
	case COL_TO_NORM:
		return (gpointer) get_normalised_string (message_list, msg_info, col);
	case COL_SIZE:
		return GINT_TO_POINTER (camel_message_info_get_size (msg_info));
	case COL_DELETED:
		return GINT_TO_POINTER ((camel_message_info_get_flags (msg_info) & CAMEL_MESSAGE_DELETED) != 0);
	case COL_DELETED_OR_JUNK:
		return GINT_TO_POINTER ((camel_message_info_get_flags (msg_info) & (CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_JUNK)) != 0);
	case COL_JUNK:
		return GINT_TO_POINTER ((camel_message_info_get_flags (msg_info) & CAMEL_MESSAGE_JUNK) != 0);
	case COL_JUNK_STRIKEOUT_COLOR:
		return GUINT_TO_POINTER (((camel_message_info_get_flags (msg_info) & CAMEL_MESSAGE_JUNK) != 0) ? 0xFF0000 : 0x0);
	case COL_UNREAD: {
		gboolean saw_unread = FALSE;

		for_node_and_subtree_if_collapsed (message_list, node, msg_info, unread_foreach, &saw_unread);

		return GINT_TO_POINTER (saw_unread);
	}
	case COL_COLOUR: {
		const gchar *colour, *due_by, *completed, *followup;

		/* Priority: colour tag; label tag; important flag; due-by tag */

		/* This is astonisngly poorly written code */

		/* To add to the woes, what color to show when the user choose multiple labels ?
		Don't say that I need to have the new labels[with subject] column visible always */

		colour = NULL;
		due_by = camel_message_info_get_user_tag (msg_info, "due-by");
		completed = camel_message_info_get_user_tag (msg_info, "completed-on");
		followup = camel_message_info_get_user_tag (msg_info, "follow-up");
		if (colour == NULL) {
			/* Get all applicable labels. */
			struct LabelsData ld;

			ld.store = e_mail_ui_session_get_label_store (
				E_MAIL_UI_SESSION (session));
			ld.labels_tag2iter = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gtk_tree_iter_free);
			ld.n_messages = 0;
			for_node_and_subtree_if_collapsed (message_list, node, msg_info, add_all_labels_foreach, &ld);

			if (g_hash_table_size (ld.labels_tag2iter) == 1) {
				GHashTableIter iter;
				GtkTreeIter *label_defn;
				GdkRGBA colour_val;
				gchar *colour_alloced;

				/* Extract the single label from the hashtable. */
				g_hash_table_iter_init (&iter, ld.labels_tag2iter);
				if (g_hash_table_iter_next (&iter, NULL, (gpointer *) &label_defn)) {
					e_mail_label_list_store_get_color (ld.store, label_defn, &colour_val);

					/* XXX Hack to avoid returning an allocated string. */
					colour_alloced = gdk_rgba_to_string (&colour_val);
					colour = g_intern_string (colour_alloced);
					g_free (colour_alloced);
				}
			} else if (ld.n_messages == 1 && g_hash_table_size (ld.labels_tag2iter) > 1) {
				/* When there is more than one label set on a single message, then pick
				   the color of the first found, in order of the EMailLabelListStore */
				GtkTreeIter titer;
				GtkTreeModel *model = GTK_TREE_MODEL (ld.store);

				if (gtk_tree_model_get_iter_first (model, &titer)) {
					do {
						gchar *tag;

						tag = e_mail_label_list_store_get_tag (ld.store, &titer);
						if (tag && g_hash_table_contains (ld.labels_tag2iter, tag)) {
							GdkRGBA colour_val;

							g_free (tag);

							if (e_mail_label_list_store_get_color (ld.store, &titer, &colour_val)) {
								gchar *colour_alloced;

								/* XXX Hack to avoid returning an allocated string. */
								colour_alloced = gdk_rgba_to_string (&colour_val);
								colour = g_intern_string (colour_alloced);
								g_free (colour_alloced);
							}
							break;
						}

						g_free (tag);
					} while (gtk_tree_model_iter_next (model, &titer));
				}
			}

			g_hash_table_destroy (ld.labels_tag2iter);
		}

		if (!colour) {
			if (camel_message_info_get_flags (msg_info) & CAMEL_MESSAGE_FLAGGED) {
				colour = "#A7453E";
			} else if (((followup && *followup) || (due_by && *due_by)) && !(completed && *completed)) {
				time_t now = time (NULL);

				if ((followup && *followup) || now >= camel_header_decode_date (due_by, NULL))
					colour = "#A7453E";
			}
		}

		if (!colour)
			colour = camel_message_info_get_user_tag (msg_info, "color");

		/*
		 * No flags/tags/user color on mail, check for unread status and
		 * "new-mail-fg-color" CSS attribute.
		 */
		if (!colour && message_list->priv->new_mail_fg_color) {
			gboolean saw_unread = FALSE;

			for_node_and_subtree_if_collapsed (message_list, node, msg_info, unread_foreach, &saw_unread);

			if (saw_unread)
				colour = message_list->priv->new_mail_fg_color;
		}

		return (gpointer) colour;
	}
	case COL_ITALIC: {
		return GINT_TO_POINTER (camel_message_info_get_user_flag (msg_info, "ignore-thread") ? 1 : 0);
	}
	case COL_LOCATION: {
		CamelFolder *folder;
		gchar *full_display_name;

		folder = message_list->priv->folder;

		if (CAMEL_IS_VEE_FOLDER (folder)) {
			folder = camel_vee_folder_get_location (
				CAMEL_VEE_FOLDER (folder),
				(CamelVeeMessageInfo *) msg_info, NULL);
		}

		full_display_name = e_mail_folder_to_full_display_name (folder, NULL);

		if (!full_display_name) {
			CamelStore *store;
			CamelService *service;
			const gchar *store_name;
			const gchar *folder_name;

			store = camel_folder_get_parent_store (folder);
			folder_name = camel_folder_get_full_name (folder);

			service = CAMEL_SERVICE (store);
			store_name = camel_service_get_display_name (service);

			full_display_name = g_strdup_printf ("%s : %s", store_name, folder_name);
		}

		return full_display_name;
	}
	case COL_MIXED_RECIPIENTS:
	case COL_RECIPIENTS:
	case COL_RECIPIENTS_MAIL: {
		str = camel_message_info_get_to (msg_info);
		return sanitize_addresses (str, col != COL_RECIPIENTS_MAIL);
	}
	case COL_MIXED_SENDER:
	case COL_SENDER:
	case COL_SENDER_MAIL: {
		str = camel_message_info_get_from (msg_info);
		return sanitize_addresses (str, col != COL_SENDER_MAIL);
	}
	case COL_CORRESPONDENTS: {
		/* Check whether the From address is one of the enabled configured accounts and
		   if so, then consider the message as being sent, not received; this fails when
		   sending from one configured account to another configured account, thus first
		   guess no received date is a sent mail, which does not work for some providers,
		   like Maildir, which sets the received date from the filename. Trying address
		   of the account of the selected folder won't work for On This Computer folders.
		   In other words, this is a pure guess, which has many counter cases. */
		const gchar *key_name = "evo-correspondents-is-sent";
		gint is_sent;

		if (camel_message_info_get_date_received (msg_info) <= 0) {
			is_sent = 1;
		} else {
			/* cache the result, to not convert & check against all accounts each paint request of the message list */
			is_sent = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (msg_info), key_name));
		}

		if (is_sent == 0) {
			is_sent = -1;

			str = camel_message_info_get_from (msg_info);

			if (str && *str) {
				CamelInternetAddress *addr;
				const gchar *email = NULL;

				addr = camel_internet_address_new ();

				if (camel_address_decode (CAMEL_ADDRESS (addr), str) > 0 &&
				    camel_internet_address_get (addr, 0, NULL, &email) &&
				    email && *email &&
				    em_utils_address_is_user (e_mail_session_get_registry (session), email, TRUE))
					is_sent = 1;

				g_clear_object (&addr);
			}

			g_object_set_data (G_OBJECT (msg_info), key_name, GINT_TO_POINTER (is_sent));
		}

		if (is_sent == 1)
			str = camel_message_info_get_to (msg_info);
		else if (is_sent == -1)
			str = camel_message_info_get_from (msg_info);
		else {
			g_warn_if_reached ();
			str = NULL;
		}

		return (gpointer) (str ? str : "");
	}
	case COL_LABELS:{
		struct LabelsData ld;
		GString *result = g_string_new ("");

		ld.store = e_mail_ui_session_get_label_store (
			E_MAIL_UI_SESSION (session));
		ld.labels_tag2iter = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gtk_tree_iter_free);
		ld.n_messages = 0;
		for_node_and_subtree_if_collapsed (message_list, node, msg_info, add_all_labels_foreach, &ld);

		if (g_hash_table_size (ld.labels_tag2iter) > 0) {
			GHashTableIter iter;
			GtkTreeIter *label_defn;

			g_hash_table_iter_init (&iter, ld.labels_tag2iter);
			while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &label_defn)) {
				gchar *label_name, *label_name_clean;

				if (result->len > 0)
					g_string_append (result, ", ");

				label_name = e_mail_label_list_store_get_name (ld.store, label_defn);
				label_name_clean = e_str_without_underscores (label_name);

				g_string_append (result, label_name_clean);

				g_free (label_name_clean);
				g_free (label_name);
			}
		}

		g_hash_table_destroy (ld.labels_tag2iter);
		return (gpointer) g_string_free (result, FALSE);
	}
	case COL_UID: {
		return (gpointer) camel_pstring_strdup (camel_message_info_get_uid (msg_info));
	}
	case COL_USER_HEADER_1:
	case COL_USER_HEADER_2:
	case COL_USER_HEADER_3: {
		const gchar *name = NULL;
		guint index = col - COL_USER_HEADER_1;
		if (index < message_list->priv->user_headers_count)
			name = message_list->priv->user_headers[index];
		if (name && *name)
			return (gpointer) camel_message_info_dup_user_header (msg_info, name);
		return NULL;
	}
	case COL_BODY_PREVIEW: {
		return camel_message_info_dup_preview (msg_info);
	}
	case COL_SUBJECT_WITH_BODY_PREVIEW: {
		const gchar *subject = camel_message_info_get_subject (msg_info);
		const gchar *preview = camel_message_info_get_preview (msg_info);

		if (!subject)
			subject = "";

		if (preview && *preview) {
			#define PREVIEW_PART_MARKUP "<span size='x-small'>%s</span>"

			if (gtk_widget_get_direction (GTK_WIDGET (message_list)) == GTK_TEXT_DIR_RTL)
				return g_markup_printf_escaped (PREVIEW_PART_MARKUP "   %s", preview, subject);
			else
				return g_markup_printf_escaped ("%s   " PREVIEW_PART_MARKUP, subject, preview);

			#undef PREVIEW_PART_MARKUP
		}

		if (!*subject)
			return NULL;

		return g_markup_escape_text (subject, -1);
	}
	default:
		g_warning ("%s: This shouldn't be reached (col:%d)", G_STRFUNC, col);
		return NULL;
	}
}

static gchar *
filter_date (const gint64 *pdate)
{
	time_t nowdate = time (NULL);
	time_t yesdate, date;
	struct tm then, now, yesterday;
	gchar buf[26];
	gboolean done = FALSE;

	if (!pdate || *pdate == 0)
		return g_strdup (_("?"));

	date = (time_t) *pdate;
	localtime_r (&date, &then);
	localtime_r (&nowdate, &now);
	if (then.tm_mday == now.tm_mday &&
	    then.tm_mon == now.tm_mon &&
	    then.tm_year == now.tm_year) {
		e_utf8_strftime_fix_am_pm (buf, 26, _("Today %l:%M %p"), &then);
		done = TRUE;
	}
	if (!done) {
		yesdate = nowdate - 60 * 60 * 24;
		localtime_r (&yesdate, &yesterday);
		if (then.tm_mday == yesterday.tm_mday &&
		    then.tm_mon == yesterday.tm_mon &&
		    then.tm_year == yesterday.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 26, _("Yesterday %l:%M %p"), &then);
			done = TRUE;
		}
	}
	if (!done) {
		gint i;
		for (i = 2; i < 7; i++) {
			yesdate = nowdate - 60 * 60 * 24 * i;
			localtime_r (&yesdate, &yesterday);
			if (then.tm_mday == yesterday.tm_mday &&
			    then.tm_mon == yesterday.tm_mon &&
			    then.tm_year == yesterday.tm_year) {
				e_utf8_strftime_fix_am_pm (buf, 26, _("%a %l:%M %p"), &then);
				done = TRUE;
				break;
			}
		}
	}
	if (!done) {
		if (then.tm_year == now.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 26, _("%b %d %l:%M %p"), &then);
		} else {
			e_utf8_strftime_fix_am_pm (buf, 26, _("%b %d %Y"), &then);
		}
	}

	return g_strdup (buf);
}

static ECell *
create_composite_cell (GSettings *mail_settings,
		       gint col)
{
	ECell *cell_vbox, *cell_hbox, *cell_date, *cell_from, *top_cell_tree, *bottom_cell_tree, *cell_attach;
	ECell *cell_preview;
	gboolean show_email;
	gboolean show_subject_above_sender;

	show_email = g_settings_get_boolean (mail_settings, "show-email");
	show_subject_above_sender = g_settings_get_boolean (mail_settings, "show-subject-above-sender");

	if (!show_email)
		col = (col == COL_FROM) ? COL_SENDER : COL_RECIPIENTS;

	cell_vbox = e_cell_vbox_new ();

	cell_hbox = e_cell_hbox_new ();

	/* Exclude the meeting icon. */
	cell_attach = e_cell_toggle_new (attachment_icons, G_N_ELEMENTS (attachment_icons));

	cell_date = e_cell_date_new (NULL, GTK_JUSTIFY_RIGHT);
	e_cell_date_set_format_component (E_CELL_DATE (cell_date), "mail");
	g_object_set (
		cell_date,
		"bold_column", COL_UNREAD,
		"italic-column", COL_ITALIC,
		"color_column", COL_COLOUR,
		NULL);

	cell_from = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell_from,
		"bold_column", COL_UNREAD,
		"italic-column", COL_ITALIC,
		"color_column", COL_COLOUR,
		NULL);

	cell_preview = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell_preview,
		"bold-column", COL_UNREAD,
		"italic-column", COL_ITALIC,
		"color-column", COL_COLOUR,
		"is-markup", TRUE,
		NULL);

	if (show_subject_above_sender)
		e_cell_hbox_append (E_CELL_HBOX (cell_hbox), cell_preview, COL_SUBJECT_WITH_BODY_PREVIEW, 68);
	else
		e_cell_hbox_append (E_CELL_HBOX (cell_hbox), cell_from, col, 68);

	e_cell_hbox_append (E_CELL_HBOX (cell_hbox), cell_attach, COL_ATTACHMENT, 5);
	e_cell_hbox_append (E_CELL_HBOX (cell_hbox), cell_date, COL_SENT, 27);

	top_cell_tree = e_cell_tree_new (TRUE, FALSE, cell_hbox);

	if (show_subject_above_sender)
		bottom_cell_tree = e_cell_tree_new (TRUE, TRUE, cell_from);
	else
		bottom_cell_tree = e_cell_tree_new (TRUE, TRUE, cell_preview);

	e_cell_vbox_append (E_CELL_VBOX (cell_vbox), top_cell_tree, show_subject_above_sender ? COL_SUBJECT_WITH_BODY_PREVIEW : col);
	e_cell_vbox_append (E_CELL_VBOX (cell_vbox), bottom_cell_tree, show_subject_above_sender ? col : COL_SUBJECT_WITH_BODY_PREVIEW);

	g_object_unref (top_cell_tree);
	g_object_unref (bottom_cell_tree);
	g_object_unref (cell_hbox);
	g_object_unref (cell_from);
	g_object_unref (cell_preview);
	g_object_unref (cell_attach);
	g_object_unref (cell_date);

	g_object_set_data (G_OBJECT (cell_vbox), "cell_date", cell_date);
	g_object_set_data (G_OBJECT (cell_vbox), "cell_from", cell_from);

	return cell_vbox;
}

static void
composite_cell_set_strike_col (ECell *cell,
                               gint strikeout_col,
			       gint strikeout_color_col)
{
	g_object_set (g_object_get_data (G_OBJECT (cell), "cell_date"),
		"strikeout-column", strikeout_col,
		"strikeout-color-column", strikeout_color_col,
		NULL);

	g_object_set (g_object_get_data (G_OBJECT (cell), "cell_from"),
		"strikeout-column", strikeout_col,
		"strikeout-color-column", strikeout_color_col,
		NULL);
}

static ETableExtras *
message_list_create_extras (GSettings *mail_settings)
{
	ETableExtras *extras;
	ECell *cell;

	extras = e_table_extras_new ();
	e_table_extras_add_icon_name (extras, "status", "mail-unread");
	e_table_extras_add_icon_name (extras, "score", "stock_score-higher");
	e_table_extras_add_icon_name (extras, "attachment", "mail-attachment");
	e_table_extras_add_icon_name (extras, "flagged", "emblem-important");
	e_table_extras_add_icon_name (extras, "followup", "stock_mail-flag-for-followup");

	e_table_extras_add_compare (extras, "address_compare", address_compare);
	e_table_extras_add_compare (extras, "mail-status", mail_status_compare);

	cell = e_cell_toggle_new (status_icons, G_N_ELEMENTS (status_icons));
	e_cell_toggle_set_icon_descriptions (E_CELL_TOGGLE (cell), status_map, G_N_ELEMENTS (status_map));
	e_table_extras_add_cell (extras, "render_message_status", cell);
	g_object_unref (cell);

	cell = e_cell_toggle_new (
		attachment_icons, G_N_ELEMENTS (attachment_icons));
	e_table_extras_add_cell (extras, "render_attachment", cell);
	g_object_unref (cell);

	cell = e_cell_toggle_new (
		flagged_icons, G_N_ELEMENTS (flagged_icons));
	e_table_extras_add_cell (extras, "render_flagged", cell);
	g_object_unref (cell);

	cell = e_cell_toggle_new (
		followup_icons, G_N_ELEMENTS (followup_icons));
	e_table_extras_add_cell (extras, "render_flag_status", cell);
	g_object_unref (cell);

	cell = e_cell_toggle_new (
		score_icons, G_N_ELEMENTS (score_icons));
	e_table_extras_add_cell (extras, "render_score", cell);
	g_object_unref (cell);

	/* date cell */
	cell = e_cell_date_new (NULL, GTK_JUSTIFY_LEFT);
	e_cell_date_set_format_component (E_CELL_DATE (cell), "mail");
	g_object_set (
		cell,
		"bold_column", COL_UNREAD,
		"italic-column", COL_ITALIC,
		"color_column", COL_COLOUR,
		NULL);
	e_table_extras_add_cell (extras, "render_date", cell);
	g_object_unref (cell);

	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"bold_column", COL_UNREAD,
		"italic-column", COL_ITALIC,
		"color_column", COL_COLOUR,
		"ellipsize-mode", PANGO_ELLIPSIZE_MIDDLE,
		NULL);
	e_table_extras_add_cell (extras, "render_location", cell);
	g_object_unref (cell);

	/* text cell */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"bold_column", COL_UNREAD,
		"italic-column", COL_ITALIC,
		"color_column", COL_COLOUR,
		NULL);
	e_table_extras_add_cell (extras, "render_text", cell);
	g_object_unref (cell);

	cell = e_cell_tree_new (TRUE, TRUE, cell);
	e_table_extras_add_cell (extras, "render_tree", cell);
	g_object_unref (cell);

	/* size cell */
	cell = e_cell_size_new (NULL, GTK_JUSTIFY_RIGHT);
	g_object_set (
		cell,
		"bold_column", COL_UNREAD,
		"italic-column", COL_ITALIC,
		"color_column", COL_COLOUR,
		NULL);
	e_table_extras_add_cell (extras, "render_size", cell);
	g_object_unref (cell);

	/* Composite cell for wide view */
	cell = create_composite_cell (mail_settings, COL_FROM);
	e_table_extras_add_cell (extras, "render_composite_from", cell);
	g_object_unref (cell);

	cell = create_composite_cell (mail_settings, COL_TO);
	e_table_extras_add_cell (extras, "render_composite_to", cell);
	g_object_unref (cell);

	/* set proper format component for a default 'date' cell renderer */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "mail");

	return extras;
}

static gboolean
message_list_is_searching (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->search && *message_list->search;
}

static void
save_tree_state (MessageList *message_list,
                 CamelFolder *folder)
{
	ETreeTableAdapter *adapter;
	gchar *filename;

	if (folder == NULL)
		return;

	if (message_list_is_searching (message_list))
		return;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));

	filename = mail_config_folder_to_cachename (folder, "et-expanded-");
	e_tree_table_adapter_save_expanded_state (adapter, filename);
	g_free (filename);

	message_list->priv->any_row_changed = FALSE;
}

static void
load_tree_state (MessageList *message_list,
                 CamelFolder *folder,
                 xmlDoc *expand_state)
{
	ETreeTableAdapter *adapter;

	if (folder == NULL)
		return;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));

	if (expand_state != NULL) {
		e_tree_table_adapter_load_expanded_state_xml (
			adapter, expand_state);
	} else {
		gchar *filename;

		filename = mail_config_folder_to_cachename (
			folder, "et-expanded-");
		e_tree_table_adapter_load_expanded_state (adapter, filename);
		g_free (filename);
	}

	message_list->priv->any_row_changed = FALSE;
}

void
message_list_save_state (MessageList *message_list)
{
	CamelFolder *folder;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	folder = message_list_ref_folder (message_list);

	if (folder != NULL) {
		save_tree_state (message_list, folder);
		g_object_unref (folder);
	}
}

static void
message_list_setup_etree (MessageList *message_list)
{
	CamelFolder *folder;

	/* Build the spec based on the folder, and possibly
	 * from a saved file.   Otherwise, leave default. */

	folder = message_list_ref_folder (message_list);

	if (folder != NULL) {
		gint data = 1;
		ETableItem *item;

		item = e_tree_get_item (E_TREE (message_list));

		g_object_set (message_list, "uniform_row_height", TRUE, NULL);
		g_object_set_data (
			G_OBJECT (((GnomeCanvasItem *) item)->canvas),
			"freeze-cursor", &data);

		/* build based on saved file */
		load_tree_state (message_list, folder, NULL);

		g_object_unref (folder);
	}
}

static void
ml_selection_get (GtkWidget *widget,
                  GtkSelectionData *data,
                  guint info,
                  guint time_stamp,
                  MessageList *message_list)
{
	struct _MLSelection *selection;

	selection = &message_list->priv->clipboard;

	if (selection->uids == NULL)
		return;

	if (info & 2) {
		/* text/plain */
		d (printf ("setting text/plain selection for uids\n"));
		em_utils_selection_set_mailbox (data, selection->folder, selection->uids);
	} else {
		/* x-uid-list */
		d (printf ("setting x-uid-list selection for uids\n"));
		em_utils_selection_set_uidlist (data, selection->folder, selection->uids);
	}
}

static gboolean
ml_selection_clear_event (GtkWidget *widget,
                          GdkEventSelection *event,
                          MessageList *message_list)
{
	MessageListPrivate *p = message_list->priv;

	clear_selection (message_list, &p->clipboard);

	return TRUE;
}

static void
ml_selection_received (GtkWidget *widget,
                       GtkSelectionData *selection_data,
                       guint time,
                       MessageList *message_list)
{
	EMailSession *session;
	CamelFolder *folder;
	GdkAtom target;

	target = gtk_selection_data_get_target (selection_data);

	if (target != gdk_atom_intern ("x-uid-list", FALSE)) {
		d (printf ("Unknown selection received by message-list\n"));
		return;
	}

	folder = message_list_ref_folder (message_list);
	session = message_list_get_session (message_list);

	/* FIXME Not passing a GCancellable or GError here. */
	em_utils_selection_get_uidlist (
		selection_data, session, folder, FALSE, NULL, NULL);

	g_clear_object (&folder);
}

static void
ml_tree_drag_data_get (ETree *tree,
                       gint row,
                       GNode *node,
                       gint col,
                       GdkDragContext *context,
                       GtkSelectionData *data,
                       guint info,
                       guint time,
                       MessageList *message_list)
{
	CamelFolder *folder;
	GPtrArray *uids;

	folder = message_list_ref_folder (message_list);
	uids = message_list_get_selected_with_collapsed_threads (message_list);

	if (uids->len > 0) {
		switch (info) {
		case DND_X_UID_LIST:
			em_utils_selection_set_uidlist (data, folder, uids);
			break;
		case DND_TEXT_URI_LIST:
			em_utils_selection_set_urilist (context, data, folder, uids);
			break;
		}
	}

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

/* TODO: merge this with the folder tree stuff via empopup targets */
/* Drop handling */
struct _drop_msg {
	MailMsg base;

	GdkDragContext *context;

	/* Only selection->data and selection->length are valid */
	GtkSelectionData *selection;

	CamelFolder *folder;
	MessageList *message_list;

	guint32 action;
	guint info;

	guint move : 1;
	guint moved : 1;
	guint aborted : 1;
};

static gchar *
ml_drop_async_desc (struct _drop_msg *m)
{
	const gchar *full_name;

	full_name = camel_folder_get_full_name (m->folder);

	if (m->move)
		return g_strdup_printf (_("Moving messages into folder %s"), full_name);
	else
		return g_strdup_printf (_("Copying messages into folder %s"), full_name);
}

static void
ml_drop_async_exec (struct _drop_msg *m,
                    GCancellable *cancellable,
                    GError **error)
{
	EMailSession *session;

	session = message_list_get_session (m->message_list);

	switch (m->info) {
	case DND_X_UID_LIST:
		em_utils_selection_get_uidlist (
			m->selection, session, m->folder,
			m->action == GDK_ACTION_MOVE,
			cancellable, error);
		break;
	case DND_MESSAGE_RFC822:
		em_utils_selection_get_message (m->selection, m->folder);
		break;
	case DND_TEXT_URI_LIST:
		em_utils_selection_get_urilist (m->selection, m->folder);
		break;
	}
}

static void
ml_drop_async_done (struct _drop_msg *m)
{
	gboolean success, delete;

	/* ?? */
	if (m->aborted) {
		success = FALSE;
		delete = FALSE;
	} else {
		success = (m->base.error == NULL);
		delete = success && m->move && !m->moved;
	}

	gtk_drag_finish (m->context, success, delete, GDK_CURRENT_TIME);
}

static void
ml_drop_async_free (struct _drop_msg *m)
{
	g_object_unref (m->context);
	g_object_unref (m->folder);
	g_object_unref (m->message_list);
	gtk_selection_data_free (m->selection);
}

static MailMsgInfo ml_drop_async_info = {
	sizeof (struct _drop_msg),
	(MailMsgDescFunc) ml_drop_async_desc,
	(MailMsgExecFunc) ml_drop_async_exec,
	(MailMsgDoneFunc) ml_drop_async_done,
	(MailMsgFreeFunc) ml_drop_async_free
};

static void
ml_drop_action (struct _drop_msg *m)
{
	m->move = m->action == GDK_ACTION_MOVE;
	mail_msg_unordered_push (m);
}

static void
ml_tree_drag_data_received (ETree *tree,
                            gint row,
                            GNode *node,
                            gint col,
                            GdkDragContext *context,
                            gint x,
                            gint y,
                            GtkSelectionData *selection_data,
                            guint info,
                            guint time,
                            MessageList *message_list)
{
	CamelFolder *folder;
	struct _drop_msg *m;

	if (gtk_selection_data_get_data (selection_data) == NULL)
		return;

	if (gtk_selection_data_get_length (selection_data) == -1)
		return;

	folder = message_list_ref_folder (message_list);
	if (folder == NULL)
		return;

	m = mail_msg_new (&ml_drop_async_info);
	m->context = g_object_ref (context);
	m->folder = g_object_ref (folder);
	m->message_list = g_object_ref (message_list);
	m->action = gdk_drag_context_get_selected_action (context);
	m->info = info;

	/* need to copy, goes away once we exit */
	m->selection = gtk_selection_data_copy (selection_data);

	ml_drop_action (m);

	g_object_unref (folder);
}

struct search_child_struct {
	gboolean found;
	gconstpointer looking_for;
};

static void
search_child_cb (GtkWidget *widget,
                 gpointer data)
{
	struct search_child_struct *search = (struct search_child_struct *) data;

	search->found = search->found || g_direct_equal (widget, search->looking_for);
}

static gboolean
is_tree_widget_children (ETree *tree,
                         gconstpointer widget)
{
	struct search_child_struct search;

	search.found = FALSE;
	search.looking_for = widget;

	gtk_container_foreach (GTK_CONTAINER (tree), search_child_cb, &search);

	return search.found;
}

static gboolean
ml_tree_drag_motion (ETree *tree,
                     GdkDragContext *context,
                     gint x,
                     gint y,
                     guint time,
                     MessageList *message_list)
{
	GList *targets;
	GdkDragAction action, actions = 0;
	GtkWidget *source_widget;

	/* If drop target is name of the account/store
	 * and not actual folder, don't allow any action. */
	if (message_list->priv->folder == NULL) {
		gdk_drag_status (context, 0, time);
		return TRUE;
	}

	source_widget = gtk_drag_get_source_widget (context);

	/* If source widget is packed under 'tree', don't allow any action */
	if (is_tree_widget_children (tree, source_widget)) {
		gdk_drag_status (context, 0, time);
		return TRUE;
	}

	if (EM_IS_FOLDER_TREE (source_widget)) {
		EMFolderTree *folder_tree;
		CamelFolder *selected_folder = NULL;
		CamelStore *selected_store;
		gchar *selected_folder_name;
		gboolean has_selection;

		folder_tree = EM_FOLDER_TREE (source_widget);

		has_selection = em_folder_tree_get_selected (
			folder_tree, &selected_store, &selected_folder_name);

		/* Sanity checks */
		g_warn_if_fail (
			(has_selection && selected_store != NULL) ||
			(!has_selection && selected_store == NULL));
		g_warn_if_fail (
			(has_selection && selected_folder_name != NULL) ||
			(!has_selection && selected_folder_name == NULL));

		if (has_selection) {
			selected_folder = camel_store_get_folder_sync (
				selected_store, selected_folder_name,
				0, NULL, NULL);
			g_object_unref (selected_store);
			g_free (selected_folder_name);
		}

		if (selected_folder == message_list->priv->folder) {
			gdk_drag_status (context, 0, time);
			return TRUE;
		}
	}

	targets = gdk_drag_context_list_targets (context);
	while (targets != NULL) {
		gint i;

		d (printf ("atom drop '%s'\n", gdk_atom_name (targets->data)));
		for (i = 0; i < G_N_ELEMENTS (ml_drag_info); i++)
			if (targets->data == (gpointer) ml_drag_info[i].atom)
				actions |= ml_drag_info[i].actions;

		targets = g_list_next (targets);
	}
	d (printf ("\n"));

	actions &= gdk_drag_context_get_actions (context);
	action = gdk_drag_context_get_suggested_action (context);
	if (action == GDK_ACTION_COPY && (actions & GDK_ACTION_MOVE))
		action = GDK_ACTION_MOVE;

	gdk_drag_status (context, action, time);

	return action != 0;
}

static gboolean
message_list_update_actions_idle_cb (gpointer user_data)
{
	GWeakRef *weak_ref = user_data;
	MessageList *message_list;

	g_return_val_if_fail (weak_ref != NULL, FALSE);

	message_list = g_weak_ref_get (weak_ref);
	if (message_list) {
		message_list->priv->update_actions_idle_id = 0;

		if (!message_list->priv->destroyed)
			g_signal_emit (message_list, signals[UPDATE_ACTIONS], 0, NULL);

		g_object_unref (message_list);
	}

	return FALSE;
}

static void
message_list_schedule_update_actions (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (!message_list->priv->update_actions_idle_id) {
		message_list->priv->update_actions_idle_id =
			g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, message_list_update_actions_idle_cb,
				e_weak_ref_new (message_list), (GDestroyNotify) e_weak_ref_free);
	}
}

static void
on_model_row_changed (ETableModel *model,
                      gint row,
                      MessageList *message_list)
{
	message_list->priv->any_row_changed = TRUE;

	if (e_selection_model_is_row_selected (e_tree_get_selection_model (E_TREE (message_list)), row))
		message_list_schedule_update_actions (message_list);
}

static gboolean
ml_tree_sorting_changed (ETreeTableAdapter *adapter,
                         MessageList *message_list)
{
	gboolean group_by_threads;

	g_return_val_if_fail (message_list != NULL, FALSE);

	group_by_threads = message_list_get_group_by_threads (message_list);

	if (group_by_threads && message_list->frozen == 0) {

		mail_regen_list (message_list, NULL, NULL);

		return TRUE;
	} else if (group_by_threads) {
		message_list->priv->thaw_needs_regen = TRUE;
	}

	return FALSE;
}

static gboolean
ml_get_new_mail_bg_color (ETableItem *item,
			  gint row,
			  gint col,
			  GdkRGBA *inout_background,
			  MessageList *message_list)
{
	CamelMessageInfo *msg_info;
	ETreePath path;

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);
	g_return_val_if_fail (inout_background != NULL, FALSE);

	if (!message_list->priv->new_mail_bg_color || row < 0)
		return FALSE;

	path = e_tree_table_adapter_node_at_row (e_tree_get_table_adapter (E_TREE (message_list)), row);
	if (!path || G_NODE_IS_ROOT ((GNode *) path))
		return FALSE;

	/* retrieve the message information array */
	msg_info = ((GNode *) path)->data;
	g_return_val_if_fail (msg_info != NULL, FALSE);

	if (!(camel_message_info_get_flags (msg_info) & CAMEL_MESSAGE_SEEN)) {
		*inout_background = *(message_list->priv->new_mail_bg_color);
		return TRUE;
	}

	return FALSE;
}

static gboolean
ml_get_bg_color_cb (ETableItem *item,
		    gint row,
		    gint col,
		    GdkRGBA *inout_background,
		    MessageList *message_list)
{
	gboolean was_set = FALSE;

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);
	g_return_val_if_fail (inout_background != NULL, FALSE);

	if (row < 0)
		return FALSE;

	if (e_selection_model_is_row_selected (e_tree_get_selection_model (E_TREE (message_list)), row)) {
		ETableModel *table_model;
		gchar *color_spec;

		table_model = E_TABLE_MODEL (e_tree_get_table_adapter (E_TREE (message_list)));

		color_spec = e_table_model_value_at (table_model, COL_COLOUR, row);
		if (color_spec && gdk_rgba_parse (inout_background, color_spec)) {
			was_set = TRUE;
		}

		if (color_spec)
			e_table_model_free_value (table_model, COL_COLOUR, color_spec);
	}

	if (!was_set)
		was_set = ml_get_new_mail_bg_color (item, row, col, inout_background, message_list);

	return was_set;
}

static void
ml_style_updated_cb (MessageList *message_list)
{
	GdkRGBA *new_mail_fg_color = NULL;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	g_clear_pointer (&message_list->priv->new_mail_bg_color, gdk_rgba_free);
	g_clear_pointer (&message_list->priv->new_mail_fg_color, g_free);

	gtk_widget_style_get (GTK_WIDGET (message_list),
		"new-mail-bg-color", &message_list->priv->new_mail_bg_color,
		"new-mail-fg-color", &new_mail_fg_color,
		NULL);

	if (new_mail_fg_color) {
		message_list->priv->new_mail_fg_color = gdk_rgba_to_string (new_mail_fg_color);

		gdk_rgba_free (new_mail_fg_color);
	}
}

static void
message_list_get_preferred_width (GtkWidget *widget,
				  gint *out_minimum_width,
				  gint *out_natural_width)
{
	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (message_list_parent_class)->get_preferred_width (widget, out_minimum_width, out_natural_width);

	if (out_minimum_width && *out_minimum_width < 50)
		*out_minimum_width = 50;

	if (out_natural_width && out_minimum_width &&
	    *out_natural_width < *out_minimum_width)
		*out_natural_width = *out_minimum_width;
}

static void
message_list_localized_re_changed_cb (GSettings *settings,
				      const gchar *key,
				      gpointer user_data)
{
	MessageList *message_list = user_data;
	gchar *prefixes;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	g_mutex_lock (&message_list->priv->re_prefixes_lock);

	g_strfreev (message_list->priv->re_prefixes);
	prefixes = g_settings_get_string (settings, "composer-localized-re");
	message_list->priv->re_prefixes = g_strsplit (prefixes ? prefixes : "", ",", -1);
	g_free (prefixes);

	g_mutex_unlock (&message_list->priv->re_prefixes_lock);
}

static void
message_list_localized_re_separators_changed_cb (GSettings *settings,
						 const gchar *key,
						 gpointer user_data)
{
	MessageList *message_list = user_data;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	g_mutex_lock (&message_list->priv->re_prefixes_lock);

	g_strfreev (message_list->priv->re_separators);
	message_list->priv->re_separators = g_settings_get_strv (settings, "composer-localized-re-separators");

	if (message_list->priv->re_separators && !*message_list->priv->re_separators) {
		g_strfreev (message_list->priv->re_separators);
		message_list->priv->re_separators = NULL;
	}

	g_mutex_unlock (&message_list->priv->re_prefixes_lock);
}

static void
message_list_user_headers_changed_cb (GSettings *settings,
				      const gchar *key,
				      gpointer user_data)
{
	/* Do it this way, to reuse the localized strings from the message-list.etspec */
	const gchar *default_titles[] = {
		N_("User Header 1"),
		N_("User Header 2"),
		N_("User Header 3")
	};
	MessageList *message_list = user_data;
	ETableSpecification *spec;
	GnomeCanvasItem *header_item;
	ETableHeader *header;
	gchar **user_headers;
	gboolean changed = FALSE;
	guint ii, jj;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	#ifdef ENABLE_MAINTAINER_MODE
	g_warn_if_fail (G_N_ELEMENTS (default_titles) == CAMEL_UTILS_MAX_USER_HEADERS);
	#endif

	spec = e_tree_get_spec (E_TREE (message_list));
	header_item = e_tree_get_header_item (E_TREE (message_list));
	if (header_item)
		g_object_get (header_item, "full-header", &header, NULL);
	else
		header = NULL;

	user_headers = g_settings_get_strv (settings, "camel-message-info-user-headers");

	for (ii = 0, jj = 0; user_headers && user_headers[ii] && jj < CAMEL_UTILS_MAX_USER_HEADERS; ii++) {
		const gchar *header_name = NULL;
		gchar *display_name = NULL;

		camel_util_decode_user_header_setting (user_headers[ii], &display_name, &header_name);

		if (header_name && *header_name) {
			ETableColumnSpecification *col_spec;

			if (g_strcmp0 (message_list->priv->user_headers[jj], header_name) != 0) {
				g_free (message_list->priv->user_headers[jj]);
				message_list->priv->user_headers[jj] = g_strdup (header_name);
				changed = TRUE;
			}

			col_spec = spec ? e_table_specification_get_column_by_model_col (spec, COL_USER_HEADER_1 + jj) : NULL;
			if (col_spec && g_strcmp0 (col_spec->title, display_name && *display_name ? display_name : header_name) != 0) {
				ETableCol *col;

				changed = TRUE;
				g_free (col_spec->title);
				if (display_name && *display_name) {
					col_spec->title = display_name;
					display_name = NULL;
				} else {
					col_spec->title = g_strdup (header_name);
				}

				col = header ? e_table_header_get_column_by_col_idx (header, COL_USER_HEADER_1 + jj) : NULL;
				if (col && g_strcmp0 (col->text, col_spec->title) != 0) {
					g_free (col->text);
					col->text = g_strdup (col_spec->title);
				}
			}

			jj++;
		}

		g_free (display_name);
	}

	message_list->priv->user_headers_count = jj;

	for (ii = jj; ii < CAMEL_UTILS_MAX_USER_HEADERS; ii++) {
		if (message_list->priv->user_headers[ii]) {
			ETableColumnSpecification *col_spec;
			ETableCol *col;
			const gchar *title;

			title = _(default_titles[ii]);

			col_spec = spec ? e_table_specification_get_column_by_model_col (spec, COL_USER_HEADER_1 + jj) : NULL;
			if (col_spec && g_strcmp0 (col_spec->title, title) != 0) {
				g_free (col_spec->title);
				col_spec->title = g_strdup (title);
			}

			col = header ? e_table_header_get_column_by_col_idx (header, COL_USER_HEADER_1 + ii) : NULL;
			if (col && g_strcmp0 (col->text, title) != 0) {
				g_free (col->text);
				col->text = g_strdup (title);
			}

			changed = TRUE;
		}

		g_free (message_list->priv->user_headers[ii]);
		message_list->priv->user_headers[ii] = NULL;
	}

	message_list->priv->user_headers[jj] = NULL;

	g_strfreev (user_headers);

	if (changed)
		gtk_widget_queue_draw (GTK_WIDGET (message_list));
}

static void
message_list_header_click_can_sort_cb (ETree *tree,
				       gboolean *out_header_click_can_sort,
				       gpointer user_data)
{
	MessageList *message_list = MESSAGE_LIST (tree);
	EAutomaticActionPolicy policy;

	policy = g_settings_get_enum (message_list->priv->mail_settings, "message-list-sort-on-header-click");

	if (policy == E_AUTOMATIC_ACTION_POLICY_ALWAYS) {
		*out_header_click_can_sort = TRUE;
	} else if (policy == E_AUTOMATIC_ACTION_POLICY_NEVER) {
		*out_header_click_can_sort = FALSE;
	} else {
		gpointer parent;
		gint response;

		parent = gtk_widget_get_toplevel (GTK_WIDGET (message_list));
		if (!GTK_IS_WINDOW (parent))
			parent = NULL;

		response = e_alert_run_dialog_for_args (parent, "mail:message-list-sort-on-header-click", NULL);
		switch (response) {
		case GTK_RESPONSE_YES:
			*out_header_click_can_sort = TRUE;
			break;
		case GTK_RESPONSE_NO:
			*out_header_click_can_sort = FALSE;
			break;
		case GTK_RESPONSE_ACCEPT:
		case GTK_RESPONSE_CANCEL:
			*out_header_click_can_sort = response == GTK_RESPONSE_ACCEPT;
			g_settings_set_enum (message_list->priv->mail_settings, "message-list-sort-on-header-click",
				*out_header_click_can_sort ? E_AUTOMATIC_ACTION_POLICY_ALWAYS : E_AUTOMATIC_ACTION_POLICY_NEVER);
			break;
		default:
			*out_header_click_can_sort = FALSE;
			break;
		}
	}
}

static void
message_list_set_session (MessageList *message_list,
                          EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (message_list->priv->session == NULL);

	message_list->priv->session = g_object_ref (session);
}

static void
message_list_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER:
			message_list_set_folder (
				MESSAGE_LIST (object),
				g_value_get_object (value));
			return;

		case PROP_GROUP_BY_THREADS:
			message_list_set_group_by_threads (
				MESSAGE_LIST (object),
				g_value_get_boolean (value));
			return;

		case PROP_SESSION:
			message_list_set_session (
				MESSAGE_LIST (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_DELETED:
			message_list_set_show_deleted (
				MESSAGE_LIST (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_JUNK:
			message_list_set_show_junk (
				MESSAGE_LIST (object),
				g_value_get_boolean (value));
			return;

		case PROP_THREAD_LATEST:
			message_list_set_thread_latest (
				MESSAGE_LIST (object),
				g_value_get_boolean (value));
			return;

		case PROP_THREAD_SUBJECT:
			message_list_set_thread_subject (
				MESSAGE_LIST (object),
				g_value_get_boolean (value));
			return;

		case PROP_THREAD_COMPRESS:
			message_list_set_thread_compress (
				MESSAGE_LIST (object),
				g_value_get_boolean (value));
			return;

		case PROP_THREAD_FLAT:
			message_list_set_thread_flat (
				MESSAGE_LIST (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
message_list_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (
				value,
				message_list_get_copy_target_list (
				MESSAGE_LIST (object)));
			return;

		case PROP_FOLDER:
			g_value_take_object (
				value,
				message_list_ref_folder (
				MESSAGE_LIST (object)));
			return;

		case PROP_GROUP_BY_THREADS:
			g_value_set_boolean (
				value,
				message_list_get_group_by_threads (
				MESSAGE_LIST (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (
				value,
				message_list_get_paste_target_list (
				MESSAGE_LIST (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				message_list_get_session (
				MESSAGE_LIST (object)));
			return;

		case PROP_SHOW_DELETED:
			g_value_set_boolean (
				value,
				message_list_get_show_deleted (
				MESSAGE_LIST (object)));
			return;

		case PROP_SHOW_JUNK:
			g_value_set_boolean (
				value,
				message_list_get_show_junk (
				MESSAGE_LIST (object)));
			return;

		case PROP_THREAD_LATEST:
			g_value_set_boolean (
				value,
				message_list_get_thread_latest (
				MESSAGE_LIST (object)));
			return;

		case PROP_THREAD_SUBJECT:
			g_value_set_boolean (
				value,
				message_list_get_thread_subject (
				MESSAGE_LIST (object)));
			return;

		case PROP_THREAD_COMPRESS:
			g_value_set_boolean (
				value,
				message_list_get_thread_compress (
				MESSAGE_LIST (object)));
			return;

		case PROP_THREAD_FLAT:
			g_value_set_boolean (
				value,
				message_list_get_thread_flat (
				MESSAGE_LIST (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
message_list_dispose (GObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	MessageListPrivate *priv;

	priv = message_list->priv;

	if (priv->folder_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->folder,
			priv->folder_changed_handler_id);
		priv->folder_changed_handler_id = 0;
	}

	g_clear_pointer (&priv->copy_target_list, gtk_target_list_unref);
	g_clear_pointer (&priv->paste_target_list, gtk_target_list_unref);

	priv->destroyed = TRUE;

	if (message_list->priv->folder != NULL)
		mail_regen_cancel (message_list);

	g_mutex_lock (&message_list->priv->regen_lock);

	/* This can happen when the regen_idle_source is removed before it's invoked */
	g_clear_object (&message_list->priv->regen_task);

	g_mutex_unlock (&message_list->priv->regen_lock);

	if (message_list->uid_nodemap) {
		g_hash_table_foreach (
			message_list->uid_nodemap,
			(GHFunc) clear_info, message_list);
		g_hash_table_destroy (message_list->uid_nodemap);
		message_list->uid_nodemap = NULL;
	}

	if (priv->mail_settings) {
		g_signal_handlers_disconnect_by_func (priv->mail_settings,
			G_CALLBACK (message_list_localized_re_changed_cb), message_list);

		g_signal_handlers_disconnect_by_func (priv->mail_settings,
			G_CALLBACK (message_list_localized_re_separators_changed_cb), message_list);
	}

	if (priv->eds_settings) {
		g_signal_handlers_disconnect_by_func (priv->eds_settings,
			G_CALLBACK (message_list_user_headers_changed_cb), message_list);
	}

	g_clear_object (&priv->session);
	g_clear_object (&priv->folder);
	g_clear_object (&priv->invisible);
	g_clear_object (&priv->mail_settings);
	g_clear_object (&priv->eds_settings);

	g_clear_object (&message_list->extras);

	if (message_list->idle_id > 0) {
		g_source_remove (message_list->idle_id);
		message_list->idle_id = 0;
	}

	if (message_list->seen_id > 0) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}

	if (priv->update_actions_idle_id) {
		g_source_remove (priv->update_actions_idle_id);
		priv->update_actions_idle_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (message_list_parent_class)->dispose (object);
}

static void
message_list_finalize (GObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	guint ii;

	g_hash_table_destroy (message_list->normalised_hash);

	g_free (message_list->search);
	g_free (message_list->frozen_search);
	g_free (message_list->cursor_uid);
	g_strfreev (message_list->priv->re_prefixes);
	g_strfreev (message_list->priv->re_separators);

	g_mutex_clear (&message_list->priv->regen_lock);
	g_mutex_clear (&message_list->priv->re_prefixes_lock);

	clear_selection (message_list, &message_list->priv->clipboard);

	if (message_list->priv->tree_model_root != NULL)
		extended_g_node_destroy (message_list->priv->tree_model_root);

	g_clear_pointer (&message_list->priv->new_mail_bg_color, gdk_rgba_free);
	g_clear_pointer (&message_list->priv->new_mail_fg_color, g_free);

	for (ii = 0; ii < CAMEL_UTILS_MAX_USER_HEADERS; ii++) {
		g_free (message_list->priv->user_headers[ii]);
		message_list->priv->user_headers[ii] = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (message_list_parent_class)->finalize (object);
}

static void
message_list_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (message_list_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
message_list_selectable_update_actions (ESelectable *selectable,
                                        EFocusTracker *focus_tracker,
                                        GdkAtom *clipboard_targets,
                                        gint n_clipboard_targets)
{
	ETreeTableAdapter *adapter;
	EUIAction *action;
	gint row_count;

	adapter = e_tree_get_table_adapter (E_TREE (selectable));
	row_count = e_table_model_row_count (E_TABLE_MODEL (adapter));

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	e_ui_action_set_tooltip (action, _("Select all visible messages"));
	e_ui_action_set_sensitive (action, row_count > 0);
}

static void
message_list_selectable_select_all (ESelectable *selectable)
{
	message_list_select_all (MESSAGE_LIST (selectable));
}

static ETreePath
message_list_get_root (ETreeModel *tree_model)
{
	MessageList *message_list = MESSAGE_LIST (tree_model);

	return message_list->priv->tree_model_root;
}

static ETreePath
message_list_get_parent (ETreeModel *tree_model,
                         ETreePath path)
{
	return ((GNode *) path)->parent;
}

static ETreePath
message_list_get_first_child (ETreeModel *tree_model,
                              ETreePath path)
{
	return g_node_first_child ((GNode *) path);
}

static ETreePath
message_list_get_next (ETreeModel *tree_model,
                       ETreePath path)
{
	return g_node_next_sibling ((GNode *) path);
}

static gboolean
message_list_is_root (ETreeModel *tree_model,
                      ETreePath path)
{
	return G_NODE_IS_ROOT ((GNode *) path);
}

static gboolean
message_list_is_expandable (ETreeModel *tree_model,
                            ETreePath path)
{
	return (g_node_first_child ((GNode *) path) != NULL);
}

static guint
message_list_get_n_nodes (ETreeModel *tree_model)
{
	ETreePath root;

	root = e_tree_model_get_root (tree_model);

	if (root == NULL)
		return 0;

	/* The root node is an empty placeholder, so
	 * subtract one from the count to exclude it. */

	return g_node_n_nodes ((GNode *) root, G_TRAVERSE_ALL) - 1;
}

static guint
message_list_get_n_children (ETreeModel *tree_model,
                             ETreePath path)
{
	return g_node_n_children ((GNode *) path);
}

static guint
message_list_depth (ETreeModel *tree_model,
                    ETreePath path)
{
	MessageList *message_list = MESSAGE_LIST (tree_model);
	guint depth;

	if (message_list_get_thread_compress (message_list) &&
	    !message_list_get_thread_flat (message_list)) {
		GNode *node = ((GNode *) path);

		depth = 1;

		while (node && !G_NODE_IS_ROOT (node)) {
			if (!node->children || node->prev || node->next || G_NODE_IS_ROOT (node->parent) ||
			    (node->parent && (node->parent->prev || node->parent->next || G_NODE_IS_ROOT (node->parent->parent))))
				depth++;

			node = node->parent;
		}
	} else {
		depth = g_node_depth ((GNode *) path);
	}

	return depth;
}

static gboolean
message_list_get_expanded_default (ETreeModel *tree_model)
{
	MessageList *message_list = MESSAGE_LIST (tree_model);

	return message_list->priv->expanded_default;
}

static gint
message_list_column_count (ETreeModel *tree_model)
{
	return COL_LAST;
}

static gchar *
message_list_get_save_id (ETreeModel *tree_model,
                          ETreePath path)
{
	CamelMessageInfo *info;

	if (G_NODE_IS_ROOT ((GNode *) path))
		return g_strdup ("root");

	/* Note: ETable can ask for the save_id while we're clearing
	 *       it, which is the only time info should be NULL. */
	info = ((GNode *) path)->data;
	if (info == NULL)
		return NULL;

	return g_strdup (camel_message_info_get_uid (info));
}

static ETreePath
message_list_get_node_by_id (ETreeModel *tree_model,
                             const gchar *save_id)
{
	MessageList *message_list;

	message_list = MESSAGE_LIST (tree_model);

	if (!strcmp (save_id, "root"))
		return e_tree_model_get_root (tree_model);

	return g_hash_table_lookup (message_list->uid_nodemap, save_id);
}

static gpointer
message_list_sort_value_at (ETreeModel *tree_model,
                            ETreePath path,
                            gint col)
{
	MessageList *message_list;
	GNode *path_node;
	struct LatestData ld;
	gint64 *res;

	message_list = MESSAGE_LIST (tree_model);

	if (!(col == COL_SENT || col == COL_RECEIVED))
		return e_tree_model_value_at (tree_model, path, col);

	path_node = (GNode *) path;

	if (G_NODE_IS_ROOT (path_node))
		return NULL;

	ld.sent = (col == COL_SENT);
	ld.latest = 0;

	latest_foreach (tree_model, path, &ld);
	if (message_list->priv->thread_latest && !message_list->priv->thread_flat &&
	    (!e_tree_get_sort_children_ascending (E_TREE (message_list)) ||
	    !path_node || !path_node->parent || !path_node->parent->parent))
		e_tree_model_node_traverse (
			tree_model, path, latest_foreach, &ld);

	res = g_new0 (gint64, 1);
	*res = (gint64) ld.latest;

	return res;
}

static gpointer
message_list_value_at (ETreeModel *tree_model,
                       ETreePath path,
                       gint col)
{
	MessageList *message_list;
	CamelMessageInfo *msg_info;
	gpointer result;

	message_list = MESSAGE_LIST (tree_model);

	if (!path || G_NODE_IS_ROOT ((GNode *) path))
		return NULL;

	/* retrieve the message information array */
	msg_info = ((GNode *) path)->data;
	g_return_val_if_fail (msg_info != NULL, NULL);

	camel_message_info_property_lock (msg_info);
	result = ml_tree_value_at_ex (tree_model, path, col, msg_info, message_list);
	camel_message_info_property_unlock (msg_info);

	return result;
}

static gpointer
message_list_duplicate_value (ETreeModel *tree_model,
                              gint col,
                              gconstpointer value)
{
	switch (col) {
		case COL_MESSAGE_STATUS:
		case COL_FLAGGED:
		case COL_SCORE:
		case COL_ATTACHMENT:
		case COL_DELETED:
		case COL_DELETED_OR_JUNK:
		case COL_JUNK:
		case COL_JUNK_STRIKEOUT_COLOR:
		case COL_UNREAD:
		case COL_SIZE:
		case COL_FOLLOWUP_FLAG:
		case COL_FOLLOWUP_FLAG_STATUS:
			return (gpointer) value;

		case COL_UID:
			return (gpointer) camel_pstring_strdup (value);

		case COL_FROM:
		case COL_SUBJECT:
		case COL_TO:
		case COL_SENDER:
		case COL_SENDER_MAIL:
		case COL_RECIPIENTS:
		case COL_RECIPIENTS_MAIL:
		case COL_MIXED_SENDER:
		case COL_MIXED_RECIPIENTS:
		case COL_LOCATION:
		case COL_LABELS:
		case COL_USER_HEADER_1:
		case COL_USER_HEADER_2:
		case COL_USER_HEADER_3:
		case COL_BODY_PREVIEW:
		case COL_SUBJECT_WITH_BODY_PREVIEW:
		case COL_CORRESPONDENTS:
			return g_strdup (value);

		case COL_SENT:
		case COL_RECEIVED:
		case COL_FOLLOWUP_DUE_BY:
			if (value) {
				gint64 *res;
				const gint64 *pvalue = value;

				res = g_new0 (gint64, 1);
				*res = *pvalue;

				return res;
			} else
				return NULL;

		default:
			g_return_val_if_reached (NULL);
	}
}

static void
message_list_free_value (ETreeModel *tree_model,
                         gint col,
                         gpointer value)
{
	switch (col) {
		case COL_MESSAGE_STATUS:
		case COL_FLAGGED:
		case COL_SCORE:
		case COL_ATTACHMENT:
		case COL_DELETED:
		case COL_DELETED_OR_JUNK:
		case COL_JUNK:
		case COL_JUNK_STRIKEOUT_COLOR:
		case COL_UNREAD:
		case COL_SIZE:
		case COL_FOLLOWUP_FLAG:
		case COL_FOLLOWUP_FLAG_STATUS:
		case COL_FROM:
		case COL_FROM_NORM:
		case COL_TO:
		case COL_TO_NORM:
		case COL_SUBJECT:
		case COL_SUBJECT_NORM:
		case COL_SUBJECT_TRIMMED:
		case COL_COLOUR:
		case COL_ITALIC:
		case COL_CORRESPONDENTS:
			break;

		case COL_UID:
			camel_pstring_free (value);
			break;

		case COL_LOCATION:
		case COL_SENDER:
		case COL_SENDER_MAIL:
		case COL_RECIPIENTS:
		case COL_RECIPIENTS_MAIL:
		case COL_MIXED_SENDER:
		case COL_MIXED_RECIPIENTS:
		case COL_LABELS:
		case COL_SENT:
		case COL_RECEIVED:
		case COL_FOLLOWUP_DUE_BY:
		case COL_USER_HEADER_1:
		case COL_USER_HEADER_2:
		case COL_USER_HEADER_3:
		case COL_BODY_PREVIEW:
		case COL_SUBJECT_WITH_BODY_PREVIEW:
			g_free (value);
			break;

		default:
			g_warn_if_reached ();
	}
}

static gpointer
message_list_initialize_value (ETreeModel *tree_model,
                               gint col)
{
	switch (col) {
		case COL_MESSAGE_STATUS:
		case COL_FLAGGED:
		case COL_SCORE:
		case COL_ATTACHMENT:
		case COL_DELETED:
		case COL_DELETED_OR_JUNK:
		case COL_JUNK:
		case COL_JUNK_STRIKEOUT_COLOR:
		case COL_UNREAD:
		case COL_SENT:
		case COL_RECEIVED:
		case COL_SIZE:
		case COL_FROM:
		case COL_SUBJECT:
		case COL_TO:
		case COL_FOLLOWUP_FLAG:
		case COL_FOLLOWUP_FLAG_STATUS:
		case COL_FOLLOWUP_DUE_BY:
		case COL_UID:
		case COL_USER_HEADER_1:
		case COL_USER_HEADER_2:
		case COL_USER_HEADER_3:
		case COL_BODY_PREVIEW:
		case COL_SUBJECT_WITH_BODY_PREVIEW:
		case COL_CORRESPONDENTS:
			return NULL;

		case COL_LOCATION:
		case COL_SENDER:
		case COL_SENDER_MAIL:
		case COL_RECIPIENTS:
		case COL_RECIPIENTS_MAIL:
		case COL_MIXED_SENDER:
		case COL_MIXED_RECIPIENTS:
		case COL_LABELS:
			return g_strdup ("");

		default:
			g_return_val_if_reached (NULL);
	}
}

static gboolean
message_list_value_is_empty (ETreeModel *tree_model,
                             gint col,
                             gconstpointer value)
{
	switch (col) {
		case COL_MESSAGE_STATUS:
		case COL_FLAGGED:
		case COL_SCORE:
		case COL_ATTACHMENT:
		case COL_DELETED:
		case COL_DELETED_OR_JUNK:
		case COL_JUNK:
		case COL_JUNK_STRIKEOUT_COLOR:
		case COL_UNREAD:
		case COL_SENT:
		case COL_RECEIVED:
		case COL_SIZE:
		case COL_FOLLOWUP_FLAG_STATUS:
		case COL_FOLLOWUP_DUE_BY:
			return value == NULL;

		case COL_FROM:
		case COL_SUBJECT:
		case COL_TO:
		case COL_FOLLOWUP_FLAG:
		case COL_LOCATION:
		case COL_SENDER:
		case COL_SENDER_MAIL:
		case COL_RECIPIENTS:
		case COL_RECIPIENTS_MAIL:
		case COL_MIXED_SENDER:
		case COL_MIXED_RECIPIENTS:
		case COL_LABELS:
		case COL_UID:
		case COL_USER_HEADER_1:
		case COL_USER_HEADER_2:
		case COL_USER_HEADER_3:
		case COL_BODY_PREVIEW:
		case COL_SUBJECT_WITH_BODY_PREVIEW:
		case COL_CORRESPONDENTS:
			return !(value && *((gchar *) value));

		default:
			g_return_val_if_reached (FALSE);
	}
}

static gchar *
message_list_value_to_string (ETreeModel *tree_model,
                              gint col,
                              gconstpointer value)
{
	guint ii;

	switch (col) {
		case COL_MESSAGE_STATUS:
			ii = GPOINTER_TO_UINT (value);
			if (ii > 5)
				return g_strdup ("");
			return g_strdup (status_map[ii]);

		case COL_SCORE:
			ii = GPOINTER_TO_UINT (value) + 3;
			if (ii > 6)
				ii = 3;
			return g_strdup (score_map[ii]);

		case COL_ATTACHMENT:
		case COL_FLAGGED:
		case COL_DELETED:
		case COL_DELETED_OR_JUNK:
		case COL_JUNK:
		case COL_JUNK_STRIKEOUT_COLOR:
		case COL_UNREAD:
		case COL_FOLLOWUP_FLAG_STATUS:
			ii = GPOINTER_TO_UINT (value);
			return g_strdup_printf ("%u", ii);

		case COL_SENT:
		case COL_RECEIVED:
		case COL_FOLLOWUP_DUE_BY:
			return filter_date (value);

		case COL_SIZE:
			return filter_size (GPOINTER_TO_INT (value));

		case COL_FROM:
		case COL_SUBJECT:
		case COL_TO:
		case COL_FOLLOWUP_FLAG:
		case COL_LOCATION:
		case COL_SENDER:
		case COL_SENDER_MAIL:
		case COL_RECIPIENTS:
		case COL_RECIPIENTS_MAIL:
		case COL_MIXED_SENDER:
		case COL_MIXED_RECIPIENTS:
		case COL_LABELS:
		case COL_UID:
		case COL_USER_HEADER_1:
		case COL_USER_HEADER_2:
		case COL_USER_HEADER_3:
		case COL_BODY_PREVIEW:
		case COL_SUBJECT_WITH_BODY_PREVIEW:
		case COL_CORRESPONDENTS:
			return g_strdup (value);

		default:
			g_return_val_if_reached (NULL);
	}

}

static void
message_list_class_init (MessageListClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	if (!ml_drag_info[0].atom) {
		gint ii;

		for (ii = 0; ii < G_N_ELEMENTS (ml_drag_info); ii++) {
			ml_drag_info[ii].atom = gdk_atom_intern (ml_drag_info[ii].target, FALSE);
		}

		for (ii = 0; ii < G_N_ELEMENTS (status_map); ii++) {
			status_map[ii] = _(status_map[ii]);
		}

		for (ii = 0; ii < G_N_ELEMENTS (score_map); ii++) {
			score_map[ii] = _(score_map[ii]);
		}
	}

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = message_list_get_preferred_width;

	gtk_widget_class_set_css_name (widget_class, "MessageList");

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = message_list_set_property;
	object_class->get_property = message_list_get_property;
	object_class->dispose = message_list_dispose;
	object_class->finalize = message_list_finalize;
	object_class->constructed = message_list_constructed;

	class->message_list_built = NULL;

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	g_object_class_install_property (
		object_class,
		PROP_FOLDER,
		g_param_spec_object (
			"folder",
			"Folder",
			"The source folder",
			CAMEL_TYPE_FOLDER,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_GROUP_BY_THREADS,
		g_param_spec_boolean (
			"group-by-threads",
			"Group By Threads",
			"Group messages into conversation threads",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			"Mail Session",
			"The mail session",
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_DELETED,
		g_param_spec_boolean (
			"show-deleted",
			"Show Deleted",
			"Show messages marked for deletion",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_JUNK,
		g_param_spec_boolean (
			"show-junk",
			"Show Junk",
			"Show messages marked as junk",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_THREAD_LATEST,
		g_param_spec_boolean (
			"thread-latest",
			"Thread Latest",
			"Sort threads by latest message",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_THREAD_SUBJECT,
		g_param_spec_boolean (
			"thread-subject",
			"Thread Subject",
			"Thread messages by Subject headers",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_THREAD_COMPRESS,
		g_param_spec_boolean (
			"thread-compress",
			"Thread Compress",
			"Compress flat threads",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_THREAD_FLAT,
		g_param_spec_boolean (
			"thread-flat",
			"Thread Flat",
			"Generate flat threads",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	gtk_widget_class_install_style_property (
		GTK_WIDGET_CLASS (class),
		g_param_spec_boxed (
			"new-mail-bg-color",
			"New Mail Background Color",
			"Background color to use for new mails",
			GDK_TYPE_RGBA,
			G_PARAM_READABLE));

	gtk_widget_class_install_style_property (
		GTK_WIDGET_CLASS (class),
		g_param_spec_boxed (
			"new-mail-fg-color",
			"New Mail Foreground Color",
			"Foreground color to use for new mails",
			GDK_TYPE_RGBA,
			G_PARAM_READABLE));

	signals[MESSAGE_SELECTED] = g_signal_new (
		"message_selected",
		MESSAGE_LIST_TYPE,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (MessageListClass, message_selected),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	signals[MESSAGE_LIST_BUILT] = g_signal_new (
		"message_list_built",
		MESSAGE_LIST_TYPE,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (MessageListClass, message_list_built),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		MESSAGE_LIST_TYPE,
		G_SIGNAL_RUN_LAST,
		0, /* G_STRUCT_OFFSET (MessageListClass, update_actions), */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
message_list_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = message_list_selectable_update_actions;
	iface->select_all = message_list_selectable_select_all;
}

static void
message_list_tree_model_init (ETreeModelInterface *iface)
{
	iface->get_root = message_list_get_root;
	iface->get_parent = message_list_get_parent;
	iface->get_first_child = message_list_get_first_child;
	iface->get_next = message_list_get_next;
	iface->is_root = message_list_is_root;
	iface->is_expandable = message_list_is_expandable;
	iface->get_n_nodes = message_list_get_n_nodes;
	iface->get_n_children = message_list_get_n_children;
	iface->depth = message_list_depth;
	iface->get_expanded_default = message_list_get_expanded_default;
	iface->column_count = message_list_column_count;
	iface->get_save_id = message_list_get_save_id;
	iface->get_node_by_id = message_list_get_node_by_id;
	iface->sort_value_at = message_list_sort_value_at;
	iface->value_at = message_list_value_at;
	iface->duplicate_value = message_list_duplicate_value;
	iface->free_value = message_list_free_value;
	iface->initialize_value = message_list_initialize_value;
	iface->value_is_empty = message_list_value_is_empty;
	iface->value_to_string = message_list_value_to_string;
}

static void
message_list_init (MessageList *message_list)
{
	MessageListPrivate *p;
	GtkTargetList *target_list;
	GdkAtom matom;

	message_list->priv = message_list_get_instance_private (message_list);

	message_list->normalised_hash = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) e_poolv_destroy);

	message_list->uid_nodemap = g_hash_table_new (g_str_hash, g_str_equal);

	message_list->cursor_uid = NULL;
	message_list->last_sel_single = FALSE;

	g_mutex_init (&message_list->priv->regen_lock);
	g_mutex_init (&message_list->priv->re_prefixes_lock);

	/* TODO: Should this only get the selection if we're realised? */
	p = message_list->priv;
	p->invisible = gtk_invisible_new ();
	p->destroyed = FALSE;
	g_object_ref_sink (p->invisible);
	p->any_row_changed = FALSE;

	matom = gdk_atom_intern ("x-uid-list", FALSE);
	gtk_selection_add_target (p->invisible, GDK_SELECTION_CLIPBOARD, matom, 0);
	gtk_selection_add_target (p->invisible, GDK_SELECTION_CLIPBOARD, GDK_SELECTION_TYPE_STRING, 2);

	g_signal_connect (
		p->invisible, "selection_get",
		G_CALLBACK (ml_selection_get), message_list);
	g_signal_connect (
		p->invisible, "selection_clear_event",
		G_CALLBACK (ml_selection_clear_event), message_list);
	g_signal_connect (
		p->invisible, "selection_received",
		G_CALLBACK (ml_selection_received), message_list);

	/* FIXME This is currently unused. */
	target_list = gtk_target_list_new (NULL, 0);
	message_list->priv->copy_target_list = target_list;

	/* FIXME This is currently unused. */
	target_list = gtk_target_list_new (NULL, 0);
	message_list->priv->paste_target_list = target_list;

	message_list->priv->mail_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	message_list->priv->eds_settings = e_util_ref_settings ("org.gnome.evolution-data-server");
	message_list->priv->re_prefixes = NULL;
	message_list->priv->re_separators = NULL;
	message_list->priv->group_by_threads = TRUE;
	message_list->priv->new_mail_bg_color = NULL;
	message_list->priv->new_mail_fg_color = NULL;

	g_signal_connect (message_list->priv->mail_settings, "changed::composer-localized-re",
		G_CALLBACK (message_list_localized_re_changed_cb), message_list);

	g_signal_connect (message_list->priv->mail_settings, "changed::composer-localized-re-separators",
		G_CALLBACK (message_list_localized_re_separators_changed_cb), message_list);

	message_list_localized_re_changed_cb (message_list->priv->mail_settings, NULL, message_list);
	message_list_localized_re_separators_changed_cb (message_list->priv->mail_settings, NULL, message_list);

	g_signal_connect (message_list->priv->eds_settings, "changed::camel-message-info-user-headers",
		G_CALLBACK (message_list_user_headers_changed_cb), message_list);

	g_signal_connect (message_list, "header-click-can-sort",
		G_CALLBACK (message_list_header_click_can_sort_cb), NULL);
}

static void
message_list_construct (MessageList *message_list)
{
	ETreeTableAdapter *adapter;
	ETableSpecification *specification;
	ETableItem *item;
	AtkObject *a11y;
	gboolean constructed;
	gchar *etspecfile;
	GError *local_error = NULL;

	/*
	 * The etree
	 */
	message_list->extras = message_list_create_extras (message_list->priv->mail_settings);

	etspecfile = g_build_filename (
		EVOLUTION_ETSPECDIR, "message-list.etspec", NULL);
	specification = e_table_specification_new (etspecfile, &local_error);

	/* Failure here is fatal. */
	if (local_error != NULL) {
		g_error ("%s: %s", etspecfile, local_error->message);
		g_return_if_reached ();
	}

	constructed = e_tree_construct (
		E_TREE (message_list),
		E_TREE_MODEL (message_list),
		message_list->extras, specification);

	g_object_unref (specification);
	g_free (etspecfile);

	adapter = e_tree_get_table_adapter (E_TREE (message_list));

	if (constructed)
		e_tree_table_adapter_root_node_set_visible (adapter, FALSE);

	if (atk_get_root () != NULL) {
		a11y = gtk_widget_get_accessible (GTK_WIDGET (message_list));
		atk_object_set_name (a11y, _("Messages"));
	}

	g_signal_connect (
		adapter, "model_row_changed",
		G_CALLBACK (on_model_row_changed), message_list);

	g_signal_connect (
		message_list, "cursor_activated",
		G_CALLBACK (on_cursor_activated_cmd), message_list);

	g_signal_connect (
		message_list, "click",
		G_CALLBACK (on_click), message_list);

	g_signal_connect (
		message_list, "selection_change",
		G_CALLBACK (on_selection_changed_cmd), message_list);

	e_tree_drag_source_set (
		E_TREE (message_list), GDK_BUTTON1_MASK,
		ml_drag_types, G_N_ELEMENTS (ml_drag_types),
		GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect (
		message_list, "tree_drag_data_get",
		G_CALLBACK (ml_tree_drag_data_get), message_list);

	gtk_drag_dest_set (
		GTK_WIDGET (message_list),
		GTK_DEST_DEFAULT_ALL,
		ml_drop_types,
		G_N_ELEMENTS (ml_drop_types),
		GDK_ACTION_MOVE |
		GDK_ACTION_COPY);

	g_signal_connect (
		message_list, "tree_drag_data_received",
		G_CALLBACK (ml_tree_drag_data_received), message_list);

	g_signal_connect (
		message_list, "drag-motion",
		G_CALLBACK (ml_tree_drag_motion), message_list);

	g_signal_connect (
		adapter, "sorting_changed",
		G_CALLBACK (ml_tree_sorting_changed), message_list);

	item = e_tree_get_item (E_TREE (message_list));
	g_signal_connect (item, "get-bg-color",
		G_CALLBACK (ml_get_bg_color_cb), message_list);

	g_signal_connect (message_list, "realize",
		G_CALLBACK (ml_style_updated_cb), NULL);

	g_signal_connect (message_list, "style-updated",
		G_CALLBACK (ml_style_updated_cb), NULL);

	message_list_user_headers_changed_cb (message_list->priv->eds_settings, NULL, message_list);
}

/**
 * message_list_new:
 *
 * Creates a new message-list widget.
 *
 * Returns a new message-list widget.
 **/
GtkWidget *
message_list_new (EMailSession *session)
{
	GtkWidget *message_list;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	message_list = g_object_new (
		message_list_get_type (),
		"session", session, NULL);

	message_list_construct (MESSAGE_LIST (message_list));

	return message_list;
}

EMailSession *
message_list_get_session (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), NULL);

	return message_list->priv->session;
}

static void
clear_info (gchar *key,
            GNode *node,
            MessageList *message_list)
{
	g_clear_object (&node->data);
}

static void
clear_tree (MessageList *message_list,
            gboolean tfree)
{
	ETreeModel *tree_model;
	CamelFolder *folder;

#ifdef TIMEIT
	struct timeval start, end;
	gulong diff;

	printf ("Clearing tree\n");
	gettimeofday (&start, NULL);
#endif

	tree_model = E_TREE_MODEL (message_list);

	/* we also reset the uid_rowmap since it is no longer useful/valid anyway */
	folder = message_list_ref_folder (message_list);
	if (folder != NULL)
		g_hash_table_foreach (
			message_list->uid_nodemap,
			(GHFunc) clear_info, message_list);
	g_hash_table_destroy (message_list->uid_nodemap);
	message_list->uid_nodemap = g_hash_table_new (g_str_hash, g_str_equal);
	g_clear_object (&folder);

	message_list->priv->newest_read_date = 0;
	message_list->priv->newest_read_uid = NULL;
	message_list->priv->oldest_unread_date = 0;
	message_list->priv->oldest_unread_uid = NULL;

	if (message_list->priv->tree_model_root != NULL) {
		/* we should be frozen already */
		message_list_tree_model_remove (
			message_list, message_list->priv->tree_model_root);
	}

	e_tree_table_adapter_clear_nodes_silent (e_tree_get_table_adapter (E_TREE (message_list)));

	/* Create a new placeholder root node. */
	message_list_tree_model_insert (message_list, NULL, 0, NULL);
	g_warn_if_fail (message_list->priv->tree_model_root != NULL);

	/* Also reset cursor node, it had been just erased */
	e_tree_set_cursor (E_TREE (message_list), message_list->priv->tree_model_root);

	if (tfree)
		e_tree_model_rebuilt (tree_model);
#ifdef TIMEIT
	gettimeofday (&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec / 1000;
	diff -= start.tv_sec * 1000 + start.tv_usec / 1000;
	printf ("Clearing tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif
}

static gboolean
message_list_folder_filters_system_flag (const gchar *expr,
					 const gchar *flag)
{
	const gchar *pos;

	if (!expr || !*expr)
		return FALSE;

	g_return_val_if_fail (flag && *flag, FALSE);

	while (pos = strstr (expr, flag), pos) {
		/* This is searching for something like 'system-flag "' + flag + '"'
		   in the expression, without fully parsing it. */
		if (pos > expr && pos[-1] == '\"' && pos[strlen(flag)] == '\"') {
			const gchar *system_flag = "system-flag";
			gint ii = 2, jj = strlen (system_flag) - 1;

			while (pos - ii >= expr && g_ascii_isspace (pos[-ii]))
				ii++;

			while (pos - ii >= expr && jj >= 0 && system_flag[jj] == pos[-ii]) {
				ii++;
				jj--;
			}

			if (jj == -1)
				return TRUE;
		}

		expr = pos + 1;
	}

	return FALSE;
}

static gboolean
folder_store_supports_vjunk_folder (CamelFolder *folder)
{
	CamelStore *store;

	g_return_val_if_fail (folder != NULL, FALSE);

	store = camel_folder_get_parent_store (folder);
	if (store == NULL)
		return FALSE;

	if (CAMEL_IS_VEE_FOLDER (folder))
		return TRUE;

	if (camel_store_get_flags (store) & CAMEL_STORE_VJUNK)
		return TRUE;

	if (camel_store_get_flags (store) & CAMEL_STORE_REAL_JUNK_FOLDER)
		return TRUE;

	return FALSE;
}

static gboolean
message_list_get_hide_junk (MessageList *message_list,
                            CamelFolder *folder)
{
	guint32 folder_flags;

	if (folder == NULL)
		return FALSE;

	if (message_list_get_show_junk (message_list))
		return FALSE;

	if (!folder_store_supports_vjunk_folder (folder))
		return FALSE;

	folder_flags = camel_folder_get_flags (folder);

	if (folder_flags & CAMEL_FOLDER_IS_JUNK)
		return FALSE;

	if (folder_flags & CAMEL_FOLDER_IS_TRASH)
		return FALSE;

	if (CAMEL_IS_VEE_FOLDER (folder)) {
		const gchar *expr = camel_vee_folder_get_expression (CAMEL_VEE_FOLDER (folder));
		if (message_list_folder_filters_system_flag (expr, "Junk"))
			return FALSE;
	}

	return TRUE;
}

static gboolean
message_list_get_hide_deleted (MessageList *message_list,
                               CamelFolder *folder)
{
	CamelStore *store;
	gboolean non_trash_folder;

	if (folder == NULL)
		return FALSE;

	if (message_list_get_show_deleted (message_list))
		return FALSE;

	store = camel_folder_get_parent_store (folder);
	g_return_val_if_fail (store != NULL, FALSE);

	non_trash_folder =
		((camel_store_get_flags (store) & CAMEL_STORE_VTRASH) == 0) ||
		((camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_TRASH) == 0);

	if (non_trash_folder && CAMEL_IS_VEE_FOLDER (folder)) {
		const gchar *expr = camel_vee_folder_get_expression (CAMEL_VEE_FOLDER (folder));
		if (message_list_folder_filters_system_flag (expr, "Deleted"))
			return FALSE;
	}

	return non_trash_folder;
}

/* Check if the given node is selectable in the current message list,
 * which depends on the type of the folder (normal, junk, trash). */
static gboolean
is_node_selectable (MessageList *message_list,
		    CamelMessageInfo *info,
		    GHashTable *removed_uids)
{
	CamelFolder *folder;
	gboolean is_junk_folder;
	gboolean is_trash_folder;
	guint32 flags, folder_flags;
	gboolean flag_junk;
	gboolean flag_deleted;
	gboolean hide_junk;
	gboolean hide_deleted;
	gboolean store_has_vjunk;
	gboolean selectable = FALSE;

	g_return_val_if_fail (info != NULL, FALSE);

	if (removed_uids && g_hash_table_contains (removed_uids, camel_message_info_get_uid (info)))
		return FALSE;

	folder = message_list_ref_folder (message_list);
	g_return_val_if_fail (folder != NULL, FALSE);

	store_has_vjunk = folder_store_supports_vjunk_folder (folder);
	folder_flags = camel_folder_get_flags (folder);

	/* check folder type */
	is_junk_folder = store_has_vjunk && (folder_flags & CAMEL_FOLDER_IS_JUNK) != 0;
	is_trash_folder = folder_flags & CAMEL_FOLDER_IS_TRASH;

	hide_junk = message_list_get_hide_junk (message_list, folder);
	hide_deleted = message_list_get_hide_deleted (message_list, folder);

	g_object_unref (folder);

	/* check flags set on current message */
	flags = camel_message_info_get_flags (info);
	flag_junk = store_has_vjunk && (flags & CAMEL_MESSAGE_JUNK) != 0;
	flag_deleted = flags & CAMEL_MESSAGE_DELETED;

	/* perform actions depending on folder type */
	if (is_junk_folder) {
		/* messages in a junk folder are selectable only if
		 * the message is marked as junk and if not deleted
		 * when hide_deleted is set */
		if (flag_junk && !(flag_deleted && hide_deleted))
			selectable = TRUE;

	} else if (is_trash_folder) {
		/* messages in a trash folder are selectable unless
		 * not deleted any more */
		if (flag_deleted)
			selectable = TRUE;
	} else {
		/* in normal folders it depends on hide_deleted,
		 * hide_junk and the message flags */
		if (!(flag_junk && hide_junk)
		    && !(flag_deleted && hide_deleted))
			selectable = TRUE;
	}

	return selectable;
}

/* We try and find something that is selectable in our tree.  There is
 * actually no assurance that we'll find something that will still be
 * there next time, but its probably going to work most of the time. */
static gchar *
find_next_selectable (MessageList *message_list,
		      GHashTable *removed_uids)
{
	ETreeTableAdapter *adapter;
	GNode *node;
	CamelMessageInfo *info;
	GSettings *settings;
	gint vrow_orig;
	gint vrow;
	gint vrow_inc;
	gint row_count;

	node = g_hash_table_lookup (
		message_list->uid_nodemap,
		message_list->cursor_uid);
	if (node == NULL)
		return NULL;

	info = get_message_info (message_list, node);
	if (info && is_node_selectable (message_list, info, removed_uids))
		return NULL;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));
	row_count = e_table_model_row_count (E_TABLE_MODEL (adapter));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (settings, "delete-selects-previous"))
		vrow_inc = -1;
	else
		vrow_inc = 1;
	g_clear_object (&settings);

	/* model_to_view_row etc simply don't work for sorted views.  Sigh. */
	vrow_orig = e_tree_table_adapter_row_of_node (adapter, node);

	/* We already checked this node. */
	vrow = vrow_orig + vrow_inc;

	while (vrow >= 0 && vrow < row_count) {
		node = e_tree_table_adapter_node_at_row (adapter, vrow);
		info = get_message_info (message_list, node);
		if (info && is_node_selectable (message_list, info, removed_uids))
			return g_strdup (camel_message_info_get_uid (info));
		vrow += vrow_inc;
	}

	vrow_inc = vrow_inc * (-1);

	/* We didn't find any undeleted entries _below_ the currently selected one
	 * so let's try to find one _above_ */
	vrow = vrow_orig + vrow_inc;

	while (vrow >= 0 && vrow < row_count) {
		node = e_tree_table_adapter_node_at_row (adapter, vrow);
		info = get_message_info (message_list, node);
		if (info && is_node_selectable (message_list, info, removed_uids))
			return g_strdup (camel_message_info_get_uid (info));
		vrow += vrow_inc;
	}

	return NULL;
}

static GNode *
ml_uid_nodemap_insert (MessageList *message_list,
                       CamelMessageInfo *info,
                       GNode *parent,
                       gint row)
{
	GNode *node;
	const gchar *uid;
	time_t date;
	guint flags;

	if (parent == NULL)
		parent = message_list->priv->tree_model_root;

	node = message_list_tree_model_insert (
		message_list, parent, row, info);

	uid = camel_message_info_get_uid (info);
	flags = camel_message_info_get_flags (info);
	date = camel_message_info_get_date_received (info);

	g_object_ref (info);
	g_hash_table_insert (message_list->uid_nodemap, (gpointer) uid, node);

	/* Track the latest seen and unseen messages shown, used in
	 * fallback heuristics for automatic message selection. */
	if (flags & CAMEL_MESSAGE_SEEN) {
		if (date > message_list->priv->newest_read_date) {
			message_list->priv->newest_read_date = date;
			message_list->priv->newest_read_uid = uid;
		}
	} else {
		if (message_list->priv->oldest_unread_date == 0) {
			message_list->priv->oldest_unread_date = date;
			message_list->priv->oldest_unread_uid = uid;
		} else if (date < message_list->priv->oldest_unread_date) {
			message_list->priv->oldest_unread_date = date;
			message_list->priv->oldest_unread_uid = uid;
		}
	}

	return node;
}

/* only call if we have a tree model */
/* builds the tree structure */

static void	build_subtree			(MessageList *message_list,
						 GNode *parent,
						 CamelFolderThreadNode *c,
						 gboolean thread_flat,
						 gboolean thread_latest,
						 gint *row);

static void
build_tree (MessageList *message_list,
            CamelFolderThread *thread,
	    gboolean thread_flat,
	    gboolean thread_latest,
            gboolean folder_changed)
{
	gint row = 0;
	ETableItem *table_item = e_tree_get_item (E_TREE (message_list));
#ifdef TIMEIT
	struct timeval start, end;
	gulong diff;

	printf ("Building tree\n");
	gettimeofday (&start, NULL);
#endif

#ifdef TIMEIT
	gettimeofday (&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec / 1000;
	diff -= start.tv_sec * 1000 + start.tv_usec / 1000;
	printf ("Loading tree state took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

	if (message_list->priv->tree_model_root == NULL) {
		message_list_tree_model_insert (message_list, NULL, 0, NULL);
		g_warn_if_fail (message_list->priv->tree_model_root != NULL);
	}

	if (table_item)
		e_table_item_freeze (table_item);

	message_list_tree_model_freeze (message_list);

	clear_tree (message_list, FALSE);

	build_subtree (
		message_list,
		message_list->priv->tree_model_root,
		thread ? camel_folder_thread_get_tree (thread) : NULL,
		thread_flat,
		thread_latest,
		&row);

	message_list_tree_model_thaw (message_list);

	if (table_item) {
		/* Show the cursor unless we're responding to a
		 * "folder-changed" signal from our CamelFolder. */
		if (folder_changed)
			table_item->queue_show_cursor = FALSE;
		e_table_item_thaw (table_item);
	}

#ifdef TIMEIT
	gettimeofday (&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec / 1000;
	diff -= start.tv_sec * 1000 + start.tv_usec / 1000;
	printf ("Building tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif
}

static void
build_subtree (MessageList *message_list,
               GNode *parent,
               CamelFolderThreadNode *c,
	       gboolean thread_flat,
	       gboolean thread_latest,
               gint *row)
{
	GNode *node;

	while (c) {
		CamelMessageInfo *nfo = camel_folder_thread_node_get_item (c);
		/* phantom nodes no longer allowed */
		if (!nfo) {
			g_warning ("c->message shouldn't be NULL\n");
			c = camel_folder_thread_node_get_next (c);
			continue;
		}

		node = ml_uid_nodemap_insert (message_list, nfo, parent, -1);

		if (thread_latest && thread_flat && parent && node && parent->data && node->data) {
			CamelMessageInfo *parent_nfo, *node_nfo;

			parent_nfo = parent->data;
			node_nfo = node->data;

			if (camel_message_info_get_date_received (parent_nfo) < camel_message_info_get_date_received (node_nfo)) {
				/* Swap the root node's message info with the added one, because the added is the latest */
				parent->data = node_nfo;
				node->data = parent_nfo;
			}
		}

		if (camel_folder_thread_node_get_child (c)) {
			build_subtree (message_list, (camel_folder_thread_node_get_parent (c) && thread_flat) ? parent : node,
				camel_folder_thread_node_get_child (c), thread_flat, thread_latest, row);
		}
		c = camel_folder_thread_node_get_next (c);
	}
}

static void
build_flat (MessageList *message_list,
            GPtrArray *summary,
            gboolean folder_changed,
	    GHashTable *removed_uids)
{
	gchar *saveuid = NULL;
	gint i;
	GPtrArray *selected;
#ifdef TIMEIT
	struct timeval start, end;
	gulong diff;

	printf ("Building flat\n");
	gettimeofday (&start, NULL);
#endif

	if (message_list->cursor_uid != NULL)
		saveuid = find_next_selectable (message_list, removed_uids);

	selected = message_list_get_selected (message_list);

	message_list_tree_model_freeze (message_list);

	clear_tree (message_list, FALSE);

	for (i = 0; summary && i < summary->len; i++) {
		CamelMessageInfo *info = summary->pdata[i];

		ml_uid_nodemap_insert (message_list, info, NULL, -1);
	}

	message_list_tree_model_thaw (message_list);

	message_list_set_selected (message_list, selected);

	g_ptr_array_unref (selected);

	if (saveuid) {
		GNode *node;

		node = g_hash_table_lookup (
			message_list->uid_nodemap, saveuid);
		if (node == NULL) {
			g_free (message_list->cursor_uid);
			message_list->cursor_uid = NULL;
			g_signal_emit (
				message_list,
				signals[MESSAGE_SELECTED], 0, NULL);
		} else if (!folder_changed || !e_tree_get_item (E_TREE (message_list))) {
			e_tree_set_cursor (E_TREE (message_list), node);
		}
		g_free (saveuid);
	}

#ifdef TIMEIT
	gettimeofday (&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec / 1000;
	diff -= start.tv_sec * 1000 + start.tv_usec / 1000;
	printf ("Building flat took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

static void
message_list_change_first_visible_parent (MessageList *message_list,
                                          GNode *node)
{
	ETreeModel *tree_model;
	ETreeTableAdapter *adapter;
	GNode *first_visible = NULL;

	tree_model = E_TREE_MODEL (message_list);
	adapter = e_tree_get_table_adapter (E_TREE (message_list));

	while (node != NULL && (node = node->parent) != NULL) {
		if (!e_tree_table_adapter_node_is_expanded (adapter, node))
			first_visible = node;
	}

	if (first_visible != NULL) {
		e_tree_model_pre_change (tree_model);
		e_tree_model_node_data_changed (tree_model, first_visible);
	}
}

static CamelFolderChangeInfo *
mail_folder_hide_by_flag (CamelFolder *folder,
                          MessageList *message_list,
                          CamelFolderChangeInfo *changes,
                          gint flag)
{
	CamelFolderChangeInfo *newchanges;
	CamelMessageInfo *info;
	gint i;

	newchanges = camel_folder_change_info_new ();

	for (i = 0; i < changes->uid_changed->len; i++) {
		GNode *node;
		guint32 flags;

		node = g_hash_table_lookup (
			message_list->uid_nodemap,
			changes->uid_changed->pdata[i]);
		info = camel_folder_get_message_info (
			folder, changes->uid_changed->pdata[i]);
		if (info)
			flags = camel_message_info_get_flags (info);

		if (node != NULL && info != NULL && (flags & flag) != 0)
			camel_folder_change_info_remove_uid (
				newchanges, changes->uid_changed->pdata[i]);
		else if (node == NULL && info != NULL && (flags & flag) == 0)
			camel_folder_change_info_add_uid (
				newchanges, changes->uid_changed->pdata[i]);
		else
			camel_folder_change_info_change_uid (
				newchanges, changes->uid_changed->pdata[i]);

		g_clear_object (&info);
	}

	if (newchanges->uid_added->len > 0 || newchanges->uid_removed->len > 0) {
		for (i = 0; i < changes->uid_added->len; i++)
			camel_folder_change_info_add_uid (
				newchanges, changes->uid_added->pdata[i]);
		for (i = 0; i < changes->uid_removed->len; i++)
			camel_folder_change_info_remove_uid (
				newchanges, changes->uid_removed->pdata[i]);
	} else {
		camel_folder_change_info_clear (newchanges);
		camel_folder_change_info_cat (newchanges, changes);
	}

	return newchanges;
}

static void
message_list_folder_changed (CamelFolder *folder,
			     CamelFolderChangeInfo *changes,
			     MessageList *message_list)
{
	CamelFolderChangeInfo *altered_changes = NULL;
	gboolean has_regen_task;
	gboolean need_list_regen = TRUE;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (changes != NULL);
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (message_list->priv->destroyed)
		return;

	g_mutex_lock (&message_list->priv->regen_lock);
	has_regen_task = message_list->priv->regen_task != NULL;
	g_mutex_unlock (&message_list->priv->regen_lock);

	d (
		printf ("%s: has_regen_task:%s changes:%p added:%d removed:%d changed:%d recent:%d for '%s'\n",
		G_STRFUNC, has_regen_task ? "yes": "no", changes,
		changes ? changes->uid_added->len : -1,
		changes ? changes->uid_removed->len : -1,
		changes ? changes->uid_changed->len : -1,
		changes ? changes->uid_recent->len : -1,
		camel_folder_get_full_name (folder)));

	/* Skip the quick update when the message list is being regenerated */
	if (changes && !has_regen_task) {
		ETreeModel *tree_model;
		gboolean hide_junk;
		gboolean hide_deleted;
		gint i;

		tree_model = E_TREE_MODEL (message_list);

		hide_junk = message_list_get_hide_junk (message_list, folder);
		hide_deleted = message_list_get_hide_deleted (message_list, folder);

		for (i = 0; i < changes->uid_removed->len; i++)
			g_hash_table_remove (
				message_list->normalised_hash,
				changes->uid_removed->pdata[i]);

		/* Check if the hidden state has changed.
		 * If so, modify accordingly and regenerate. */
		if (hide_junk || hide_deleted)
			altered_changes = mail_folder_hide_by_flag (
				folder, message_list, changes,
				(hide_junk ? CAMEL_MESSAGE_JUNK : 0) |
				(hide_deleted ? CAMEL_MESSAGE_DELETED : 0));
		else {
			altered_changes = camel_folder_change_info_new ();
			camel_folder_change_info_cat (altered_changes, changes);
		}

		if (altered_changes->uid_added->len == 0 && altered_changes->uid_removed->len == 0 && altered_changes->uid_changed->len < 100) {
			for (i = 0; i < altered_changes->uid_changed->len; i++) {
				GNode *node;

				node = g_hash_table_lookup (
					message_list->uid_nodemap,
					altered_changes->uid_changed->pdata[i]);
				if (node) {
					e_tree_model_pre_change (tree_model);
					e_tree_model_node_data_changed (tree_model, node);

					message_list_change_first_visible_parent (message_list, node);
				}
			}

			g_signal_emit (
				message_list,
				signals[MESSAGE_LIST_BUILT], 0);

			need_list_regen = FALSE;
		}
	}

	if (need_list_regen) {
		/* Use 'changes' only if this is not the first change after the folder
		   had been set. There could happen a race condition on folder enter which prevented
		   the message list to scroll to the cursor position due to the folder_changed = TRUE,
		   by cancelling the full rebuild request. */
		mail_regen_list (message_list, NULL, message_list->just_set_folder ? NULL : changes);
	}

	if (altered_changes != NULL)
		camel_folder_change_info_free (altered_changes);
}

typedef struct _FolderChangedData {
	GWeakRef *folder; /* CamelFolder * */
	CamelFolderChangeInfo *changes;
	GWeakRef *message_list; /* MessageList * */
} FolderChangedData;

static void
folder_changed_data_free (gpointer ptr)
{
	FolderChangedData *fcd = ptr;

	if (fcd) {
		e_weak_ref_free (fcd->folder);
		e_weak_ref_free (fcd->message_list);
		camel_folder_change_info_free (fcd->changes);
		g_slice_free (FolderChangedData, fcd);
	}
}

static gboolean
message_list_folder_changed_timeout_cb (gpointer user_data)
{
	FolderChangedData *fcd = user_data;
	CamelFolder *folder;
	MessageList *message_list;

	g_return_val_if_fail (fcd != NULL, FALSE);

	folder = g_weak_ref_get (fcd->folder);
	message_list = g_weak_ref_get (fcd->message_list);

	if (folder && message_list)
		message_list_folder_changed (folder, fcd->changes, message_list);

	g_clear_object (&message_list);
	g_clear_object (&folder);

	return FALSE;
}

static void
message_list_folder_changed_cb (CamelFolder *folder,
				CamelFolderChangeInfo *changes,
				MessageList *message_list)
{
	if (message_list->priv->destroyed)
		return;

	if (e_util_is_main_thread (g_thread_self ())) {
		message_list_folder_changed (folder, changes, message_list);
	} else {
		FolderChangedData *fcd;

		fcd = g_slice_new0 (FolderChangedData);
		fcd->folder = e_weak_ref_new (folder);
		fcd->changes = camel_folder_change_info_copy (changes);
		fcd->message_list = e_weak_ref_new (message_list);

		/* Just to have it called in the main/UI thread */
		g_timeout_add_full (G_PRIORITY_DEFAULT, 1,
			message_list_folder_changed_timeout_cb,
			fcd, folder_changed_data_free);
	}
}

CamelFolder *
message_list_ref_folder (MessageList *message_list)
{
	CamelFolder *folder = NULL;

	/* XXX Do we need a property lock to guard this? */

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), NULL);

	if (message_list->priv->folder != NULL)
		folder = g_object_ref (message_list->priv->folder);

	return folder;
}

/**
 * message_list_set_folder:
 * @message_list: Message List widget
 * @folder: folder backend to be set
 *
 * Sets @folder to be the backend folder for @message_list.
 **/
void
message_list_set_folder (MessageList *message_list,
                         CamelFolder *folder)
{
	/* XXX Do we need a property lock to guard this? */

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (folder == message_list->priv->folder)
		return;

	if (folder != NULL) {
		g_return_if_fail (CAMEL_IS_FOLDER (folder));
		g_object_ref (folder);
	}

	mail_regen_cancel (message_list);

	g_free (message_list->search);
	message_list->search = NULL;

	g_free (message_list->frozen_search);
	message_list->frozen_search = NULL;

	if (message_list->seen_id) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}

	/* reset the normalised sort performance hack */
	g_hash_table_remove_all (message_list->normalised_hash);

	if (message_list->priv->folder != NULL)
		save_tree_state (message_list, message_list->priv->folder);

	message_list_tree_model_freeze (message_list);
	clear_tree (message_list, TRUE);
	message_list_tree_model_thaw (message_list);

	/* remove the cursor activate idle handler */
	if (message_list->idle_id != 0) {
		g_source_remove (message_list->idle_id);
		message_list->idle_id = 0;
	}

	if (message_list->priv->folder != NULL) {
		g_signal_handler_disconnect (
			message_list->priv->folder,
			message_list->priv->folder_changed_handler_id);
		message_list->priv->folder_changed_handler_id = 0;

		if (message_list->uid_nodemap != NULL)
			g_hash_table_foreach (
				message_list->uid_nodemap,
				(GHFunc) clear_info, message_list);

		g_clear_object (&message_list->priv->folder);
	}

	g_free (message_list->cursor_uid);
	message_list->cursor_uid = NULL;

	/* Always emit message-selected, event when an account node
	 * (folder == NULL) is selected, so that views know what happened and
	 * can stop all running operations etc. */
	g_signal_emit (message_list, signals[MESSAGE_SELECTED], 0, NULL);

	if (folder != NULL) {
		gboolean non_trash_folder;
		gboolean non_junk_folder;
		gint strikeout_col, strikeout_color_col;
		ECell *cell;
		gulong handler_id;

		message_list->priv->folder = folder;
		message_list->just_set_folder = TRUE;

		non_trash_folder = !(camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_TRASH);
		non_junk_folder = !(camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_JUNK);

		strikeout_col = -1;
		strikeout_color_col = -1;

		/* Setup the strikeout effect for non-trash or non-junk folders */
		if (non_trash_folder && non_junk_folder) {
			strikeout_col = COL_DELETED_OR_JUNK;
			strikeout_color_col = COL_JUNK_STRIKEOUT_COLOR;
		} else if (non_trash_folder) {
			strikeout_col = COL_DELETED;
		} else if (non_junk_folder) {
			strikeout_col = COL_JUNK;
			strikeout_color_col = COL_JUNK_STRIKEOUT_COLOR;
		}

		cell = e_table_extras_get_cell (message_list->extras, "render_date");
		g_object_set (cell, "strikeout-column", strikeout_col, "strikeout-color-column", strikeout_color_col, NULL);

		cell = e_table_extras_get_cell (message_list->extras, "render_text");
		g_object_set (cell, "strikeout-column", strikeout_col, "strikeout-color-column", strikeout_color_col, NULL);

		cell = e_table_extras_get_cell (message_list->extras, "render_size");
		g_object_set (cell, "strikeout-column", strikeout_col, "strikeout-color-column", strikeout_color_col, NULL);

		cell = e_table_extras_get_cell (message_list->extras, "render_composite_from");
		composite_cell_set_strike_col (cell, strikeout_col, strikeout_color_col);

		cell = e_table_extras_get_cell (message_list->extras, "render_composite_to");
		composite_cell_set_strike_col (cell, strikeout_col, strikeout_color_col);

		/* Build the etree suitable for this folder */
		message_list_setup_etree (message_list);

		handler_id = g_signal_connect (
			folder, "changed",
			G_CALLBACK (message_list_folder_changed_cb),
			message_list);
		message_list->priv->folder_changed_handler_id = handler_id;

		if (message_list->frozen == 0)
			mail_regen_list (message_list, NULL, NULL);
		else
			message_list->priv->thaw_needs_regen = TRUE;
	}
}

GtkTargetList *
message_list_get_copy_target_list (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), NULL);

	return message_list->priv->copy_target_list;
}

GtkTargetList *
message_list_get_paste_target_list (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), NULL);

	return message_list->priv->paste_target_list;
}

void
message_list_set_expanded_default (MessageList *message_list,
                                   gboolean expanded_default)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	message_list->priv->expanded_default = expanded_default;
}

gboolean
message_list_get_group_by_threads (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->priv->group_by_threads;
}

void
message_list_set_group_by_threads (MessageList *message_list,
                                   gboolean group_by_threads)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (group_by_threads == message_list->priv->group_by_threads)
		return;

	message_list->priv->group_by_threads = group_by_threads;
	e_tree_set_grouped_view (E_TREE (message_list), group_by_threads);

	g_object_notify (G_OBJECT (message_list), "group-by-threads");

	/* Changing this property triggers a message list regen. */
	if (message_list->frozen == 0)
		mail_regen_list (message_list, NULL, NULL);
	else
		message_list->priv->thaw_needs_regen = TRUE;
}

gboolean
message_list_get_show_deleted (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->priv->show_deleted;
}

void
message_list_set_show_deleted (MessageList *message_list,
                               gboolean show_deleted)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (show_deleted == message_list->priv->show_deleted)
		return;

	message_list->priv->show_deleted = show_deleted;

	g_object_notify (G_OBJECT (message_list), "show-deleted");

	/* Changing this property triggers a message list regen. */
	if (message_list->frozen == 0)
		mail_regen_list (message_list, NULL, NULL);
	else
		message_list->priv->thaw_needs_regen = TRUE;
}

gboolean
message_list_get_show_junk (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->priv->show_junk;
}

void
message_list_set_show_junk (MessageList *message_list,
			    gboolean show_junk)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (show_junk == message_list->priv->show_junk)
		return;

	message_list->priv->show_junk = show_junk;

	g_object_notify (G_OBJECT (message_list), "show-junk");

	/* Changing this property triggers a message list regen. */
	if (message_list->frozen == 0)
		mail_regen_list (message_list, NULL, NULL);
	else
		message_list->priv->thaw_needs_regen = TRUE;
}

gboolean
message_list_get_thread_latest (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->priv->thread_latest;
}

void
message_list_set_thread_latest (MessageList *message_list,
                                gboolean thread_latest)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (thread_latest == message_list->priv->thread_latest)
		return;

	message_list->priv->thread_latest = thread_latest;

	g_object_notify (G_OBJECT (message_list), "thread-latest");
}

gboolean
message_list_get_thread_subject (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->priv->thread_subject;
}

void
message_list_set_thread_subject (MessageList *message_list,
                                 gboolean thread_subject)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (thread_subject == message_list->priv->thread_subject)
		return;

	message_list->priv->thread_subject = thread_subject;

	g_object_notify (G_OBJECT (message_list), "thread-subject");
}

gboolean
message_list_get_thread_compress (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->priv->thread_compress;
}

void
message_list_set_thread_compress (MessageList *message_list,
				  gboolean thread_compress)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if ((thread_compress ? 1 : 0) == (message_list->priv->thread_compress ? 1 : 0))
		return;

	message_list->priv->thread_compress = thread_compress;

	g_object_notify (G_OBJECT (message_list), "thread-compress");

	gtk_widget_queue_draw (GTK_WIDGET (message_list));
}

gboolean
message_list_get_thread_flat (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->priv->thread_flat;
}

void
message_list_set_thread_flat (MessageList *message_list,
			      gboolean thread_flat)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if ((thread_flat ? 1 : 0) == (message_list->priv->thread_flat ? 1 : 0))
		return;

	message_list->priv->thread_flat = thread_flat;

	g_object_notify (G_OBJECT (message_list), "thread-flat");

	if (message_list->priv->group_by_threads) {
		if (!message_list->frozen)
			mail_regen_list (message_list, NULL, NULL);
		else
			message_list->priv->thaw_needs_regen = TRUE;
	}
}

gboolean
message_list_get_regen_selects_unread (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return message_list->priv->regen_selects_unread;
}

void
message_list_set_regen_selects_unread (MessageList *message_list,
				       gboolean regen_selects_unread)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if ((regen_selects_unread ? 1 : 0) == (message_list->priv->regen_selects_unread ? 1 : 0))
		return;

	message_list->priv->regen_selects_unread = regen_selects_unread;
}

static gboolean
on_cursor_activated_idle (gpointer data)
{
	MessageList *message_list = data;
	ESelectionModel *esm;
	gint selected;

	esm = e_tree_get_selection_model (E_TREE (message_list));
	selected = e_selection_model_selected_count (esm);

	if (selected == 1 && message_list->cursor_uid) {
		d (printf ("emitting cursor changed signal, for uid %s\n", message_list->cursor_uid));
		g_signal_emit (
			message_list,
			signals[MESSAGE_SELECTED], 0,
			message_list->cursor_uid);
	} else {
		g_signal_emit (
			message_list,
			signals[MESSAGE_SELECTED], 0,
			NULL);
	}

	message_list->idle_id = 0;
	return FALSE;
}

static void
on_cursor_activated_cmd (ETree *tree,
                         gint row,
                         GNode *node,
                         gpointer user_data)
{
	MessageList *message_list = MESSAGE_LIST (user_data);
	const gchar *new_uid;

	if (node == NULL || G_NODE_IS_ROOT (node))
		new_uid = NULL;
	else
		new_uid = get_message_uid (message_list, node);

	/* Do not check the cursor_uid and the new_uid values, because the
	* selected item (set in on_selection_changed_cmd) can be different
	* from the one with a cursor (when selecting with Ctrl, for example).
	* This has a little side-effect, when keeping list it that state,
	* then changing folders forth and back will select and move cursor
	* to that selected item. Does anybody consider it as a bug? */
	if ((message_list->cursor_uid == NULL && new_uid == NULL)
	    || (message_list->last_sel_single && message_list->cursor_uid != NULL && new_uid != NULL))
		return;

	g_free (message_list->cursor_uid);
	message_list->cursor_uid = g_strdup (new_uid);

	if (!message_list->idle_id) {
		message_list->idle_id =
			g_idle_add_full (
				G_PRIORITY_LOW, on_cursor_activated_idle,
				message_list, NULL);
	}
}

static void
on_selection_changed_cmd (ETree *tree,
                          MessageList *message_list)
{
	GPtrArray *uids = NULL;
	const gchar *newuid;
	guint selected_count;
	GNode *cursor;

	selected_count = message_list_selected_count (message_list);
	if (selected_count == 1) {
		uids = message_list_get_selected (message_list);

		if (uids->len == 1)
			newuid = g_ptr_array_index (uids, 0);
		else
			newuid = NULL;
	} else if ((cursor = e_tree_get_cursor (tree)))
		newuid = (gchar *) camel_message_info_get_uid (cursor->data);
	else
		newuid = NULL;

	/* If the selection isn't empty, then we ignore the no-uid check, since this event
	 * is also used for other updating.  If it is empty, it might just be a setup event
	 * from etree which we do need to ignore */
	if ((newuid == NULL && message_list->cursor_uid == NULL && selected_count == 0) ||
	    (message_list->last_sel_single && selected_count == 1 && message_list->cursor_uid != NULL && (newuid == NULL || !strcmp (message_list->cursor_uid, newuid)))) {
		/* noop */
	} else {
		g_free (message_list->cursor_uid);
		message_list->cursor_uid = g_strdup (newuid);
		if (message_list->idle_id == 0)
			message_list->idle_id = g_idle_add_full (
				G_PRIORITY_LOW,
				on_cursor_activated_idle,
				message_list, NULL);
	}

	message_list->last_sel_single = selected_count == 1;

	if (uids)
		g_ptr_array_unref (uids);
}

static gint
on_click (ETree *tree,
          gint row,
          GNode *node,
          gint col,
          GdkEvent *event,
          MessageList *list)
{
	CamelFolder *folder;
	CamelMessageInfo *info;
	gboolean folder_is_trash;
	gint flag = 0;
	guint32 flags;

	if (col == COL_MESSAGE_STATUS)
		flag = CAMEL_MESSAGE_SEEN;
	else if (col == COL_FLAGGED)
		flag = CAMEL_MESSAGE_FLAGGED;
	else if (col != COL_FOLLOWUP_FLAG_STATUS)
		return FALSE;

	if (!(info = get_message_info (list, node)))
		return FALSE;

	folder = message_list_ref_folder (list);
	g_return_val_if_fail (folder != NULL, FALSE);

	if (col == COL_FOLLOWUP_FLAG_STATUS) {
		const gchar *tag, *cmp;

		tag = camel_message_info_get_user_tag (info, "follow-up");
		cmp = camel_message_info_get_user_tag (info, "completed-on");
		if (tag && tag[0]) {
			if (cmp && cmp[0]) {
				camel_message_info_set_user_tag (info, "follow-up", NULL);
				camel_message_info_set_user_tag (info, "due-by", NULL);
				camel_message_info_set_user_tag (info, "completed-on", NULL);
			} else {
				gchar *text;

				text = camel_header_format_date (time (NULL), 0);
				camel_message_info_set_user_tag (info, "completed-on", text);
				g_free (text);
			}
		} else {
			/* default follow-up flag name to use when clicked in the message list column */
			camel_message_info_set_user_tag (info, "follow-up", _("Follow-up"));
			camel_message_info_set_user_tag (info, "completed-on", NULL);
		}

		g_object_unref (folder);

		return TRUE;
	}

	flags = camel_message_info_get_flags (info);

	folder_is_trash =
		((camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_TRASH) != 0);

	/* If a message was marked as deleted and the user flags it as
	 * important or unread in a non-Trash folder, then undelete the
	 * message.  We avoid automatically undeleting messages while
	 * viewing a Trash folder because it would cause the message to
	 * suddenly disappear from the message list, which is confusing
	 * and alarming to the user. */
	if (!folder_is_trash && flags & CAMEL_MESSAGE_DELETED) {
		if (col == COL_FLAGGED && !(flags & CAMEL_MESSAGE_FLAGGED))
			flag |= CAMEL_MESSAGE_DELETED;

		if (col == COL_MESSAGE_STATUS && (flags & CAMEL_MESSAGE_SEEN))
			flag |= CAMEL_MESSAGE_DELETED;
	}

	camel_message_info_set_flags (info, flag, ~flags);

	/* Notify the folder tree model that the user has marked a message
	 * as unread so it doesn't mistake the event as new mail arriving. */
	if (col == COL_MESSAGE_STATUS && (flags & CAMEL_MESSAGE_SEEN)) {
		EMFolderTreeModel *model;

		model = em_folder_tree_model_get_default ();
		em_folder_tree_model_user_marked_unread (model, folder, 1);
	}

	if (flag == CAMEL_MESSAGE_SEEN && list->seen_id &&
	    g_strcmp0 (list->cursor_uid, camel_message_info_get_uid (info)) == 0) {
		g_source_remove (list->seen_id);
		list->seen_id = 0;
	}

	g_object_unref (folder);

	return TRUE;
}

struct _ml_selected_data {
	MessageList *message_list;
	ETreeTableAdapter *adapter;
	gboolean with_collapsed_threads;
	GPtrArray *uids;
};

static gboolean
ml_getselected_collapsed_cb (ETreeModel *tree_model,
			     ETreePath path,
			     gpointer user_data)
{
	struct _ml_selected_data *data = user_data;
	const gchar *uid;
	GNode *node = (GNode *) path;

	uid = get_message_uid (data->message_list, node);
	g_return_val_if_fail (uid != NULL, FALSE);
	g_ptr_array_add (data->uids, g_strdup (uid));

	return FALSE;
}

static void
ml_getselected_cb (GNode *node,
                   gpointer user_data)
{
	struct _ml_selected_data *data = user_data;
	const gchar *uid;

	if (G_NODE_IS_ROOT (node))
		return;

	uid = get_message_uid (data->message_list, node);
	g_return_if_fail (uid != NULL);
	g_ptr_array_add (data->uids, g_strdup (uid));

	if (data->with_collapsed_threads && g_node_first_child (node) &&
	    !e_tree_table_adapter_node_is_expanded (data->adapter, node)) {
		e_tree_model_node_traverse (E_TREE_MODEL (data->message_list), node, ml_getselected_collapsed_cb, data);
	}
}

static GPtrArray *
message_list_get_selected_full (MessageList *message_list,
				gboolean with_collapsed_threads)
{
	CamelFolder *folder;
	ESelectionModel *selection;

	struct _ml_selected_data data = {
		message_list,
		NULL
	};

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), NULL);

	data.adapter = e_tree_get_table_adapter (E_TREE (message_list));
	data.with_collapsed_threads = with_collapsed_threads;
	data.uids = g_ptr_array_new ();
	g_ptr_array_set_free_func (data.uids, (GDestroyNotify) g_free);

	selection = e_tree_get_selection_model (E_TREE (message_list));

	e_tree_selection_model_foreach (
		E_TREE_SELECTION_MODEL (selection),
		(ETreeForeachFunc) ml_getselected_cb, &data);

	folder = message_list_ref_folder (message_list);

	if (folder != NULL && data.uids->len > 0)
		camel_folder_sort_uids (folder, data.uids);

	g_clear_object (&folder);

	return data.uids;
}

GPtrArray *
message_list_get_selected (MessageList *message_list)
{
	return message_list_get_selected_full (message_list, FALSE);
}

GPtrArray *
message_list_get_selected_with_collapsed_threads (MessageList *message_list)
{
	return message_list_get_selected_full (message_list, TRUE);
}

void
message_list_set_selected (MessageList *message_list,
                           GPtrArray *uids)
{
	gint i;
	ETreeSelectionModel *etsm;
	GNode *node;
	GPtrArray *paths;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	paths = g_ptr_array_new ();
	etsm = (ETreeSelectionModel *)
		e_tree_get_selection_model (E_TREE (message_list));
	for (i = 0; i < uids->len; i++) {
		node = g_hash_table_lookup (
			message_list->uid_nodemap, uids->pdata[i]);
		if (node != NULL)
			g_ptr_array_add (paths, node);
	}

	e_tree_selection_model_select_paths (etsm, paths);
	g_ptr_array_free (paths, TRUE);
}

struct ml_sort_uids_data {
	gchar *uid;
	gint row;
};

static gint
ml_sort_uids_cb (gconstpointer a,
                 gconstpointer b)
{
	struct ml_sort_uids_data * const *pdataA = a;
	struct ml_sort_uids_data * const *pdataB = b;

	return (* pdataA)->row - (* pdataB)->row;
}

void
message_list_sort_uids (MessageList *message_list,
                        GPtrArray *uids)
{
	struct ml_sort_uids_data *data;
	GPtrArray *array;
	GNode *node;
	ETreeTableAdapter *adapter;
	gint ii;

	g_return_if_fail (message_list != NULL);
	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	g_return_if_fail (uids != NULL);

	if (uids->len <= 1)
		return;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));

	array = g_ptr_array_new_full (uids->len, g_free);

	for (ii = 0; ii < uids->len; ii++) {
		data = g_new0 (struct ml_sort_uids_data, 1);
		data->uid = g_ptr_array_index (uids, ii);

		node = g_hash_table_lookup (message_list->uid_nodemap, data->uid);
		if (node != NULL)
			data->row = e_tree_table_adapter_row_of_node (adapter, node);
		else
			data->row = ii;

		g_ptr_array_add (array, data);
	}

	g_ptr_array_sort (array, ml_sort_uids_cb);

	for (ii = 0; ii < uids->len; ii++) {
		data = g_ptr_array_index (array, ii);

		uids->pdata[ii] = data->uid;
	}

	g_ptr_array_free (array, TRUE);
}

struct ml_count_data {
	MessageList *message_list;
	guint count;
};

static void
ml_getcount_cb (GNode *node,
                gpointer user_data)
{
	struct ml_count_data *data = user_data;

	if (!G_NODE_IS_ROOT (node))
		data->count++;
}

guint
message_list_count (MessageList *message_list)
{
	struct ml_count_data data = { message_list, 0 };

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), 0);

	e_tree_path_foreach (
		E_TREE (message_list),
		(ETreeForeachFunc) ml_getcount_cb, &data);

	return data.count;
}

guint
message_list_selected_count (MessageList *message_list)
{
	ESelectionModel *selection;

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), 0);

	selection = e_tree_get_selection_model (E_TREE (message_list));
	return  e_selection_model_selected_count (selection);
}

void
message_list_freeze (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	message_list->frozen++;
}

void
message_list_thaw (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	g_return_if_fail (message_list->frozen != 0);

	message_list->frozen--;
	if (message_list->frozen == 0 && message_list->priv->thaw_needs_regen) {
		const gchar *search;

		if (message_list->frozen_search != NULL)
			search = message_list->frozen_search;
		else
			search = NULL;

		mail_regen_list (message_list, search, NULL);

		g_free (message_list->frozen_search);
		message_list->frozen_search = NULL;
		message_list->priv->thaw_needs_regen = FALSE;
	}
}

/* set whether we are in threaded view or flat view */
void
message_list_set_threaded_expand_all (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (message_list_get_group_by_threads (message_list)) {
		message_list->expand_all = 1;

		if (message_list->frozen == 0)
			mail_regen_list (message_list, NULL, NULL);
		else
			message_list->priv->thaw_needs_regen = TRUE;
	}
}

void
message_list_set_threaded_collapse_all (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (message_list_get_group_by_threads (message_list)) {
		message_list->collapse_all = 1;

		if (message_list->frozen == 0)
			mail_regen_list (message_list, NULL, NULL);
		else
			message_list->priv->thaw_needs_regen = TRUE;
	}
}

void
message_list_set_search (MessageList *message_list,
                         const gchar *search)
{
	gboolean has_regen_task;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	g_mutex_lock (&message_list->priv->regen_lock);
	has_regen_task = message_list->priv->regen_task != NULL;
	g_mutex_unlock (&message_list->priv->regen_lock);

	if (!has_regen_task && (search == NULL || search[0] == '\0'))
		if (message_list->search == NULL || message_list->search[0] == '\0')
			return;

	if (!has_regen_task && search != NULL && message_list->search != NULL &&
	    strcmp (search, message_list->search) == 0) {
		return;
	}

	if (message_list->frozen == 0)
		mail_regen_list (message_list, search ? search : "", NULL);
	else {
		g_free (message_list->frozen_search);
		message_list->frozen_search = g_strdup (search);
		message_list->priv->thaw_needs_regen = TRUE;
	}
}

static void
message_list_regen_tweak_search_results (MessageList *message_list,
                                         GPtrArray *search_results,
                                         CamelFolder *folder,
                                         gboolean folder_changed,
                                         gboolean show_deleted,
                                         gboolean show_junk)
{
	CamelMessageInfo *info;
	CamelMessageFlags flags;
	const gchar *uid;
	gboolean needs_tweaking;
	gboolean uid_is_deleted;
	gboolean uid_is_junk;
	gboolean add_uid;
	guint ii;

	/* If we're responding to a "folder-changed" signal, then the
	 * displayed message may not be included in the search results.
	 * Include the displayed message anyway so it doesn't suddenly
	 * disappear while the user is reading it. */
	needs_tweaking =
		((folder_changed || message_list->just_set_folder) && message_list->cursor_uid != NULL);

	if (!needs_tweaking)
		return;

	uid = message_list->cursor_uid;

	/* Scan the search results for a particular UID.
	 * If found then the results don't need tweaked. */
	for (ii = 0; ii < search_results->len; ii++) {
		if (g_str_equal (uid, search_results->pdata[ii]))
			return;
	}

	info = camel_folder_get_message_info (folder, uid);

	/* XXX Should we emit a runtime warning here? */
	if (info == NULL)
		return;

	flags = camel_message_info_get_flags (info);
	uid_is_deleted = ((flags & CAMEL_MESSAGE_DELETED) != 0);
	uid_is_junk = ((flags & CAMEL_MESSAGE_JUNK) != 0);

	if (!folder_store_supports_vjunk_folder (folder))
		uid_is_junk = FALSE;

	add_uid =
		(!uid_is_junk || show_junk) &&
		(!uid_is_deleted || show_deleted);

	if (add_uid)
		g_ptr_array_add (
			search_results,
			(gpointer) camel_pstring_strdup (uid));

	g_clear_object (&info);
}

static void
message_list_regen_thread (GTask *task,
                           gpointer source_object,
                           gpointer task_data,
                           GCancellable *cancellable)
{
	MessageList *message_list;
	RegenData *regen_data;
	GPtrArray *uids = NULL, *searchuids = NULL;
	CamelMessageInfo *info;
	CamelFolder *folder;
	GNode *cursor;
	ETree *tree;
	GString *expr;
	gboolean hide_deleted;
	gboolean hide_junk;
	GError *local_error = NULL;

	message_list = MESSAGE_LIST (source_object);
	regen_data = task_data;

	if (g_task_return_error_if_cancelled (task))
		return;

	/* Just for convenience. */
	folder = g_object_ref (regen_data->folder);

	hide_junk = message_list_get_hide_junk (message_list, folder);
	hide_deleted = message_list_get_hide_deleted (message_list, folder);

	tree = E_TREE (message_list);
	cursor = e_tree_get_cursor (tree);
	if (cursor != NULL)
		regen_data->last_row =
			e_tree_table_adapter_row_of_node (
			e_tree_get_table_adapter (tree), cursor);

	/* Construct the search expression. */

	expr = g_string_new ("");

	if (hide_deleted && hide_junk) {
		g_string_append_printf (
			expr, "(and %s %s)",
			EXCLUDE_DELETED_MESSAGES_EXPR,
			EXCLUDE_JUNK_MESSAGES_EXPR);
	} else if (hide_deleted) {
		g_string_append (expr, EXCLUDE_DELETED_MESSAGES_EXPR);
	} else if (hide_junk) {
		g_string_append (expr, EXCLUDE_JUNK_MESSAGES_EXPR);
	}

	/* The 'expr' should be enclosed in "(match-all ...)", thus the search traverses
	   folder content, but also try to not repeat it, to avoid unnecessary performance hits. */
	if (regen_data->search != NULL) {
		gboolean is_match_all = g_str_has_prefix (regen_data->search, "(match-all ") && !strstr (regen_data->search, "(body-contains ");
		gboolean is_match_threads = strstr (regen_data->search, "(match-threads ") != NULL;

		if (expr->len == 0) {
			g_string_assign (expr, regen_data->search);

			if (!is_match_all && !is_match_threads && expr->len) {
				g_string_prepend (expr, "(match-all ");
				g_string_append_c (expr, ')');
			}
		} else if (is_match_threads || !is_match_all) {
			/* The "match-threads" cannot be below "match-all". */
			g_string_prepend (expr, "(and (match-all ");
			g_string_append (expr, ") ");
			g_string_append (expr, regen_data->search);
			g_string_append_c (expr, ')');
		} else {
			const gchar *stripped_search = regen_data->search + 11; /* strlen ("(match-all ") */
			gint len = strlen (stripped_search);

			g_string_prepend (expr, "(match-all (and ");
			g_string_append_c (expr, ' ');

			if (len > 0 && stripped_search[len - 1] == ')') {
				g_string_append_len (expr, stripped_search, len - 1);
			} else {
				g_string_append (expr, regen_data->search);
			}

			g_string_append (expr, "))");
		}
	} else if (expr->len) {
		g_string_prepend (expr, "(match-all ");
		g_string_append_c (expr, ')');
	}

	/* Execute the search. */

	if (expr->len == 0) {
		uids = camel_folder_dup_uids (folder);
		dd (g_print ("%s: got %d uids in folder %p (%s : %s)\n", G_STRFUNC, uids ? uids->len : -1, folder,
			camel_service_get_display_name (CAMEL_SERVICE (camel_folder_get_parent_store (folder))),
			camel_folder_get_full_name (folder)));
	} else {
		if (!camel_folder_search_sync (folder, expr->str, &uids, cancellable, &local_error))
			uids = NULL;

		dd (g_print ("%s: got %d uids in folder %p (%s : %s) for expression:---%s---\n", G_STRFUNC,
			uids ? uids->len : -1, folder,
			camel_service_get_display_name (CAMEL_SERVICE (camel_folder_get_parent_store (folder))),
			camel_folder_get_full_name (folder), expr->str));

		/* XXX This indicates we need to use a different
		 *     "free UID" function for some dumb reason. */
		searchuids = uids;

		if (uids != NULL) {
			message_list_regen_tweak_search_results (
				message_list,
				uids, folder,
				regen_data->folder_changed,
				!hide_deleted,
				!hide_junk);

			dd (g_print ("   %s: got %d uids in folder %p (%s : %s) after tweak, hide_deleted:%d, hide_junk:%d\n", G_STRFUNC,
				uids ? uids->len : -1, folder,
				camel_service_get_display_name (CAMEL_SERVICE (camel_folder_get_parent_store (folder))),
				camel_folder_get_full_name (folder), hide_deleted, hide_junk));
		}
	}

	g_string_free (expr, TRUE);

	/* Handle search error or cancellation. */

	if (g_task_return_error_if_cancelled (task))
		goto exit;

	if (local_error != NULL) {
		g_task_return_error (task, g_steal_pointer (&local_error));
		goto exit;
	}

	/* XXX This check might not be necessary.  A successfully completed
	 *     search with no results should return an empty UID array, but
	 *     still need to verify that. */
	if (uids == NULL)
		goto exit_successfully;

	camel_folder_sort_uids (folder, uids);

	/* update/build a new tree */
	if (regen_data->group_by_threads) {
		CamelFolderThread *thread_tree;

		/* Always build a new thread_tree, to avoid race condition
		   when accessing it here and in the build_tree() call
		   from multiple threads. */
		thread_tree = camel_folder_thread_new (folder, uids, regen_data->thread_subject ? CAMEL_FOLDER_THREAD_FLAG_SUBJECT : CAMEL_FOLDER_THREAD_FLAG_NONE);

		/* We will build the ETreeModel content from this
		 * CamelFolderThread during regen post-processing.
		 *
		 * We're committed at this point so keep our own
		 * reference in case the MessageList's reference
		 * gets invalidated before regen post-processing. */
		regen_data->thread_tree = thread_tree;

	} else {
		guint ii;

		regen_data->summary = g_ptr_array_sized_new (uids->len);

		camel_folder_summary_prepare_fetch_all (camel_folder_get_folder_summary (folder), NULL);

		for (ii = 0; ii < uids->len; ii++) {
			const gchar *uid;

			uid = g_ptr_array_index (uids, ii);
			info = camel_folder_get_message_info (folder, uid);
			if (info != NULL)
				g_ptr_array_add (regen_data->summary, info);
		}
	}

exit_successfully:
	g_task_return_boolean (task, TRUE);
exit:
	if (searchuids != NULL)
		g_ptr_array_unref (searchuids);
	else if (uids != NULL)
		g_ptr_array_unref (uids);

	g_object_unref (folder);
	g_clear_error (&local_error);
}

static gint
message_list_correct_row_for_remove (MessageList *message_list,
				     gint row,
				     GHashTable *removed_uids)
{
	ETreeTableAdapter *adapter;
	gint orig_row = row, row_count;
	gboolean delete_selects_previous;
	GSettings *settings;
	gboolean done = FALSE;
	gint round;

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), row);

	if (!removed_uids)
		return row;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	delete_selects_previous = g_settings_get_boolean (settings, "delete-selects-previous");
	g_clear_object (&settings);

	adapter = e_tree_get_table_adapter (E_TREE (message_list));
	row_count = e_table_model_row_count (E_TABLE_MODEL (adapter));

	for (round = 0; round < 2 && !done && row_count; round++) {
		row = orig_row;

		/* The first round tries to find the next/previous not-deleted message in the list;
		   the second round does the same in the opposite direction. */
		if (round)
			delete_selects_previous = !delete_selects_previous;

		while (!done && row >= 0 && row < row_count) {
			GNode *node;

			node = e_tree_table_adapter_node_at_row (adapter, row);
			if (!node)
				break;

			done = !g_hash_table_contains (removed_uids, get_message_uid (message_list, node));

			if (!done) {
				if (delete_selects_previous)
					row--;
				else
					row++;
			}
		}
	}

	if (!done) {
		/* This is flipped due to the second round */
		if (delete_selects_previous)
			row = row_count - 1;
		else
			row = row_count ? 0 : -1;
	}

	return row;
}

static gint
message_list_correct_row_for_remove_in_selection (MessageList *message_list,
						  gint row,
						  GHashTable *removed_uids)
{
	ETreeTableAdapter *adapter;
	GNode *node;
	GPtrArray *selected;
	guint ii;
	gint best_row = row, best_dist = -1;

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), row);

	if (!removed_uids)
		return row;

	adapter = e_tree_get_table_adapter (E_TREE (message_list));
	node = e_tree_table_adapter_node_at_row (adapter, row);
	if (!node || !g_hash_table_contains (removed_uids, get_message_uid (message_list, node)))
		return row;

	selected = message_list_get_selected (message_list);
	if (!selected)
		return row;

	for (ii = 0; ii < selected->len; ii++) {
		gint sel_row, sel_dist;

		node = g_hash_table_lookup (message_list->uid_nodemap, g_ptr_array_index (selected, ii));
		if (!node || g_hash_table_contains (removed_uids, get_message_uid (message_list, node)))
			continue;

		sel_row = e_tree_table_adapter_row_of_node (adapter, node);
		sel_dist = ABS (sel_row - row);

		/* No good guess between selection, just find the nearest not deleted selected row */
		if (sel_dist < best_dist || best_dist == -1) {
			best_row = sel_row;
			best_dist = sel_dist;
		}
	}

	g_ptr_array_unref (selected);

	return best_row;
}

static void
message_list_update_tree_text (MessageList *message_list)
{
	ETreeTableAdapter *adapter;
	ETree *tree;
	const gchar *info_message;
	gboolean have_search_expr;
	gint row_count;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	g_return_if_fail (e_util_is_main_thread (g_thread_self ()));

	if (!gtk_widget_is_visible (GTK_WIDGET (message_list)))
		return;

	tree = E_TREE (message_list);
	adapter = e_tree_get_table_adapter (tree);
	row_count = e_table_model_row_count (E_TABLE_MODEL (adapter));

	/* space is used to indicate no search too */
	have_search_expr =
		(message_list->search != NULL) &&
		(*message_list->search != '\0') &&
		(strcmp (message_list->search, " ") != 0);

	if (row_count > 0) {
		info_message = NULL;
	} else if (message_list_is_setting_up_search_folder (message_list)) {
		info_message = _("Generating message list…");
	} else if (have_search_expr) {
		info_message =
			_("No message satisfies your search criteria. "
			"Change search criteria by selecting a new "
			"Show message filter from the drop down list "
			"above or by running a new search either by "
			"clearing it with Search→Clear menu item or "
			"by changing the query above.");
	} else {
		info_message = _("There are no messages in this folder.");
	}

	e_tree_set_info_message (tree, info_message);
}

static void
message_list_regen_done_cb (GObject *source_object,
                            GAsyncResult *result,
                            gpointer user_data)
{
	MessageList *message_list;
	RegenData *regen_data;
	EActivity *activity;
	ETree *tree;
	ETreeTableAdapter *adapter;
	gboolean was_searching, is_searching;
	gint row_count;
	const gchar *start_selection_uid = NULL, *last_row_uid = NULL; /* These are in Camel's string pool */
	GError *local_error = NULL;

	message_list = MESSAGE_LIST (source_object);
	regen_data = g_task_get_task_data (G_TASK (result));
	g_task_propagate_boolean (G_TASK (result), &local_error);

	/* Withdraw our RegenData from the private struct, if it hasn't
	 * already been replaced.  We have exclusive access to it now. */
	g_mutex_lock (&message_list->priv->regen_lock);
	if (message_list->priv->regen_task == G_TASK (result)) {
		g_clear_object (&message_list->priv->regen_task);
		e_tree_set_info_message (E_TREE (message_list), NULL);
	}
	g_mutex_unlock (&message_list->priv->regen_lock);

	activity = regen_data->activity;
	if (e_activity_handle_cancellation (activity, local_error)) {
		g_clear_error (&local_error);
		return;

	} else if (local_error != NULL) {
		EAlertSink *alert_sink = e_activity_get_alert_sink (activity);
		gboolean handled = FALSE;

		if (alert_sink) {
			e_alert_submit (alert_sink, "mail:message-list-regen-failed", local_error->message, NULL);
			handled = TRUE;
		}

		if (!handled)
			g_warning ("%s: %s", G_STRFUNC, local_error->message);

		g_clear_error (&local_error);
		return;
	}

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	tree = E_TREE (message_list);
	adapter = e_tree_get_table_adapter (tree);

	/* Show the cursor unless we're responding to a
	 * "folder-changed" signal from our CamelFolder. */
	if (!regen_data->folder_changed)
		e_tree_show_cursor_after_reflow (tree);

	g_signal_handlers_block_by_func (
		adapter, ml_tree_sorting_changed, message_list);

	was_searching = message_list_is_searching (message_list);

	g_free (message_list->search);
	message_list->search = g_strdup (regen_data->search);

	is_searching = message_list_is_searching (message_list);

	if (!message_list->just_set_folder) {
		gint row;

		row = e_tree_selection_model_get_selection_start_row (E_TREE_SELECTION_MODEL (e_tree_get_selection_model (tree)));

		if (row != -1)
			row = message_list_correct_row_for_remove_in_selection (message_list, row, regen_data->removed_uids);

		if (row != -1) {
			GNode *node;

			node = e_tree_table_adapter_node_at_row (adapter, row);
			if (node)
				start_selection_uid = camel_pstring_strdup (get_message_uid (message_list, node));
		}
	}

	if (!regen_data->select_all && !regen_data->select_uid && regen_data->last_row != -1) {
		regen_data->last_row = message_list_correct_row_for_remove (message_list, regen_data->last_row, regen_data->removed_uids);

		if (regen_data->last_row != -1) {
			GNode *node;

			node = e_tree_table_adapter_node_at_row (adapter, regen_data->last_row);
			if (node)
				last_row_uid = camel_pstring_strdup (get_message_uid (message_list, node));
		}
	}

	if (regen_data->group_by_threads) {
		ETableItem *table_item = e_tree_get_item (E_TREE (message_list));
		GPtrArray *selected;
		gchar *saveuid = NULL;
		gboolean forcing_expand_state;

		forcing_expand_state =
			message_list->expand_all ||
			message_list->collapse_all;

		if (message_list->just_set_folder) {
			message_list->just_set_folder = FALSE;
			/* Load state from disk rather than use
			 * the memory data when changing folders. */
			g_clear_pointer (&regen_data->expand_state, xmlFreeDoc);
		}

		if (forcing_expand_state) {
			gint state;

			if (message_list->expand_all)
				state = 1;  /* force expand */
			else
				state = -1; /* force collapse */

			e_tree_table_adapter_force_expanded_state (
				adapter, state);
		}

		if (message_list->cursor_uid != NULL)
			saveuid = find_next_selectable (message_list, regen_data->removed_uids);

		selected = message_list_get_selected (message_list);

		/* Show the cursor unless we're responding to a
		 * "folder-changed" signal from our CamelFolder. */
		build_tree (
			message_list,
			regen_data->thread_tree,
			regen_data->thread_flat,
			regen_data->thread_latest,
			regen_data->folder_changed);

		if (forcing_expand_state) {
			if (message_list->priv->folder != NULL && tree != NULL)
				save_tree_state (message_list, regen_data->folder);

			/* Disable forced expand/collapse state. */
			e_tree_table_adapter_force_expanded_state (adapter, 0);
		} else if (was_searching && !is_searching) {
			/* Load expand state from disk */
			load_tree_state (
				message_list,
				regen_data->folder,
				NULL);
		} else {
			/* Load expand state from the previous state or disk */
			load_tree_state (
				message_list,
				regen_data->folder,
				regen_data->expand_state);
		}

		message_list->expand_all = 0;
		message_list->collapse_all = 0;

		/* restore cursor position only after the expand state is restored,
		   thus the row numbers will actually match their real rows in UI */

		e_table_item_freeze (table_item);

		message_list_set_selected (message_list, selected);
		g_ptr_array_unref (selected);

		/* Show the cursor unless we're responding to a
		 * "folder-changed" signal from our CamelFolder. */
		if (regen_data->folder_changed && table_item != NULL)
			table_item->queue_show_cursor = FALSE;

		e_table_item_thaw (table_item);

		if ((!saveuid || !g_hash_table_lookup (message_list->uid_nodemap, saveuid)) &&
		    message_list->cursor_uid && g_hash_table_lookup (message_list->uid_nodemap, message_list->cursor_uid)) {
			/* this makes sure a visible node is selected, like when
			 * collapsing all nodes and a children had been selected
			 */
			g_free (saveuid);
			saveuid = g_strdup (message_list->cursor_uid);
		}

		if (message_list_selected_count (message_list) > 1) {
			g_free (saveuid);
		} else if (saveuid) {
			GNode *node;

			node = g_hash_table_lookup (
				message_list->uid_nodemap, saveuid);
			if (node == NULL) {
				g_free (message_list->cursor_uid);
				message_list->cursor_uid = NULL;
				g_signal_emit (
					message_list,
					signals[MESSAGE_SELECTED], 0, NULL);

			} else {
				GNode *parent = node;

				while ((parent = parent->parent) != NULL) {
					if (!e_tree_table_adapter_node_is_expanded (adapter, parent))
						node = parent;
				}

				e_table_item_freeze (table_item);

				e_tree_set_cursor (E_TREE (message_list), node);

				/* Show the cursor unless we're responding to a
				 * "folder-changed" signal from our CamelFolder. */
				if (regen_data->folder_changed && table_item != NULL)
					table_item->queue_show_cursor = FALSE;

				e_table_item_thaw (table_item);
			}
			g_free (saveuid);
		} else if (message_list->cursor_uid && !g_hash_table_lookup (message_list->uid_nodemap, message_list->cursor_uid)) {
			g_free (message_list->cursor_uid);
			message_list->cursor_uid = NULL;
			g_signal_emit (
				message_list,
				signals[MESSAGE_SELECTED], 0, NULL);
		}
	} else {
		build_flat (
			message_list,
			regen_data->summary,
			regen_data->folder_changed,
			regen_data->removed_uids);
	}

	row_count = e_table_model_row_count (E_TABLE_MODEL (adapter));

	if (start_selection_uid) {
		GNode *node;

		node = g_hash_table_lookup (message_list->uid_nodemap, start_selection_uid);
		if (node) {
			gint row;

			row = e_tree_table_adapter_row_of_node (adapter, node);

			if (row >= 0 && row < row_count)
				e_tree_selection_model_set_selection_start_row (E_TREE_SELECTION_MODEL (e_tree_get_selection_model (tree)), row);
		}

		camel_pstring_free (start_selection_uid);
	}

	if (regen_data->select_all) {
		message_list_select_all (message_list);

	} else if (regen_data->select_uid != NULL) {
		message_list_select_uid (
			message_list,
			regen_data->select_uid,
			regen_data->select_use_fallback);

	} else if (message_list->cursor_uid == NULL && last_row_uid) {
		GNode *node = NULL;
		gint sel_count;

		sel_count = message_list_selected_count (message_list);

		/* It can be that multi-select start and/or end had been removed, in which
		   case "clamp" the new start/end according to start/end of the restored
		   selection, even if it is not a consecutive selection (Shift+Arrow can
		   be broken after this "clamp"). */
		if (sel_count > 0) {
			GPtrArray *selected;

			selected = message_list_get_selected (message_list);

			if (selected && selected->len) {
				guint ii;
				gint min_row = -1, max_row = -1;

				for (ii = 0; ii < selected->len; ii++) {
					GNode *selected_node;

					selected_node = g_hash_table_lookup (message_list->uid_nodemap, g_ptr_array_index (selected, ii));
					if (selected_node) {
						gint selected_row;

						selected_row = e_tree_table_adapter_row_of_node (adapter, selected_node);

						if (selected_row >= 0 && selected_row < row_count) {
							if (min_row > selected_row || min_row == -1)
								min_row = selected_row;

							if (max_row < selected_row || max_row == -1)
								max_row = selected_row;
						}
					}
				}

				if (min_row != -1 && max_row != -1) {
					gint start_sel_row, new_last_row = regen_data->last_row;

					start_sel_row = e_tree_selection_model_get_selection_start_row (E_TREE_SELECTION_MODEL (e_tree_get_selection_model (tree)));
					node = g_hash_table_lookup (message_list->uid_nodemap, last_row_uid);
					if (node)
						new_last_row = e_tree_table_adapter_row_of_node (adapter, node);

					/* Swap them if needed */
					if (start_sel_row != -1 && start_sel_row > new_last_row) {
						gint tmp = min_row;
						min_row = max_row;
						max_row = tmp;
					}

					node = e_tree_table_adapter_node_at_row (adapter, max_row);
					if (node) {
						/* This also deselects rows */
						select_node (message_list, node);

						message_list_set_selected (message_list, selected);
						e_tree_selection_model_set_selection_start_row (E_TREE_SELECTION_MODEL (e_tree_get_selection_model (tree)), min_row);
					}
				}
			}

			if (selected)
				g_ptr_array_unref (selected);

			if (!node)
				sel_count = 0;
		}

		if (!node)
			node = g_hash_table_lookup (message_list->uid_nodemap, last_row_uid);

		if (!node) {
			if (regen_data->last_row >= row_count)
				regen_data->last_row = row_count - 1;

			if (regen_data->last_row != -1)
				node = e_tree_table_adapter_node_at_row (adapter, regen_data->last_row);
		}

		if (node && sel_count <= 1)
			select_node (message_list, node);
	}

	if (last_row_uid)
		camel_pstring_free (last_row_uid);

	message_list_update_tree_text (message_list);

	g_signal_handlers_unblock_by_func (
		adapter, ml_tree_sorting_changed, message_list);

	g_signal_emit (
		message_list,
		signals[MESSAGE_LIST_BUILT], 0);

	message_list->priv->any_row_changed = FALSE;
	message_list->just_set_folder = FALSE;

	if (!regen_data->select_all && regen_data->select_unread) {
		ETreePath cursor_path;
		gboolean call_select = TRUE;

		cursor_path = e_tree_get_cursor (E_TREE (message_list));

		if (cursor_path) {
			CamelMessageInfo *info;

			info = get_message_info (message_list, cursor_path);
			if (info && !(camel_message_info_get_flags (info) & CAMEL_MESSAGE_SEEN))
				call_select = FALSE;
		}

		if (call_select) {
			message_list_select (MESSAGE_LIST (message_list), MESSAGE_LIST_SELECT_NEXT |
				MESSAGE_LIST_SELECT_WRAP | MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED,
				0, CAMEL_MESSAGE_SEEN);
		}
	}
}

static gboolean
message_list_regen_idle_cb (gpointer user_data)
{
	GTask *task;
	RegenData *regen_data;
	MessageList *message_list;
	ETreeTableAdapter *adapter;
	gboolean searching;
	gint row_count;

	task = G_TASK (user_data);
	regen_data = g_task_get_task_data (task);
	message_list = g_task_get_source_object (task);

	g_mutex_lock (&message_list->priv->regen_lock);

	/* Capture MessageList state to use for this regen. */

	regen_data->group_by_threads = message_list_get_group_by_threads (message_list);
	regen_data->thread_subject = message_list_get_thread_subject (message_list);
	regen_data->thread_flat = message_list_get_thread_flat (message_list);
	regen_data->thread_latest = message_list_get_thread_latest (message_list);
	regen_data->select_unread = message_list_get_regen_selects_unread (message_list);

	if (regen_data->select_unread)
		message_list_set_regen_selects_unread (message_list, FALSE);

	searching = message_list_is_searching (message_list);

	adapter = e_tree_get_table_adapter (E_TREE (message_list));
	row_count = e_table_model_row_count (E_TABLE_MODEL (adapter));

	if (row_count <= 0) {
		if (gtk_widget_is_visible (GTK_WIDGET (message_list)))
			e_tree_set_info_message (E_TREE (message_list), _("Generating message list…"));
	} else if (regen_data->group_by_threads &&
		   !message_list->just_set_folder &&
		   !searching) {
		if (message_list->priv->any_row_changed) {
			/* Something changed.  If it was an expand
			 * state change, then save the expand state. */
			message_list_save_state (message_list);
		} else {
			/* Remember the expand state and restore it
			 * after regen. */
			regen_data->expand_state =
				e_tree_table_adapter_save_expanded_state_xml (
				adapter);
		}
	} else {
		/* Remember the expand state and restore it after regen. */
		regen_data->expand_state = e_tree_table_adapter_save_expanded_state_xml (adapter);
	}

	g_clear_pointer (&message_list->priv->regen_idle_source, g_source_unref);

	g_mutex_unlock (&message_list->priv->regen_lock);

	if (!g_task_return_error_if_cancelled (task))
		g_task_run_in_thread (task, message_list_regen_thread);

	return G_SOURCE_REMOVE;
}

static void
mail_regen_cancel (MessageList *message_list)
{
	GCancellable *cancellable = NULL;
	GTask *regen_task = NULL;
	gboolean idle_deleted = FALSE;

	g_mutex_lock (&message_list->priv->regen_lock);

	if (g_set_object (&regen_task, message_list->priv->regen_task))
		cancellable = g_task_get_cancellable (regen_task);

	if (message_list->priv->regen_idle_source) {
		g_source_destroy (message_list->priv->regen_idle_source);
		g_clear_pointer (&message_list->priv->regen_idle_source, g_source_unref);
		idle_deleted = TRUE;
	}

	g_mutex_unlock (&message_list->priv->regen_lock);

	/* Cancel outside the lock, since this will emit a signal. */
	if (cancellable != NULL)
		g_cancellable_cancel (cancellable);

	if (idle_deleted && regen_task)
		g_task_return_error_if_cancelled (regen_task);

	g_clear_object (&regen_task);
}

static void
mail_regen_list (MessageList *message_list,
                 const gchar *search,
                 CamelFolderChangeInfo *folder_changes)
{
	GTask *task;
	GCancellable *cancellable;
	RegenData *new_regen_data;
	GTask *old_regen_task = NULL;
	RegenData *old_regen_data = NULL;
	gchar *tmp_search_copy = NULL;

	if (!search) {
		g_mutex_lock (&message_list->priv->regen_lock);

		if (message_list->priv->regen_task)
			old_regen_data = g_task_get_task_data (message_list->priv->regen_task);

		if (old_regen_data && old_regen_data->folder == message_list->priv->folder) {
			tmp_search_copy = g_strdup (old_regen_data->search);
			search = tmp_search_copy;
		} else {
			tmp_search_copy = g_strdup (message_list->search);
			search = tmp_search_copy;
		}

		old_regen_data = NULL;
		g_mutex_unlock (&message_list->priv->regen_lock);
	} else if (search && !*search) {
		search = NULL;
	}

	/* Report empty search as NULL, not as one/two-space string. */
	if (search && (strcmp (search, " ") == 0 || strcmp (search, "  ") == 0))
		search = NULL;

	/* Can't list messages in a folder until we have a folder. */
	if (message_list->priv->folder == NULL) {
		g_free (message_list->search);
		message_list->search = g_strdup (search);
		g_free (tmp_search_copy);
		return;
	}

	g_mutex_lock (&message_list->priv->regen_lock);

	old_regen_task = g_steal_pointer (&message_list->priv->regen_task);
	if (old_regen_task)
		old_regen_data = g_task_get_task_data (old_regen_task);

	/* If a regen is scheduled but not yet started, just
	 * apply the argument values without cancelling it. */
	if (message_list->priv->regen_idle_source) {
		g_return_if_fail (old_regen_data != NULL);

		if (g_strcmp0 (search, old_regen_data->search) != 0) {
			g_free (old_regen_data->search);
			old_regen_data->search = g_strdup (search);
		}

		/* Only turn off the folder_changed flag, do not turn it on, because otherwise
		   the view may not scroll to the cursor position, due to claiming that
		   the regen was done for folder-changed signal, while the initial regen
		   request would be due to change of the folder in the view (or other similar
		   reasons). */
		if (!folder_changes) {
			old_regen_data->folder_changed = FALSE;
		} else if (folder_changes->uid_removed) {
			guint ii;

			if (!old_regen_data->removed_uids)
				old_regen_data->removed_uids = g_hash_table_new_full (g_direct_hash, g_direct_equal, (GDestroyNotify) camel_pstring_free, NULL);

			for (ii = 0; ii < folder_changes->uid_removed->len; ii++) {
				g_hash_table_insert (old_regen_data->removed_uids, (gpointer) camel_pstring_strdup (folder_changes->uid_removed->pdata[ii]), NULL);
			}
		}

		/* Avoid cancelling on the way out. */
		old_regen_data = NULL;
		message_list->priv->regen_task = g_steal_pointer (&old_regen_task);

		goto exit;
	}

	cancellable = g_cancellable_new ();

	new_regen_data = regen_data_new (message_list, cancellable);
	new_regen_data->search = g_strdup (search);
	/* Make sure the folder_changes won't reset currently running regen, which would scroll to the selection in the UI */
	new_regen_data->folder_changed = folder_changes != NULL && (!old_regen_data || old_regen_data->folder_changed);

	if (folder_changes && folder_changes->uid_removed && new_regen_data->folder_changed) {
		guint ii;

		new_regen_data->removed_uids = g_hash_table_new_full (g_direct_hash, g_direct_equal, (GDestroyNotify) camel_pstring_free, NULL);

		for (ii = 0; ii < folder_changes->uid_removed->len; ii++) {
			g_hash_table_insert (new_regen_data->removed_uids, (gpointer) camel_pstring_strdup (folder_changes->uid_removed->pdata[ii]), NULL);
		}
	}

	/* We generate the message list content in a worker thread, and
	 * then supply our own GAsyncReadyCallback to redraw the widget. */

	task = g_task_new (message_list, cancellable, message_list_regen_done_cb, NULL);
	g_task_set_source_tag (task, mail_regen_list);
	g_task_set_task_data (task, new_regen_data, (GDestroyNotify) regen_data_free);

	message_list->priv->regen_idle_source = g_idle_source_new ();
	g_task_attach_source (task,
		message_list->priv->regen_idle_source,
		message_list_regen_idle_cb);

	/* Set the regen_task immediately, but start the actual regen
	 * operation from an idle callback.  That way the caller has
	 * the remainder of this main loop iteration to make further
	 * MessageList changes without triggering additional regens. */
	message_list->priv->regen_task = g_steal_pointer (&task);

	g_object_unref (cancellable);

exit:
	g_mutex_unlock (&message_list->priv->regen_lock);

	/* Cancel outside the lock, since this will emit a signal. */
	if (old_regen_task != NULL) {
		e_activity_cancel (old_regen_data->activity);
		g_clear_object (&old_regen_task);
	}

	g_free (tmp_search_copy);
}

gboolean
message_list_contains_uid (MessageList *message_list,
			   const gchar *uid)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	if (!uid || !*uid || !message_list->priv->folder)
		return FALSE;

	return g_hash_table_lookup (message_list->uid_nodemap, uid) != NULL;
}

void
message_list_inc_setting_up_search_folder (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	g_atomic_int_add (&message_list->priv->setting_up_search_folder, 1);
}

void
message_list_dec_setting_up_search_folder (MessageList *message_list)
{
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (g_atomic_int_dec_and_test (&message_list->priv->setting_up_search_folder))
		message_list_update_tree_text (message_list);
}

gboolean
message_list_is_setting_up_search_folder (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	return g_atomic_int_get (&message_list->priv->setting_up_search_folder) > 0;
}
