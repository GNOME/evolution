/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <camel/camel.h>
#include <e-util/e-util.h>
#include <libebook-contacts/libebook-contacts.h>

#include <libemail-engine/libemail-engine.h>

#include "em-utils.h"
#include "e-mail-label-list-store.h"
#include "e-mail-ui-session.h"
#include "em-folder-tree-model.h"
#include "e-message-list.h"

/* EMessageListNode - row object wrapping CamelFolderViewRow or group */
#define E_TYPE_MESSAGE_LIST_NODE (e_message_list_node_get_type ())
G_DECLARE_FINAL_TYPE (EMessageListNode, e_message_list_node, E, MESSAGE_LIST_NODE, GObject)

struct _EMessageListNode {
	GObject parent_instance;
	CamelFolderViewRow *row;
	CamelFolderViewGeneration *generation;
	gchar *group_label;
	guint row_index;
	guint depth;
	gboolean expandable;
	gboolean expanded;
};

gchar *	_e_message_list_compute_avatar_initials	(const gchar *sender);
gboolean	_e_message_list_avatar_pixbuf_cache_contains_key	(EMessageList *self, const gchar *key);
void		_e_message_list_avatar_photo_miss_record		(EMessageList *self, const gchar *email);
gboolean	_e_message_list_avatar_photo_miss_is_recent		(EMessageList *self, const gchar *email);

G_DEFINE_FINAL_TYPE (EMessageListNode, e_message_list_node, G_TYPE_OBJECT)

static void
e_message_list_node_finalize (GObject *object)
{
	EMessageListNode *self = E_MESSAGE_LIST_NODE (object);

	self->row = NULL;
	g_clear_pointer (&self->generation, camel_folder_view_generation_unref);
	g_free (self->group_label);

	G_OBJECT_CLASS (e_message_list_node_parent_class)->finalize (object);
}

static void
e_message_list_node_class_init (EMessageListNodeClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = e_message_list_node_finalize;
}

static void
e_message_list_node_init (EMessageListNode *self)
{
}

/* EMessageListModel - EVirtualTreeModel bridging CamelFolderView */
#define E_TYPE_MESSAGE_LIST_MODEL (e_message_list_model_get_type ())
G_DECLARE_FINAL_TYPE (EMessageListModel, e_message_list_model, E, MESSAGE_LIST_MODEL, GObject)

struct _EMessageListModel {
	GObject parent_instance;
	CamelFolderView *folder_view;
	gboolean grouping_active;
	gulong sig_rows_changed;
	gulong sig_rows_inserted;
	gulong sig_rows_removed;
	gulong sig_row_count_changed;
};

static void e_message_list_model_iface_init (EVirtualTreeModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EMessageListModel, e_message_list_model, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_VIRTUAL_TREE_MODEL, e_message_list_model_iface_init))

static guint
model_get_row_count (EVirtualTreeModel *model)
{
	EMessageListModel *self = E_MESSAGE_LIST_MODEL (model);

	if (!self->folder_view)
		return 0;

	return camel_folder_view_get_row_count (self->folder_view);
}

static GPtrArray *
model_get_rows (EVirtualTreeModel *model,
		guint first_row,
		guint last_row,
		gboolean include_collapsed)
{
	EMessageListModel *self = E_MESSAGE_LIST_MODEL (model);
	GPtrArray *result;
	guint ii;

	result = g_ptr_array_new_with_free_func (g_object_unref);

	if (!self->folder_view)
		return result;

	for (ii = first_row; ii <= last_row; ii++) {
		EMessageListNode *node;

		node = g_object_new (E_TYPE_MESSAGE_LIST_NODE, NULL);
		node->row_index = ii;

		if (camel_folder_view_is_group_row (self->folder_view, ii)) {
			const gchar *label;

			label = camel_folder_view_get_group_label (self->folder_view, ii);
			node->group_label = g_strdup (label);
			node->depth = 0;
			node->expandable = TRUE;
			node->expanded = TRUE;
		} else {
			CamelFolderViewRow *row;

			row = camel_folder_view_get_row (self->folder_view, ii);
			if (row) {
				const gchar *uid = camel_folder_view_row_get_uid (row);
				guint depth;

				node->row = row;
				node->generation = camel_folder_view_ref_current_generation (self->folder_view);
				depth = camel_folder_view_get_depth (self->folder_view, uid);
				node->depth = self->grouping_active ? depth + 1 : depth;
				node->expandable = camel_folder_view_is_expandable (self->folder_view, uid);
				node->expanded = camel_folder_view_get_expanded (self->folder_view, uid);
			}
		}

		g_ptr_array_add (result, node);
	}

	return result;
}

static guint
model_get_depth (EVirtualTreeModel *model,
		 GObject *row_object)
{
	return E_MESSAGE_LIST_NODE (row_object)->depth;
}

static gboolean
model_is_expandable (EVirtualTreeModel *model,
		     GObject *row_object)
{
	return E_MESSAGE_LIST_NODE (row_object)->expandable;
}

static gboolean
model_get_expanded (EVirtualTreeModel *model,
		    GObject *row_object)
{
	return E_MESSAGE_LIST_NODE (row_object)->expanded;
}

static void
model_set_expanded (EVirtualTreeModel *model,
		    GObject *row_object,
		    gboolean expanded)
{
	EMessageListModel *self = E_MESSAGE_LIST_MODEL (model);
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (!self->folder_view || !node->row)
		return;

	camel_folder_view_set_expanded (self->folder_view,
		camel_folder_view_row_get_uid (node->row), expanded);
}

static void
on_fv_rows_changed (CamelFolderView *fv,
		    guint first_row,
		    guint last_row,
		    gpointer user_data)
{
	g_return_if_fail (e_util_is_main_thread (NULL));

	e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (user_data),
		first_row, last_row);
}

static void
on_fv_rows_inserted (CamelFolderView *fv,
		     guint first_row,
		     guint last_row,
		     gpointer user_data)
{
	g_return_if_fail (e_util_is_main_thread (NULL));

	e_virtual_tree_model_emit_rows_inserted (E_VIRTUAL_TREE_MODEL (user_data),
		first_row, last_row);
}

static void
on_fv_rows_removed (CamelFolderView *fv,
		    guint first_row,
		    guint last_row,
		    gpointer user_data)
{
	g_return_if_fail (e_util_is_main_thread (NULL));

	e_virtual_tree_model_emit_rows_removed (E_VIRTUAL_TREE_MODEL (user_data),
		first_row, last_row);
}

static void
on_fv_row_count_changed (CamelFolderView *fv,
			 gpointer user_data)
{
	g_return_if_fail (e_util_is_main_thread (NULL));

	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (user_data));
}

static void
e_message_list_model_disconnect (EMessageListModel *self)
{
	if (self->folder_view) {
		if (self->sig_rows_changed) {
			g_signal_handler_disconnect (self->folder_view, self->sig_rows_changed);
			self->sig_rows_changed = 0;
		}
		if (self->sig_rows_inserted) {
			g_signal_handler_disconnect (self->folder_view, self->sig_rows_inserted);
			self->sig_rows_inserted = 0;
		}
		if (self->sig_rows_removed) {
			g_signal_handler_disconnect (self->folder_view, self->sig_rows_removed);
			self->sig_rows_removed = 0;
		}
		if (self->sig_row_count_changed) {
			g_signal_handler_disconnect (self->folder_view, self->sig_row_count_changed);
			self->sig_row_count_changed = 0;
		}
	}

	g_clear_object (&self->folder_view);
}

static void
e_message_list_model_connect (EMessageListModel *self,
			      CamelFolderView *fv,
			      gboolean grouping)
{
	e_message_list_model_disconnect (self);

	if (!fv)
		return;

	self->folder_view = g_object_ref (fv);
	self->grouping_active = grouping;

	self->sig_rows_changed = g_signal_connect (fv, "rows-changed", G_CALLBACK (on_fv_rows_changed), self);
	self->sig_rows_inserted = g_signal_connect (fv, "rows-inserted", G_CALLBACK (on_fv_rows_inserted), self);
	self->sig_rows_removed = g_signal_connect (fv, "rows-removed", G_CALLBACK (on_fv_rows_removed), self);
	self->sig_row_count_changed = g_signal_connect (fv, "row-count-changed", G_CALLBACK (on_fv_row_count_changed), self);
}

static void
e_message_list_model_dispose (GObject *object)
{
	e_message_list_model_disconnect (E_MESSAGE_LIST_MODEL (object));
	G_OBJECT_CLASS (e_message_list_model_parent_class)->dispose (object);
}

static gconstpointer
model_get_row_key (EVirtualTreeModel *model,
		   GObject *row_object)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row)
		return camel_folder_view_row_get_uid (node->row);

	return NULL;
}

static EVirtualTreeKeyType
model_get_key_type (EVirtualTreeModel *model)
{
	EVirtualTreeKeyType kt = {
		g_direct_hash,
		g_direct_equal,
		(GBoxedCopyFunc) camel_pstring_strdup,
		(GDestroyNotify) camel_pstring_free
	};
	return kt;
}

static guint
model_find_row_by_key (EVirtualTreeModel *model,
		       gconstpointer key)
{
	EMessageListModel *self = E_MESSAGE_LIST_MODEL (model);

	if (!self->folder_view || !key)
		return G_MAXUINT;

	return camel_folder_view_find_row_by_uid (self->folder_view, key);
}

static void
e_message_list_model_iface_init (EVirtualTreeModelInterface *iface)
{
	iface->get_row_count = model_get_row_count;
	iface->dup_rows = model_get_rows;
	iface->get_depth = model_get_depth;
	iface->is_expandable = model_is_expandable;
	iface->get_expanded = model_get_expanded;
	iface->set_expanded = model_set_expanded;
	iface->get_row_key = model_get_row_key;
	iface->get_key_type = model_get_key_type;
	iface->find_row_by_key = model_find_row_by_key;
}

static void
e_message_list_model_class_init (EMessageListModelClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = e_message_list_model_dispose;
}

static void
e_message_list_model_init (EMessageListModel *self)
{
}

/* EMessageList widget */
struct _EMessageList {
	GtkBox parent_instance;

	EVirtualTree *vtree;
	EMessageListModel *model;
	CamelFolder *folder;
	CamelFolderView *folder_view;

	gboolean show_deleted;
	gboolean show_junk;
	CamelFolderViewThreading threading;
	CamelFolderViewGroupBy group_by;
	gboolean thread_subject;
	gboolean thread_latest;
	gboolean sort_children_ascending;

	CamelFolderViewColumn sort_column;
	CamelSortType sort_order;

	gchar *search_sexp;
	const gchar *ensure_uid;

	GMutex regen_lock;
	GTask *regen_task;
	GSource *regen_idle_source;
	EActivity *regen_activity;
	GTask *changes_task;
	gboolean changes_requeue;
	gboolean regen_was_cancelled;

	GSettings *mail_settings;
	GSettings *eds_settings;
	gboolean show_subject_above_sender;
	gboolean show_email;
	gboolean show_avatar;
	gboolean show_body_preview;

	GdkRGBA *new_mail_bg_color;
	gchar *new_mail_fg_color;
	gchar *important_fg_color;

	guint idle_id;
	guint update_actions_idle_id;
	guint seen_id;
	gboolean last_sel_single;

	const gchar *pending_select_uid;
	gboolean pending_select_fallback;

	EMailSession *session;

	GHashTable *avatar_fetch_pending;
	GCancellable *avatar_fetch_cancellable;
	GHashTable *avatar_pixbuf_cache;
	GHashTable *avatar_photo_miss_emails;
	guint avatar_pixbuf_cache_sweep_id;
	gdouble avatar_font_scale;

	GPtrArray *clipboard_uids;
	CamelFolder *clipboard_folder;
	GtkWidget *invisible;

	guint frozen;
	gboolean thaw_needs_regen;
	gboolean regen_selects_unread;
	gboolean expanded_default;
	gboolean is_trash_folder;
	gboolean is_junk_folder;
	volatile gint setting_up_search_folder;
	gboolean just_set_folder;
};

enum {
	DND_X_UID_LIST,
	DND_MESSAGE_RFC822,
	DND_TEXT_URI_LIST
};

static GtkTargetEntry ml_drag_types[] = {
	{ (gchar *) "x-uid-list", 0, DND_X_UID_LIST },
	{ (gchar *) "text/uri-list", 0, DND_TEXT_URI_LIST },
};

static GtkTargetEntry ml_drop_types[] = {
	{ (gchar *) "x-uid-list", 0, DND_X_UID_LIST },
	{ (gchar *) "message/rfc822", 0, DND_MESSAGE_RFC822 },
	{ (gchar *) "text/uri-list", 0, DND_TEXT_URI_LIST },
};

enum {
	MESSAGE_SELECTED,
	MESSAGE_LIST_BUILT,
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_PASTE_TARGET_LIST,
	PROP_THREADING
};

static void e_message_list_selectable_init (ESelectableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EMessageList, e_message_list, GTK_TYPE_BOX,
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, e_message_list_selectable_init))

static void
message_list_push_show_flags (EMessageList *self,
                              CamelFolderView *folder_view)
{
	gboolean show_deleted, show_junk;

	show_deleted = self->show_deleted || self->is_trash_folder;
	show_junk = self->show_junk || self->is_trash_folder || self->is_junk_folder;

	camel_folder_view_set_show_deleted (folder_view, show_deleted);
	camel_folder_view_set_show_junk (folder_view, show_junk);
}

static void
update_empty_message (EMessageList *self)
{
	guint row_count;

	if (!self->vtree)
		return;

	if (!self->folder) {
		e_virtual_tree_set_empty_message (self->vtree, _("No folder selected."));
		return;
	}

	if (self->regen_task || g_atomic_int_get (&self->setting_up_search_folder) > 0) {
		e_virtual_tree_set_empty_message (self->vtree, _("Generating message list…"));
		return;
	}

	row_count = self->folder_view ? camel_folder_view_get_row_count (self->folder_view) : 0;

	if (row_count > 0)
		e_virtual_tree_set_empty_message (self->vtree, NULL);
	else if (self->regen_was_cancelled)
		e_virtual_tree_set_empty_message (self->vtree, _("Message list generation has been cancelled."));
	else if (self->search_sexp && *self->search_sexp)
		e_virtual_tree_set_empty_message (self->vtree,
			_("No message satisfies your search criteria. "
			"Change search criteria by selecting a new "
			"Show message filter from the drop down list "
			"above or by running a new search either by "
			"clearing it with Search->Clear menu item or "
			"by changing the query above."));
	else
		e_virtual_tree_set_empty_message (self->vtree, _("There are no messages in this folder."));
}

static gchar *
format_date (gint64 epoch)
{
	if (epoch <= 0)
		return g_strdup ("");

	return e_datetime_format_format ("mail", "table", DTFormatKindDateTime, (time_t) epoch);
}

static gchar *
format_size (guint32 size)
{
	return g_format_size ((guint64) size);
}

static EMessageList *
get_message_list (EVirtualTree *tree)
{
	GtkWidget *ancestor;

	ancestor = gtk_widget_get_ancestor (GTK_WIDGET (tree), E_TYPE_MESSAGE_LIST);

	return ancestor ? E_MESSAGE_LIST (ancestor) : NULL;
}

static const gchar *
get_label_color (EMessageList *self,
		 CamelFolderViewRow *row)
{
	EMailLabelListStore *label_store;
	const gchar *labels;
	gchar **tags;
	GdkRGBA color;
	const gchar *result = NULL;
	guint ii, n_found;

	labels = camel_folder_view_row_get_labels (row);
	if (!labels || !*labels)
		return NULL;

	if (!E_IS_MAIL_UI_SESSION (self->session))
		return NULL;

	label_store = e_mail_ui_session_get_label_store (E_MAIL_UI_SESSION (self->session));
	if (!label_store)
		return NULL;

	tags = g_strsplit (labels, ",", -1);
	n_found = 0;

	for (ii = 0; tags[ii]; ii++) {
		GtkTreeIter iter;

		if (e_mail_label_list_store_lookup (label_store, tags[ii], &iter))
			n_found++;
	}

	if (n_found == 1) {
		for (ii = 0; tags[ii]; ii++) {
			GtkTreeIter iter;

			if (e_mail_label_list_store_lookup (label_store, tags[ii], &iter) &&
			    e_mail_label_list_store_get_color (label_store, &iter, &color)) {
				gchar *tmp = gdk_rgba_to_string (&color);
				result = g_intern_string (tmp);
				g_free (tmp);
				break;
			}
		}
	} else if (n_found > 1) {
		GtkTreeModel *model = GTK_TREE_MODEL (label_store);
		GtkTreeIter titer;

		if (gtk_tree_model_get_iter_first (model, &titer)) {
			do {
				gchar *tag = e_mail_label_list_store_get_tag (label_store, &titer);
				gboolean found = FALSE;

				for (ii = 0; tags[ii] && !found; ii++) {
					if (g_strcmp0 (tag, tags[ii]) == 0)
						found = TRUE;
				}

				g_free (tag);

				if (found && e_mail_label_list_store_get_color (label_store, &titer, &color)) {
					gchar *tmp = gdk_rgba_to_string (&color);
					result = g_intern_string (tmp);
					g_free (tmp);
					break;
				}
			} while (gtk_tree_model_iter_next (model, &titer));
		}
	}

	g_strfreev (tags);

	return result;
}

#define DEFAULT_IMPORTANT_FG_COLOR_LIGHT	"#A7453E"
#define DEFAULT_IMPORTANT_FG_COLOR_DARK	"#FF6B6B"

static const gchar *
get_row_foreground_color (EMessageList *self,
			  CamelFolderViewRow *row,
			  gboolean *out_is_junk_color)
{
	const gchar *foreground;
	const gchar *important_fg_color;
	guint32 flags;

	if (out_is_junk_color)
		*out_is_junk_color = FALSE;

	flags = camel_folder_view_row_get_flags (row);

	if ((flags & CAMEL_MESSAGE_JUNK) &&
	    !(flags & CAMEL_MESSAGE_DELETED) &&
	    !self->is_junk_folder) {
		if (out_is_junk_color)
			*out_is_junk_color = TRUE;
		return "#FF0000";
	}

	important_fg_color = self->important_fg_color ? self->important_fg_color : DEFAULT_IMPORTANT_FG_COLOR_LIGHT;

	foreground = get_label_color (self, row);

	if (!foreground && (flags & CAMEL_MESSAGE_FLAGGED))
		foreground = important_fg_color;

	if (!foreground) {
		const gchar *followup = camel_folder_view_row_get_followup_flag (row);
		gint64 due_by = camel_folder_view_row_get_followup_due_by (row);

		if (((followup && *followup) || due_by > 0) &&
		    !camel_folder_view_row_get_followup_completed (row)) {
			if ((followup && *followup) || due_by <= (gint64) time (NULL))
				foreground = important_fg_color;
		}
	}

	if (!foreground)
		foreground = camel_folder_view_row_get_color (row);

	if (!foreground && !(flags & CAMEL_MESSAGE_SEEN) && self->new_mail_fg_color)
		foreground = self->new_mail_fg_color;

	return foreground;
}

static gboolean
message_list_selected_row_color_func (EVirtualTree *tree,
				      GObject *row_object,
				      guint visible_row,
				      GdkRGBA *out_color,
				      gpointer user_data)
{
	EMessageList *self = user_data;
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);
	const gchar *color_spec;
	gboolean found = FALSE;

	if (!node->row)
		return FALSE;

	camel_folder_view_row_lock (node->row);

	color_spec = get_row_foreground_color (self, node->row, NULL);
	if (color_spec && gdk_rgba_parse (out_color, color_spec))
		found = TRUE;

	camel_folder_view_row_unlock (node->row);

	return found;
}

static void
apply_message_style (EMessageList *self,
		     GtkCellRenderer *renderer,
		     CamelFolderViewRow *row)
{
	const gchar *foreground;
	guint32 flags;
	gboolean unread, deleted, junk, strikethrough, is_junk_color;

	if (!row)
		return;

	camel_folder_view_row_lock (row);

	flags = camel_folder_view_row_get_flags (row);
	unread = !(flags & CAMEL_MESSAGE_SEEN);
	deleted = (flags & CAMEL_MESSAGE_DELETED) != 0;
	junk = (flags & CAMEL_MESSAGE_JUNK) != 0;

	if (self->is_trash_folder && self->is_junk_folder)
		strikethrough = FALSE;
	else if (self->is_trash_folder)
		strikethrough = junk;
	else if (self->is_junk_folder)
		strikethrough = deleted;
	else
		strikethrough = deleted || junk;

	g_object_set (renderer,
		"weight", unread ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		"strikethrough", strikethrough,
		"style", camel_folder_view_row_get_ignore_thread (row) ?
			PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
		NULL);

	is_junk_color = FALSE;
	foreground = get_row_foreground_color (self, row, &is_junk_color);

	if (is_junk_color) {
		GdkRGBA red = { 1.0, 0.0, 0.0, 1.0 };
		g_object_set (renderer,
			"foreground-rgba", &red,
			"foreground-set", TRUE,
			NULL);
		camel_folder_view_row_unlock (row);
		return;
	}

	if (foreground)
		g_object_set (renderer,
			"foreground", foreground,
			"foreground-set", TRUE,
			NULL);
	else
		g_object_set (renderer,
			"foreground-set", FALSE,
			NULL);

	camel_folder_view_row_unlock (row);

	if (unread && self->new_mail_bg_color)
		g_object_set (renderer,
			"cell-background-rgba", self->new_mail_bg_color,
			"cell-background-set", TRUE,
			NULL);
	else
		g_object_set (renderer,
			"cell-background-set", FALSE,
			NULL);
}

static void
status_icon_data_func (EVirtualTree *tree,
		       GtkCellRenderer *renderer,
		       GObject *row_object,
		       guint visible_row,
		       gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);
	const gchar *icon_name = NULL;
	guint32 flags;

	if (!node->row)
		return;

	flags = camel_folder_view_row_get_flags (node->row);

	if (!(flags & CAMEL_MESSAGE_SEEN) && (flags & CAMEL_MESSAGE_ANSWERED))
		icon_name = "mail-replied";
	else if (!(flags & CAMEL_MESSAGE_SEEN) && (flags & CAMEL_MESSAGE_FORWARDED))
		icon_name = "mail-forward";
	else if (!(flags & CAMEL_MESSAGE_SEEN))
		icon_name = "mail-unread";
	else if (flags & CAMEL_MESSAGE_ANSWERED)
		icon_name = "mail-replied";
	else if (flags & CAMEL_MESSAGE_FORWARDED)
		icon_name = "mail-forward";
	else
		icon_name = "mail-read";

	g_object_set (renderer, "icon-name", icon_name, NULL);
}

static void
attachment_icon_data_func (EVirtualTree *tree,
			   GtkCellRenderer *renderer,
			   GObject *row_object,
			   guint visible_row,
			   gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);
	guint32 flags;

	if (!node->row)
		return;

	flags = camel_folder_view_row_get_flags (node->row);
	g_object_set (renderer, "icon-name",
		(flags & CAMEL_MESSAGE_ATTACHMENTS) ? "mail-attachment" : NULL,
		NULL);
}

static void
flagged_icon_data_func (EVirtualTree *tree,
			GtkCellRenderer *renderer,
			GObject *row_object,
			guint visible_row,
			gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);
	guint32 flags;

	if (!node->row)
		return;

	flags = camel_folder_view_row_get_flags (node->row);
	g_object_set (renderer, "icon-name",
		(flags & CAMEL_MESSAGE_FLAGGED) ? "emblem-important" : NULL,
		NULL);
}

static void
from_data_func (EVirtualTree *tree,
		GtkCellRenderer *renderer,
		GObject *row_object,
		guint visible_row,
		gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row) {
		camel_folder_view_row_lock (node->row);
		g_object_set (renderer, "text", camel_folder_view_row_get_from (node->row), NULL);
		camel_folder_view_row_unlock (node->row);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else if (node->group_label) {
		g_object_set (renderer, "text", node->group_label,
			"weight", PANGO_WEIGHT_BOLD,
			"strikethrough", FALSE,
			"style", PANGO_STYLE_NORMAL,
			NULL);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

static void
subject_data_func (EVirtualTree *tree,
		   GtkCellRenderer *renderer,
		   GObject *row_object,
		   guint visible_row,
		   gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row) {
		camel_folder_view_row_lock (node->row);
		g_object_set (renderer, "text", camel_folder_view_row_get_subject (node->row), NULL);
		camel_folder_view_row_unlock (node->row);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else if (node->group_label) {
		g_object_set (renderer, "text", node->group_label,
			"weight", PANGO_WEIGHT_BOLD,
			"strikethrough", FALSE,
			"style", PANGO_STYLE_NORMAL,
			NULL);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

static void
date_data_func (EVirtualTree *tree,
		GtkCellRenderer *renderer,
		GObject *row_object,
		guint visible_row,
		gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row) {
		gchar *text = format_date (camel_folder_view_row_get_date_sent (node->row));
		g_object_set (renderer, "text", text, NULL);
		g_free (text);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

static void
size_data_func (EVirtualTree *tree,
		GtkCellRenderer *renderer,
		GObject *row_object,
		guint visible_row,
		gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row) {
		gchar *text = format_size (camel_folder_view_row_get_size (node->row));
		g_object_set (renderer, "text", text, NULL);
		g_free (text);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

typedef const gchar * (* TextColumnGetter) (CamelFolderViewRow *row);

static void
text_column_data_func (EVirtualTree *tree,
		       GtkCellRenderer *renderer,
		       GObject *row_object,
		       guint visible_row,
		       gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);
	TextColumnGetter getter = (TextColumnGetter) user_data;

	if (node->row) {
		camel_folder_view_row_lock (node->row);
		g_object_set (renderer, "text", getter (node->row), NULL);
		camel_folder_view_row_unlock (node->row);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

static void
date_received_data_func (EVirtualTree *tree,
			 GtkCellRenderer *renderer,
			 GObject *row_object,
			 guint visible_row,
			 gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row) {
		gchar *text = format_date (camel_folder_view_row_get_date_received (node->row));
		g_object_set (renderer, "text", text, NULL);
		g_free (text);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

static void
followup_due_by_data_func (EVirtualTree *tree,
			   GtkCellRenderer *renderer,
			   GObject *row_object,
			   guint visible_row,
			   gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row) {
		gchar *text = format_date (camel_folder_view_row_get_followup_due_by (node->row));
		g_object_set (renderer, "text", text, NULL);
		g_free (text);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

static void
followup_flag_status_data_func (EVirtualTree *tree,
				GtkCellRenderer *renderer,
				GObject *row_object,
				guint visible_row,
				gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);
	const gchar *icon_name = NULL;

	if (node->row) {
		const gchar *flag;

		camel_folder_view_row_lock (node->row);

		flag = camel_folder_view_row_get_followup_flag (node->row);
		if (flag && *flag) {
			if (camel_folder_view_row_get_followup_completed (node->row))
				icon_name = "stock_mail-flag-for-followup-done";
			else
				icon_name = "stock_mail-flag-for-followup";
		}

		camel_folder_view_row_unlock (node->row);
	}

	g_object_set (renderer, "icon-name", icon_name, NULL);
}

static void
score_data_func (EVirtualTree *tree,
		 GtkCellRenderer *renderer,
		 GObject *row_object,
		 guint visible_row,
		 gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row) {
		gint score = camel_folder_view_row_get_score (node->row);
		const gchar *icon_name = NULL;

		if (score <= -3)
			icon_name = "stock_score-lowest";
		else if (score <= -1)
			icon_name = "stock_score-lower";
		else if (score >= 3)
			icon_name = "stock_score-highest";
		else if (score >= 1)
			icon_name = "stock_score-higher";
		else
			icon_name = "stock_score-normal";

		g_object_set (renderer, "icon-name", icon_name, NULL);
	} else {
		g_object_set (renderer, "icon-name", NULL, NULL);
	}
}

static void
user_header_data_func (EVirtualTree *tree,
		       GtkCellRenderer *renderer,
		       GObject *row_object,
		       guint visible_row,
		       gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);
	guint index = GPOINTER_TO_UINT (user_data);

	if (node->row) {
		g_object_set (renderer, "text", camel_folder_view_row_get_user_header (node->row, index), NULL);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

static const struct {
	guint col_idx;
	CamelFolderViewColumn fv_column;
} column_map[] = {
	{ E_MESSAGE_LIST_COLUMN_STATUS,          CAMEL_FOLDER_VIEW_COLUMN_STATUS },
	{ E_MESSAGE_LIST_COLUMN_ATTACHMENT,      CAMEL_FOLDER_VIEW_COLUMN_ATTACHMENT },
	{ E_MESSAGE_LIST_COLUMN_FLAGGED,         CAMEL_FOLDER_VIEW_COLUMN_FLAGGED },
	{ E_MESSAGE_LIST_COLUMN_FROM,            CAMEL_FOLDER_VIEW_COLUMN_FROM },
	{ E_MESSAGE_LIST_COLUMN_SUBJECT,         CAMEL_FOLDER_VIEW_COLUMN_SUBJECT },
	{ E_MESSAGE_LIST_COLUMN_DATE_SENT,       CAMEL_FOLDER_VIEW_COLUMN_DATE_SENT },
	{ E_MESSAGE_LIST_COLUMN_SIZE,            CAMEL_FOLDER_VIEW_COLUMN_SIZE },
	{ E_MESSAGE_LIST_COLUMN_TO,              CAMEL_FOLDER_VIEW_COLUMN_TO },
	{ E_MESSAGE_LIST_COLUMN_CC,              CAMEL_FOLDER_VIEW_COLUMN_CC },
	{ E_MESSAGE_LIST_COLUMN_DATE_RECEIVED,   CAMEL_FOLDER_VIEW_COLUMN_DATE_RECEIVED },
	{ E_MESSAGE_LIST_COLUMN_SENDER,          CAMEL_FOLDER_VIEW_COLUMN_SENDER },
	{ E_MESSAGE_LIST_COLUMN_SENDER_MAIL,     CAMEL_FOLDER_VIEW_COLUMN_SENDER_MAIL },
	{ E_MESSAGE_LIST_COLUMN_RECIPIENTS,      CAMEL_FOLDER_VIEW_COLUMN_RECIPIENTS },
	{ E_MESSAGE_LIST_COLUMN_RECIPIENTS_MAIL, CAMEL_FOLDER_VIEW_COLUMN_RECIPIENTS_MAIL },
	{ E_MESSAGE_LIST_COLUMN_CORRESPONDENTS,  CAMEL_FOLDER_VIEW_COLUMN_CORRESPONDENTS },
	{ E_MESSAGE_LIST_COLUMN_SUBJECT_TRIMMED, CAMEL_FOLDER_VIEW_COLUMN_SUBJECT_TRIMMED },
	{ E_MESSAGE_LIST_COLUMN_LABELS,          CAMEL_FOLDER_VIEW_COLUMN_LABELS },
	{ E_MESSAGE_LIST_COLUMN_MLIST,           CAMEL_FOLDER_VIEW_COLUMN_MLIST },
	{ E_MESSAGE_LIST_COLUMN_FOLLOWUP_FLAG_STATUS, CAMEL_FOLDER_VIEW_COLUMN_FOLLOWUP_FLAG },
	{ E_MESSAGE_LIST_COLUMN_FOLLOWUP_FLAG,   CAMEL_FOLDER_VIEW_COLUMN_FOLLOWUP_FLAG },
	{ E_MESSAGE_LIST_COLUMN_FOLLOWUP_DUE_BY, CAMEL_FOLDER_VIEW_COLUMN_FOLLOWUP_DUE_BY },
	{ E_MESSAGE_LIST_COLUMN_SCORE,           CAMEL_FOLDER_VIEW_COLUMN_SCORE },
	{ E_MESSAGE_LIST_COLUMN_LOCATION,        CAMEL_FOLDER_VIEW_COLUMN_LOCATION },
	{ E_MESSAGE_LIST_COLUMN_PREVIEW,         CAMEL_FOLDER_VIEW_COLUMN_PREVIEW },
	{ E_MESSAGE_LIST_COLUMN_USER_HEADER_1,   CAMEL_FOLDER_VIEW_COLUMN_USER_HEADER_1 },
	{ E_MESSAGE_LIST_COLUMN_USER_HEADER_2,   CAMEL_FOLDER_VIEW_COLUMN_USER_HEADER_2 },
	{ E_MESSAGE_LIST_COLUMN_USER_HEADER_3,   CAMEL_FOLDER_VIEW_COLUMN_USER_HEADER_3 },
	{ E_MESSAGE_LIST_COLUMN_UID,             CAMEL_FOLDER_VIEW_COLUMN_UID }
};

static gboolean
column_index_to_sort_column (guint col_idx,
			     CamelFolderViewColumn *out_column)
{
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (column_map); ii++) {
		if (column_map[ii].col_idx == col_idx) {
			*out_column = column_map[ii].fv_column;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
sort_column_to_column_index (CamelFolderViewColumn column,
			     guint *out_col_idx)
{
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (column_map); ii++) {
		if (column_map[ii].fv_column == column) {
			*out_col_idx = column_map[ii].col_idx;
			return TRUE;
		}
	}

	return FALSE;
}

static void message_list_regen (EMessageList *self);
static void on_folder_view_folder_changed (CamelFolderView *folder_view, gpointer user_data);

static void
on_folder_view_rebuild_needed (CamelFolderView *folder_view,
			       gpointer user_data)
{
	message_list_regen (E_MESSAGE_LIST (user_data));
}

typedef struct _ProcessChangesTaskData {
	CamelFolderView *folder_view;
	CamelFolderViewGeneration *old_generation;
	gboolean cursor_was_visible;
} ProcessChangesTaskData;

static void
process_changes_task_data_free (gpointer data)
{
	ProcessChangesTaskData *td = data;

	g_clear_object (&td->folder_view);
	g_clear_pointer (&td->old_generation, camel_folder_view_generation_unref);
	g_free (td);
}

static void
message_list_process_changes_thread (GTask *task,
				     gpointer source_object,
				     gpointer task_data,
				     GCancellable *cancellable)
{
	ProcessChangesTaskData *td = task_data;
	GError *local_error = NULL;

	if (g_task_return_error_if_cancelled (task))
		return;

	if (!camel_folder_view_process_pending_changes_sync (td->folder_view, td->old_generation, cancellable, &local_error))
		g_task_return_error (task, local_error);
	else
		g_task_return_boolean (task, TRUE);
}

static void
message_list_process_changes_done_cb (GObject *source_object,
				      GAsyncResult *result,
				      gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (source_object);
	GTask *task = G_TASK (result);
	ProcessChangesTaskData *td = g_task_get_task_data (task);
	GError *local_error = NULL;
	gboolean requeue;
	gboolean success;

	success = g_task_propagate_boolean (task, &local_error);
	if (!success) {
		if (local_error && !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Message list incremental update failed: %s", local_error->message);
		g_clear_error (&local_error);
	}

	if (success && td->cursor_was_visible && self->vtree) {
		gint cursor_row = e_virtual_tree_get_cursor (self->vtree);

		if (cursor_row >= 0 && !e_virtual_tree_row_is_visible (self->vtree, (guint) cursor_row))
			e_virtual_tree_scroll_cursor_into_view (self->vtree);
	}

	g_mutex_lock (&self->regen_lock);
	if (self->changes_task == task)
		g_clear_object (&self->changes_task);
	requeue = self->changes_requeue;
	self->changes_requeue = FALSE;
	g_mutex_unlock (&self->regen_lock);

	if (requeue && self->folder_view)
		on_folder_view_folder_changed (self->folder_view, self);

	g_signal_emit (self, signals[MESSAGE_LIST_BUILT], 0);
}

static void
on_folder_view_folder_changed (CamelFolderView *folder_view,
			       gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);
	GTask *task;
	GCancellable *cancellable;
	ProcessChangesTaskData *td;

	g_mutex_lock (&self->regen_lock);

	if (self->regen_task || self->regen_idle_source) {
		g_mutex_unlock (&self->regen_lock);
		return;
	}

	if (self->changes_task) {
		self->changes_requeue = TRUE;
		g_mutex_unlock (&self->regen_lock);
		return;
	}

	self->changes_requeue = FALSE;

	cancellable = g_cancellable_new ();
	task = g_task_new (self, cancellable, message_list_process_changes_done_cb, NULL);
	td = g_new0 (ProcessChangesTaskData, 1);
	td->folder_view = g_object_ref (folder_view);
	td->old_generation = camel_folder_view_ref_current_generation (folder_view);
	if (self->vtree) {
		gint cursor_row = e_virtual_tree_get_cursor (self->vtree);

		td->cursor_was_visible = cursor_row >= 0 &&
			e_virtual_tree_row_is_visible (self->vtree, (guint) cursor_row);
	}
	g_task_set_task_data (task, td, process_changes_task_data_free);
	self->changes_task = task;

	g_mutex_unlock (&self->regen_lock);

	g_task_run_in_thread (task, message_list_process_changes_thread);

	g_object_unref (cancellable);
}

static void
message_list_regen_cancel (EMessageList *self)
{
	GCancellable *cancellable = NULL;
	GCancellable *changes_cancellable = NULL;

	g_mutex_lock (&self->regen_lock);

	if (self->regen_idle_source) {
		g_source_destroy (self->regen_idle_source);
		g_clear_pointer (&self->regen_idle_source, g_source_unref);
	}

	if (self->regen_activity) {
		e_activity_cancel (self->regen_activity);
		g_clear_object (&self->regen_activity);
	}

	if (self->regen_task)
		g_set_object (&cancellable, g_task_get_cancellable (self->regen_task));

	if (self->changes_task)
		g_set_object (&changes_cancellable, g_task_get_cancellable (self->changes_task));
	self->changes_requeue = FALSE;

	g_mutex_unlock (&self->regen_lock);

	g_cancellable_cancel (cancellable);
	g_clear_object (&cancellable);
	g_cancellable_cancel (changes_cancellable);
	g_clear_object (&changes_cancellable);
}

static void
message_list_regen_done_cb (GObject *source_object,
			    GAsyncResult *result,
			    gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (source_object);
	GTask *task = G_TASK (result);
	EActivity *activity = NULL;
	CamelFolderView *folder_view;
	GError *local_error = NULL;

	if (!g_task_propagate_boolean (task, &local_error)) {
		g_mutex_lock (&self->regen_lock);
		if (self->regen_task == task)
			g_clear_object (&self->regen_task);
		if (self->regen_activity) {
			if (e_activity_handle_cancellation (self->regen_activity, local_error)) {
				self->regen_was_cancelled = TRUE;
			} else {
				EAlertSink *alert_sink;

				alert_sink = e_activity_get_alert_sink (self->regen_activity);
				if (alert_sink && local_error)
					e_alert_submit (alert_sink, "mail:message-list-regen-failed", local_error->message, NULL);
				else if (local_error)
					g_warning ("Message list regen failed: %s", local_error->message);
			}
			activity = g_steal_pointer (&self->regen_activity);
		} else if (local_error && !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_warning ("Message list regen failed: %s", local_error->message);
		}
		g_mutex_unlock (&self->regen_lock);
		g_clear_object (&activity);
		update_empty_message (self);
		g_clear_error (&local_error);
		return;
	}

	folder_view = g_task_get_task_data (task);

	g_mutex_lock (&self->regen_lock);
	if (self->regen_task == task)
		g_clear_object (&self->regen_task);
	if (self->regen_activity) {
		e_activity_set_state (self->regen_activity, E_ACTIVITY_COMPLETED);
		activity = g_steal_pointer (&self->regen_activity);
	}
	g_mutex_unlock (&self->regen_lock);
	g_clear_object (&activity);

	if (self->folder != camel_folder_view_get_folder (folder_view))
		return;

	if (!self->vtree)
		return;

	if (self->folder_view) {
		g_signal_handlers_disconnect_by_func (self->folder_view, on_folder_view_rebuild_needed, self);
		g_signal_handlers_disconnect_by_func (self->folder_view, on_folder_view_folder_changed, self);
	}
	g_set_object (&self->folder_view, folder_view);

	g_signal_connect (self->folder_view, "rebuild-needed",
		G_CALLBACK (on_folder_view_rebuild_needed), self);
	g_signal_connect (self->folder_view, "folder-changed",
		G_CALLBACK (on_folder_view_folder_changed), self);

	e_message_list_model_disconnect (self->model);
	e_virtual_tree_set_model (self->vtree, NULL);

	e_message_list_model_connect (self->model, self->folder_view,
		self->group_by != CAMEL_FOLDER_VIEW_GROUP_BY_NONE);

	e_virtual_tree_set_model (self->vtree, E_VIRTUAL_TREE_MODEL (self->model));
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (self->model));
	update_empty_message (self);

	if (self->pending_select_uid) {
		const gchar *uid = self->pending_select_uid;
		gboolean fallback = self->pending_select_fallback;

		self->pending_select_uid = NULL;
		self->pending_select_fallback = FALSE;
		e_message_list_select_uid (self, uid, fallback);
		camel_pstring_free (uid);
	}

	if (self->regen_selects_unread) {
		self->regen_selects_unread = FALSE;
		e_message_list_select (self,
			E_MESSAGE_LIST_SELECT_NEXT | E_MESSAGE_LIST_SELECT_WRAP,
			0, CAMEL_MESSAGE_SEEN);
	}

	self->just_set_folder = FALSE;
	g_signal_emit (self, signals[MESSAGE_LIST_BUILT], 0);
}

static void
message_list_regen_thread (GTask *task,
			   gpointer source_object,
			   gpointer task_data,
			   GCancellable *cancellable)
{
	CamelFolderView *folder_view = task_data;
	GError *local_error = NULL;

	if (g_task_return_error_if_cancelled (task))
		return;

	if (!camel_folder_view_rebuild_sync (folder_view, cancellable, &local_error))
		g_task_return_error (task, local_error);
	else
		g_task_return_boolean (task, TRUE);
}

static gboolean
message_list_regen_idle_cb (gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	EMessageList *self;
	CamelFolderView *folder_view;

	if (g_task_return_error_if_cancelled (task))
		return G_SOURCE_REMOVE;

	self = g_task_get_source_object (task);
	folder_view = g_task_get_task_data (task);

	g_mutex_lock (&self->regen_lock);
	g_clear_pointer (&self->regen_idle_source, g_source_unref);
	g_mutex_unlock (&self->regen_lock);

	g_signal_handlers_disconnect_by_func (folder_view, on_folder_view_rebuild_needed, self);
	g_signal_handlers_disconnect_by_func (folder_view, on_folder_view_folder_changed, self);

	e_message_list_model_disconnect (self->model);
	e_virtual_tree_set_model (self->vtree, NULL);

	camel_folder_view_set_threading (folder_view, self->threading);
	camel_folder_view_set_thread_subject (folder_view, self->thread_subject);
	camel_folder_view_set_thread_latest (folder_view, self->thread_latest);
	camel_folder_view_set_sort_children_ascending (folder_view, self->sort_children_ascending);
	camel_folder_view_set_group_by (folder_view, self->group_by);

	camel_folder_view_set_filter (folder_view, self->search_sexp);
	camel_folder_view_set_ensure_uid (folder_view, self->ensure_uid);
	message_list_push_show_flags (self, folder_view);

	camel_folder_view_set_sort (folder_view,
		self->sort_column, self->sort_order);

	update_empty_message (self);

	if (self->session && E_IS_MAIL_UI_SESSION (self->session)) {
		EActivity *activity;

		activity = e_activity_new ();
		e_activity_set_cancellable (activity, g_task_get_cancellable (task));
		e_activity_set_text (activity, _("Generating message list…"));
		e_mail_ui_session_add_activity (E_MAIL_UI_SESSION (self->session), activity);

		g_mutex_lock (&self->regen_lock);
		g_set_object (&self->regen_activity, activity);
		g_mutex_unlock (&self->regen_lock);

		g_object_unref (activity);
	}

	g_task_run_in_thread (task, message_list_regen_thread);

	return G_SOURCE_REMOVE;
}

static void
message_list_regen (EMessageList *self)
{
	GTask *task;
	GCancellable *cancellable;
	GTask *old_task = NULL;
	GTask *old_changes_task = NULL;

	if (!self->folder || !self->folder_view)
		return;

	if (self->frozen > 0) {
		self->thaw_needs_regen = TRUE;
		return;
	}

	g_signal_handlers_block_by_func (self->folder_view, on_folder_view_rebuild_needed, self);

	camel_folder_view_set_threading (self->folder_view, self->threading);
	camel_folder_view_set_thread_subject (self->folder_view, self->thread_subject);
	camel_folder_view_set_thread_latest (self->folder_view, self->thread_latest);
	camel_folder_view_set_sort_children_ascending (self->folder_view, self->sort_children_ascending);
	camel_folder_view_set_group_by (self->folder_view, self->group_by);

	camel_folder_view_set_filter (self->folder_view, self->search_sexp);
	camel_folder_view_set_ensure_uid (self->folder_view, self->ensure_uid);
	message_list_push_show_flags (self, self->folder_view);

	camel_folder_view_set_sort (self->folder_view, self->sort_column, self->sort_order);

	g_signal_handlers_unblock_by_func (self->folder_view, on_folder_view_rebuild_needed, self);

	if (camel_folder_view_sort (self->folder_view)) {
		e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (self->model));

		update_empty_message (self);
		return;
	}

	g_mutex_lock (&self->regen_lock);

	if (self->regen_idle_source) {
		g_mutex_unlock (&self->regen_lock);
		return;
	}

	old_task = g_steal_pointer (&self->regen_task);
	old_changes_task = g_steal_pointer (&self->changes_task);
	self->changes_requeue = FALSE;

	g_mutex_unlock (&self->regen_lock);

	if (old_task) {
		g_cancellable_cancel (g_task_get_cancellable (old_task));
		g_clear_object (&old_task);
	}

	if (old_changes_task) {
		g_cancellable_cancel (g_task_get_cancellable (old_changes_task));
		g_clear_object (&old_changes_task);
	}

	cancellable = g_cancellable_new ();

	task = g_task_new (self, cancellable, message_list_regen_done_cb, NULL);
	g_task_set_task_data (task, g_object_ref (self->folder_view), g_object_unref);

	g_mutex_lock (&self->regen_lock);
	self->regen_idle_source = g_idle_source_new ();
	g_task_attach_source (task, self->regen_idle_source, message_list_regen_idle_cb);
	self->regen_task = task;
	self->regen_was_cancelled = FALSE;
	g_mutex_unlock (&self->regen_lock);

	g_object_unref (cancellable);
}

static gboolean
on_cell_clicked (EVirtualTree *vtree,
		 guint visible_row,
		 GObject *row_object,
		 guint col_idx,
		 GtkCellRenderer *hit_renderer,
		 gpointer user_data)
{
	EMessageList *self = user_data;
	EMessageListNode *node;
	CamelFolder *folder;
	CamelMessageInfo *info;
	const gchar *uid;
	gboolean folder_is_trash;
	guint32 flags;
	gint flag;

	if (col_idx != E_MESSAGE_LIST_COLUMN_STATUS &&
	    col_idx != E_MESSAGE_LIST_COLUMN_FLAGGED &&
	    col_idx != E_MESSAGE_LIST_COLUMN_FOLLOWUP_FLAG_STATUS)
		return FALSE;

	if (!E_IS_MESSAGE_LIST_NODE (row_object))
		return TRUE;

	node = E_MESSAGE_LIST_NODE (row_object);
	if (!node->row)
		return TRUE;

	folder = e_message_list_ref_folder (self);
	if (!folder)
		return TRUE;

	uid = camel_folder_view_row_get_uid (node->row);
	info = uid ? camel_folder_get_message_info (folder, uid) : NULL;

	if (!info) {
		g_object_unref (folder);
		return TRUE;
	}

	if (col_idx == E_MESSAGE_LIST_COLUMN_FOLLOWUP_FLAG_STATUS) {
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
			camel_message_info_set_user_tag (info, "follow-up", _("Follow-up"));
			camel_message_info_set_user_tag (info, "completed-on", NULL);
		}

		g_clear_object (&info);
		g_object_unref (folder);
		return TRUE;
	}

	flag = 0;
	if (col_idx == E_MESSAGE_LIST_COLUMN_STATUS)
		flag = CAMEL_MESSAGE_SEEN;
	else if (col_idx == E_MESSAGE_LIST_COLUMN_FLAGGED)
		flag = CAMEL_MESSAGE_FLAGGED;

	flags = camel_message_info_get_flags (info);
	folder_is_trash = (camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_TRASH) != 0;

	if (!folder_is_trash && (flags & CAMEL_MESSAGE_DELETED)) {
		if (col_idx == E_MESSAGE_LIST_COLUMN_FLAGGED && !(flags & CAMEL_MESSAGE_FLAGGED))
			flag |= CAMEL_MESSAGE_DELETED;

		if (col_idx == E_MESSAGE_LIST_COLUMN_STATUS && (flags & CAMEL_MESSAGE_SEEN))
			flag |= CAMEL_MESSAGE_DELETED;
	}

	camel_message_info_set_flags (info, flag, ~flags);

	if (col_idx == E_MESSAGE_LIST_COLUMN_STATUS && (flags & CAMEL_MESSAGE_SEEN)) {
		EMFolderTreeModel *model;

		model = em_folder_tree_model_get_default ();
		em_folder_tree_model_user_marked_unread (model, folder, 1);
	}

	if (flag == CAMEL_MESSAGE_SEEN && self->seen_id &&
	    g_strcmp0 (e_message_list_get_cursor_uid (self), uid) == 0) {
		g_source_remove (self->seen_id);
		self->seen_id = 0;
	}

	g_clear_object (&info);
	g_object_unref (folder);

	return TRUE;
}

static void
on_column_state_changed (EVirtualTree *vtree,
			  gpointer user_data)
{
	e_message_list_apply_sort_from_vtree (user_data);
}

static void
on_mail_settings_changed (GSettings *settings,
			  const gchar *key,
			  gpointer user_data)
{
	EMessageList *self = user_data;
	gboolean changed = FALSE;

	if (g_strcmp0 (key, "show-subject-above-sender") == 0) {
		gboolean val = g_settings_get_boolean (settings, key);
		if (self->show_subject_above_sender != val) {
			self->show_subject_above_sender = val;
			changed = TRUE;
		}
	} else if (g_strcmp0 (key, "show-email") == 0) {
		gboolean val = g_settings_get_boolean (settings, key);
		if (self->show_email != val) {
			self->show_email = val;
			changed = TRUE;
		}
	} else if (g_strcmp0 (key, "show-avatar-in-message-list") == 0) {
		gboolean val = g_settings_get_boolean (settings, key);
		if (self->show_avatar != val) {
			self->show_avatar = val;
			changed = TRUE;
		}
	} else if (g_strcmp0 (key, "show-body-preview-in-message-list") == 0) {
		gboolean val = g_settings_get_boolean (settings, key);
		if (self->show_body_preview != val) {
			self->show_body_preview = val;
			changed = TRUE;
		}
	} else if (g_strcmp0 (key, "thread-expand") == 0) {
		self->expanded_default = g_settings_get_boolean (settings, key);
	} else if (g_strcmp0 (key, "thread-latest") == 0) {
		gboolean val = g_settings_get_boolean (settings, key);
		if (self->thread_latest != val) {
			self->thread_latest = val;
			message_list_regen (self);
		}
	} else if (g_strcmp0 (key, "thread-children-ascending") == 0) {
		gboolean val = g_settings_get_boolean (settings, key);
		if (self->sort_children_ascending != val) {
			self->sort_children_ascending = val;
			message_list_regen (self);
		}
	} else if (g_strcmp0 (key, "show-deleted") == 0) {
		gboolean val = g_settings_get_boolean (settings, key);
		if (self->show_deleted != val) {
			self->show_deleted = val;
			message_list_regen (self);
		}
	} else if (g_strcmp0 (key, "show-junk") == 0) {
		gboolean val = g_settings_get_boolean (settings, key);
		if (self->show_junk != val) {
			self->show_junk = val;
			message_list_regen (self);
		}
	}

	if (changed && self->vtree)
		gtk_widget_queue_draw (GTK_WIDGET (self->vtree));
}

static void
update_user_header_titles (EMessageList *self)
{
	gchar **user_headers;
	guint ii, jj;

	user_headers = g_settings_get_strv (self->eds_settings, "camel-message-info-user-headers");

	for (ii = 0, jj = 0; user_headers && user_headers[ii] && jj < CAMEL_UTILS_MAX_USER_HEADERS; ii++) {
		const gchar *header_name = NULL;
		gchar *display_name = NULL;
		GtkTreeViewColumn *tvc;
		const gchar *title;

		camel_util_decode_user_header_setting (user_headers[ii], &display_name, &header_name);

		if (!header_name || !*header_name) {
			g_free (display_name);
			continue;
		}

		tvc = e_virtual_tree_get_column (self->vtree, E_MESSAGE_LIST_COLUMN_USER_HEADER_1 + jj);
		title = (display_name && *display_name) ? display_name : header_name;
		gtk_tree_view_column_set_title (tvc, title);

		g_free (display_name);
		jj++;
	}

	g_strfreev (user_headers);
}

static void
on_eds_settings_changed (GSettings *settings,
			 const gchar *key,
			 gpointer user_data)
{
	EMessageList *self = user_data;

	if (g_strcmp0 (key, "camel-message-info-user-headers") == 0)
		update_user_header_titles (self);
}

static void
composite_line_text_render (EMessageList *self,
			    GtkCellRenderer *renderer,
			    EMessageListNode *node,
			    gboolean show_subject,
			    const gchar * (* raw_getter) (CamelFolderViewRow *row),
			    const gchar * (* parsed_getter) (CamelFolderViewRow *row))
{
	if (!node->row) {
		g_object_set (renderer, "text", "", NULL);
		return;
	}

	camel_folder_view_row_lock (node->row);

	if (show_subject) {
		const gchar *subject = camel_folder_view_row_get_subject (node->row);
		g_object_set (renderer, "text", subject ? subject : "", NULL);
	} else {
		const gchar *text = self->show_email ? raw_getter (node->row) : parsed_getter (node->row);

		g_object_set (renderer, "text", text ? text : "", NULL);
	}

	camel_folder_view_row_unlock (node->row);

	apply_message_style (self, renderer, node->row);
}

static void
composite_line0_text_data_func (EVirtualTree *tree,
				GtkCellRenderer *renderer,
				GObject *row_object,
				guint visible_row,
				gpointer user_data)
{
	EMessageList *self = user_data;

	composite_line_text_render (self, renderer, E_MESSAGE_LIST_NODE (row_object),
		self->show_subject_above_sender, camel_folder_view_row_get_from, camel_folder_view_row_get_sender);
}

static void
composite_line0_text_to_data_func (EVirtualTree *tree,
				   GtkCellRenderer *renderer,
				   GObject *row_object,
				   guint visible_row,
				   gpointer user_data)
{
	EMessageList *self = user_data;

	composite_line_text_render (self, renderer, E_MESSAGE_LIST_NODE (row_object),
		self->show_subject_above_sender, camel_folder_view_row_get_to, camel_folder_view_row_get_recipients);
}

static void
composite_line0_date_data_func (EVirtualTree *tree,
				GtkCellRenderer *renderer,
				GObject *row_object,
				guint visible_row,
				gpointer user_data)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);

	if (node->row) {
		gchar *text = format_date (camel_folder_view_row_get_date_sent (node->row));
		g_object_set (renderer, "text", text, NULL);
		g_free (text);
		apply_message_style (get_message_list (tree), renderer, node->row);
	} else {
		g_object_set (renderer, "text", "", NULL);
	}
}

static void
composite_line1_text_data_func (EVirtualTree *tree,
				GtkCellRenderer *renderer,
				GObject *row_object,
				guint visible_row,
				gpointer user_data)
{
	EMessageList *self = user_data;

	composite_line_text_render (self, renderer, E_MESSAGE_LIST_NODE (row_object),
		!self->show_subject_above_sender, camel_folder_view_row_get_from, camel_folder_view_row_get_sender);
}

static void
composite_line1_text_to_data_func (EVirtualTree *tree,
				   GtkCellRenderer *renderer,
				   GObject *row_object,
				   guint visible_row,
				   gpointer user_data)
{
	EMessageList *self = user_data;

	composite_line_text_render (self, renderer, E_MESSAGE_LIST_NODE (row_object),
		!self->show_subject_above_sender, camel_folder_view_row_get_to, camel_folder_view_row_get_recipients);
}

static gchar *
extract_display_name (const gchar *sender)
{
	gchar *name;
	gchar *cut;
	gsize len;

	name = camel_header_decode_string (sender, "UTF-8");
	if (!name)
		name = g_strdup (sender);

	g_strstrip (name);

	if (name[0] == '"') {
		gchar *closing = strchr (name + 1, '"');

		if (closing) {
			memmove (name, name + 1, closing - (name + 1));
			name[closing - (name + 1)] = '\0';
		}
	} else {
		cut = strchr (name, '<');
		if (cut)
			*cut = '\0';
	}

	g_strstrip (name);

	/* e_contact_name_from_string() would otherwise treat a trailing
	 * "(comment)" as the family name. */
	len = strlen (name);
	if (len > 0 && name[len - 1] == ')') {
		gchar *open = strrchr (name, '(');

		if (open) {
			*open = '\0';
			g_strstrip (name);
		}
	}

	if (!*name) {
		g_free (name);
		name = g_strdup (sender);
	}

	return name;
}

static void
compute_avatar_initials (const gchar *sender,
			 gchar initials[7])
{
	gchar *name;
	EContactName *contact_name;
	gunichar first_char = 0, last_char = 0;
	gint written;

	if (!sender || !*sender)
		sender = "?";

	name = extract_display_name (sender);
	contact_name = e_contact_name_from_string (name);
	g_free (name);

	if (contact_name) {
		if (contact_name->given && *contact_name->given)
			first_char = g_utf8_get_char (contact_name->given);

		if (contact_name->family && *contact_name->family)
			last_char = g_utf8_get_char (contact_name->family);

		e_contact_name_free (contact_name);
	}

	if (!first_char) {
		first_char = last_char;
		last_char = 0;
	}

	if (!first_char)
		first_char = '?';

	written = 0;
	written += g_unichar_to_utf8 (g_unichar_toupper (first_char), initials + written);
	if (last_char && last_char != first_char)
		written += g_unichar_to_utf8 (g_unichar_toupper (last_char), initials + written);
	initials[written] = '\0';
}

gchar *
_e_message_list_compute_avatar_initials (const gchar *sender)
{
	gchar initials[7];

	compute_avatar_initials (sender, initials);

	return g_strdup (initials);
}

static GdkPixbuf *
create_avatar_pixbuf (const gchar *sender, gint size)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	PangoLayout *layout;
	PangoFontDescription *font_desc;
	PangoRectangle ink_rect, logical_rect;
	GdkPixbuf *pixbuf;
	cairo_pattern_t *gradient;
	GdkRGBA bg_color, text_color;
	guint hash;
	gdouble hue, sat, light, r, g, b, r2, g2, b2;
	gchar initials[7];

	if (!sender || !*sender)
		sender = "?";

	compute_avatar_initials (sender, initials);

	hash = g_str_hash (sender);
	hue = (hash % 360) / 360.0;
	sat = 0.65;
	light = 0.35;

	gtk_hsv_to_rgb (hue, sat, CLAMP (1.0 - light + 0.18, 0.0, 1.0), &r, &g, &b);
	gtk_hsv_to_rgb (hue, sat, CLAMP (1.0 - light - 0.18, 0.0, 1.0), &r2, &g2, &b2);
	bg_color.red = (r + r2) / 2.0;
	bg_color.green = (g + g2) / 2.0;
	bg_color.blue = (b + b2) / 2.0;
	bg_color.alpha = 1.0;

	text_color = e_utils_get_text_color_for_background (&bg_color);

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
	cr = cairo_create (surface);

	cairo_arc (cr, size / 2.0, size / 2.0, size / 2.0 - 1.0, 0, 2 * G_PI);
	gradient = cairo_pattern_create_linear (0, size, size, 0);
	cairo_pattern_add_color_stop_rgb (gradient, 0.0, r, g, b);
	cairo_pattern_add_color_stop_rgb (gradient, 1.0, r2, g2, b2);
	cairo_set_source (cr, gradient);
	cairo_fill (cr);
	cairo_pattern_destroy (gradient);

	layout = pango_cairo_create_layout (cr);
	font_desc = pango_font_description_new ();
	pango_font_description_set_family (font_desc, "Sans");
	pango_font_description_set_weight (font_desc, PANGO_WEIGHT_BOLD);
	pango_font_description_set_absolute_size (font_desc, size * 0.40 * PANGO_SCALE);
	pango_layout_set_font_description (layout, font_desc);
	pango_layout_set_text (layout, initials, -1);
	pango_layout_get_pixel_extents (layout, &ink_rect, &logical_rect);

	gdk_cairo_set_source_rgba (cr, &text_color);
	cairo_move_to (cr, (size - logical_rect.width) / 2.0 - logical_rect.x, (size - logical_rect.height) / 2.0 - logical_rect.y);
	pango_cairo_show_layout (cr, layout);

	pango_font_description_free (font_desc);
	g_object_unref (layout);
	cairo_destroy (cr);

	pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
	cairo_surface_destroy (surface);

	return pixbuf;
}

static GdkPixbuf *
avatar_pixbuf_mask_circle (GdkPixbuf *pixbuf)
{
	gint width = gdk_pixbuf_get_width (pixbuf);
	gint height = gdk_pixbuf_get_height (pixbuf);
	gint size = MIN (width, height);
	cairo_surface_t *surface;
	cairo_t *cr;
	GdkPixbuf *masked;

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
	cr = cairo_create (surface);

	cairo_arc (cr, size / 2.0, size / 2.0, size / 2.0, 0, 2 * G_PI);
	cairo_clip (cr);

	gdk_cairo_set_source_pixbuf (cr, pixbuf, (size - width) / 2.0, (size - height) / 2.0);
	cairo_paint (cr);

	cairo_destroy (cr);

	masked = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
	cairo_surface_destroy (surface);

	return masked;
}

#define AVATAR_SIZE 32

static gint
avatar_compute_size (EMessageList *self,
		     gint scale_factor)
{
	return (gint) (AVATAR_SIZE * scale_factor * self->avatar_font_scale + 0.5);
}

static gchar *
extract_bare_email (const gchar *address_text)
{
	CamelInternetAddress *cia;
	const gchar *found_email = NULL;
	gchar *email = NULL;

	if (!address_text || !*address_text)
		return NULL;

	cia = camel_internet_address_new ();

	if (camel_address_decode (CAMEL_ADDRESS (cia), address_text) > 0 &&
	    camel_internet_address_get (cia, 0, NULL, &found_email) && found_email && *found_email)
		email = g_strdup (found_email);

	g_object_unref (cia);

	return email;
}

typedef struct {
	GdkPixbuf *pixbuf;
	gint avatar_size;
	gint64 last_used;
} AvatarPixbufCacheEntry;

static AvatarPixbufCacheEntry *
avatar_pixbuf_cache_entry_new (GdkPixbuf *pixbuf,
			       gint avatar_size)
{
	AvatarPixbufCacheEntry *entry = g_new0 (AvatarPixbufCacheEntry, 1);

	entry->pixbuf = g_object_ref (pixbuf);
	entry->avatar_size = avatar_size;
	entry->last_used = g_get_monotonic_time ();

	return entry;
}

static void
avatar_pixbuf_cache_entry_free (gpointer data)
{
	AvatarPixbufCacheEntry *entry = data;

	g_object_unref (entry->pixbuf);
	g_free (entry);
}

static void
avatar_pixbuf_cache_insert (EMessageList *self,
			    const gchar *key,
			    GdkPixbuf *pixbuf,
			    gint avatar_size)
{
	if (!self->avatar_pixbuf_cache)
		return;

	g_hash_table_insert (self->avatar_pixbuf_cache, g_strdup (key),
		avatar_pixbuf_cache_entry_new (pixbuf, avatar_size));
}

gboolean
_e_message_list_avatar_pixbuf_cache_contains_key (EMessageList *self,
						  const gchar *key)
{
	return g_hash_table_contains (self->avatar_pixbuf_cache, key);
}

#define AVATAR_PIXBUF_CACHE_TTL_SECONDS 60
#define AVATAR_PIXBUF_CACHE_SWEEP_INTERVAL_SECONDS 30

static gboolean
avatar_pixbuf_cache_sweep_cb (gpointer user_data)
{
	EMessageList *self = user_data;
	GHashTableIter iter;
	gpointer key, value;
	gint64 now = g_get_monotonic_time ();
	gint64 ttl_usec = ((gint64) AVATAR_PIXBUF_CACHE_TTL_SECONDS) * G_USEC_PER_SEC;

	g_hash_table_iter_init (&iter, self->avatar_pixbuf_cache);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		AvatarPixbufCacheEntry *entry = value;

		if (now - entry->last_used > ttl_usec)
			g_hash_table_iter_remove (&iter);
	}

	g_hash_table_iter_init (&iter, self->avatar_photo_miss_emails);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		gint64 *recorded_at = value;

		if (now - *recorded_at > ttl_usec)
			g_hash_table_iter_remove (&iter);
	}

	return G_SOURCE_CONTINUE;
}

/* EPhotoCache does not always cache a "no photo" result, so track misses
 * ourselves to avoid rescheduling a fetch on every redraw. */
static void
avatar_photo_miss_record (EMessageList *self,
			  const gchar *email)
{
	gint64 *recorded_at;

	if (!self->avatar_photo_miss_emails)
		return;

	recorded_at = g_new (gint64, 1);
	*recorded_at = g_get_monotonic_time ();
	g_hash_table_insert (self->avatar_photo_miss_emails, g_strdup (email), recorded_at);
}

static gboolean
avatar_photo_miss_is_recent (EMessageList *self,
			     const gchar *email)
{
	gint64 *recorded_at = g_hash_table_lookup (self->avatar_photo_miss_emails, email);
	gint64 ttl_usec = ((gint64) AVATAR_PIXBUF_CACHE_TTL_SECONDS) * G_USEC_PER_SEC;

	return recorded_at && (g_get_monotonic_time () - *recorded_at) < ttl_usec;
}

void
_e_message_list_avatar_photo_miss_record (EMessageList *self,
					  const gchar *email)
{
	avatar_photo_miss_record (self, email);
}

gboolean
_e_message_list_avatar_photo_miss_is_recent (EMessageList *self,
					     const gchar *email)
{
	return avatar_photo_miss_is_recent (self, email);
}

typedef struct {
	EMessageList *list;    /* owned */
	gchar *email;          /* owned */
	GInputStream *stream;  /* owned; set only while a decode is in flight,
				 * see avatar_fetch_done_cb() below */
} AvatarFetchClosure;

static AvatarFetchClosure *
avatar_fetch_closure_new (EMessageList *self,
			  gchar *email) /* (transfer full) */
{
	AvatarFetchClosure *closure = g_new0 (AvatarFetchClosure, 1);

	closure->list = g_object_ref (self);
	closure->email = g_steal_pointer (&email);

	return closure;
}

static void
avatar_fetch_closure_free (AvatarFetchClosure *closure)
{
	g_clear_object (&closure->list);
	g_clear_object (&closure->stream);
	g_free (closure->email);
	g_free (closure);
}

static void
avatar_fetch_finish_and_redraw (AvatarFetchClosure *closure)
{
	EMessageList *self = closure->list;
	GPtrArray *pending;

	if (self->avatar_fetch_pending) {
		pending = g_hash_table_lookup (self->avatar_fetch_pending, closure->email);

		if (pending) {
			EVirtualTreeModel *model = E_VIRTUAL_TREE_MODEL (self->model);
			guint ii;

			for (ii = 0; ii < pending->len; ii++) {
				const gchar *uid = g_ptr_array_index (pending, ii);
				guint row_idx = self->folder_view ? camel_folder_view_find_row_by_uid (self->folder_view, uid) : G_MAXUINT;

				if (row_idx != G_MAXUINT)
					e_virtual_tree_model_emit_rows_changed (model, row_idx, row_idx);
			}

			g_hash_table_remove (self->avatar_fetch_pending, closure->email);
		}
	}

	avatar_fetch_closure_free (closure);
}

static void
avatar_fetch_stream_decoded_cb (GObject *source_object,
				GAsyncResult *result,
				gpointer user_data)
{
	AvatarFetchClosure *closure = user_data;
	GdkPixbuf *decoded;
	GdkPixbuf *pixbuf = NULL;

	decoded = gdk_pixbuf_new_from_stream_finish (result, NULL);

	if (decoded) {
		pixbuf = avatar_pixbuf_mask_circle (decoded);
		g_object_unref (decoded);

		avatar_pixbuf_cache_insert (closure->list, closure->email, pixbuf,
			avatar_compute_size (closure->list, gtk_widget_get_scale_factor (GTK_WIDGET (closure->list))));
	} else {
		avatar_photo_miss_record (closure->list, closure->email);
	}

	g_clear_object (&pixbuf);
	g_clear_object (&closure->stream);

	avatar_fetch_finish_and_redraw (closure);
}

static void
avatar_fetch_done_cb (GObject *source_object,
		      GAsyncResult *result,
		      gpointer user_data)
{
	AvatarFetchClosure *closure = user_data;
	GInputStream *stream = NULL;

	e_photo_cache_get_photo_finish (E_PHOTO_CACHE (source_object), result, &stream, NULL);

	if (stream) {
		gint avatar_size = avatar_compute_size (closure->list, gtk_widget_get_scale_factor (GTK_WIDGET (closure->list)));

		closure->stream = g_steal_pointer (&stream);
		gdk_pixbuf_new_from_stream_at_scale_async (closure->stream, avatar_size, avatar_size, TRUE,
			closure->list->avatar_fetch_cancellable, avatar_fetch_stream_decoded_cb, closure);
	} else {
		avatar_photo_miss_record (closure->list, closure->email);
		avatar_fetch_finish_and_redraw (closure);
	}
}

static void
schedule_avatar_fetch (EMessageList *self,
		       EPhotoCache *photo_cache,
		       gchar *email, /* (transfer full) */
		       const gchar *uid)
{
	GPtrArray *pending;
	AvatarFetchClosure *closure;

	if (!self->avatar_fetch_pending) {
		g_free (email);
		return;
	}

	pending = g_hash_table_lookup (self->avatar_fetch_pending, email);

	if (pending) {
		if (!g_ptr_array_find (pending, uid, NULL))
			g_ptr_array_add (pending, (gpointer) camel_pstring_strdup (uid));
		g_free (email);
		return;
	}

	pending = g_ptr_array_new_with_free_func ((GDestroyNotify) camel_pstring_free);
	g_ptr_array_add (pending, (gpointer) camel_pstring_strdup (uid));
	g_hash_table_insert (self->avatar_fetch_pending, g_strdup (email), pending);

	closure = avatar_fetch_closure_new (self, g_steal_pointer (&email));

	e_photo_cache_get_photo (photo_cache, closure->email, self->avatar_fetch_cancellable,
		avatar_fetch_done_cb, closure);
}

static void
composite_avatar_render (EMessageList *self,
			 EVirtualTree *tree,
			 GtkCellRenderer *renderer,
			 EMessageListNode *node,
			 const gchar * (* get_address) (CamelFolderViewRow *row))
{
	EPhotoCache *photo_cache = NULL;
	const gchar *sender;
	gchar *email;
	const gchar *photo_cache_key;
	gint scale_factor;
	gint avatar_size;
	GdkPixbuf *pixbuf = NULL;
	GInputStream *stream = NULL;
	cairo_surface_t *surface;

	if (!self->show_avatar || !node->row) {
		g_object_set (renderer, "visible", FALSE, "surface", NULL, NULL);
		return;
	}

	if (E_IS_MAIL_UI_SESSION (self->session))
		photo_cache = e_mail_ui_session_get_photo_cache (E_MAIL_UI_SESSION (self->session));

	scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (tree));
	avatar_size = avatar_compute_size (self, scale_factor);

	camel_folder_view_row_lock (node->row);

	sender = get_address (node->row);

	/* Photos are keyed by email; generated initials by the full address,
	 * so a shared address with different names does not share initials. */
	email = extract_bare_email (sender);
	photo_cache_key = (email && *email) ? email : sender;

	if (photo_cache_key && *photo_cache_key) {
		AvatarPixbufCacheEntry *entry = g_hash_table_lookup (self->avatar_pixbuf_cache, photo_cache_key);

		if (entry && entry->avatar_size == avatar_size) {
			entry->last_used = g_get_monotonic_time ();
			pixbuf = g_object_ref (entry->pixbuf);
		} else if (photo_cache && email && *email) {
			if (e_photo_cache_peek_cached (photo_cache, email, &stream)) {
				if (stream) {
					GdkPixbuf *decoded = gdk_pixbuf_new_from_stream_at_scale (stream, avatar_size, avatar_size, TRUE, NULL, NULL);

					if (decoded) {
						pixbuf = avatar_pixbuf_mask_circle (decoded);
						g_object_unref (decoded);
						avatar_pixbuf_cache_insert (self, photo_cache_key, pixbuf, avatar_size);
					}
				}
			} else if (!avatar_photo_miss_is_recent (self, email)) {
				schedule_avatar_fetch (self, photo_cache, g_steal_pointer (&email), camel_folder_view_row_get_uid (node->row));
			}
		}
	}

	if (!pixbuf && sender && *sender) {
		AvatarPixbufCacheEntry *entry = g_hash_table_lookup (self->avatar_pixbuf_cache, sender);

		if (entry && entry->avatar_size == avatar_size) {
			entry->last_used = g_get_monotonic_time ();
			pixbuf = g_object_ref (entry->pixbuf);
		} else {
			pixbuf = create_avatar_pixbuf (sender, avatar_size);
			avatar_pixbuf_cache_insert (self, sender, pixbuf, avatar_size);
		}
	}

	if (!pixbuf)
		pixbuf = create_avatar_pixbuf (sender, avatar_size);

	camel_folder_view_row_unlock (node->row);

	g_clear_object (&stream);
	g_free (email);

	surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
	g_object_set (renderer, "visible", TRUE, "surface", surface, NULL);
	cairo_surface_destroy (surface);
	g_object_unref (pixbuf);
}

static void
composite_avatar_data_func (EVirtualTree *tree,
			    GtkCellRenderer *renderer,
			    GObject *row_object,
			    guint visible_row,
			    gpointer user_data)
{
	composite_avatar_render (user_data, tree, renderer, E_MESSAGE_LIST_NODE (row_object), camel_folder_view_row_get_from);
}

static void
composite_avatar_to_data_func (EVirtualTree *tree,
			       GtkCellRenderer *renderer,
			       GObject *row_object,
			       guint visible_row,
			       gpointer user_data)
{
	composite_avatar_render (user_data, tree, renderer, E_MESSAGE_LIST_NODE (row_object), camel_folder_view_row_get_to);
}

static void
composite_line2_preview_data_func (EVirtualTree *tree,
				   GtkCellRenderer *renderer,
				   GObject *row_object,
				   guint visible_row,
				   gpointer user_data)
{
	EMessageList *self = user_data;
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_object);
	const gchar *preview = NULL;
	gboolean show_line = self->show_body_preview && node->row;

	if (show_line) {
		gboolean has_preview;

		camel_folder_view_row_lock (node->row);
		preview = camel_folder_view_row_get_preview (node->row);
		has_preview = preview && *preview;
		g_object_set (renderer,
			"visible", TRUE,
			"text", has_preview ? preview : _("(Preview not available)"),
			"style", has_preview ? PANGO_STYLE_NORMAL : PANGO_STYLE_ITALIC,
			"scale", 0.8,
			NULL);
		g_object_set (renderer, "foreground-rgba", &(GdkRGBA){ 0.5, 0.5, 0.5, 1.0 }, NULL);
		camel_folder_view_row_unlock (node->row);
	} else {
		g_object_set (renderer, "visible", FALSE, "text", "", NULL);
	}
}

static void
set_column_header_icon (EVirtualTree *vtree,
			guint col_idx,
			const gchar *icon_name,
			const gchar *tooltip)
{
	GtkTreeViewColumn *tvc;
	GtkWidget *image;

	tvc = e_virtual_tree_get_column (vtree, col_idx);
	g_return_if_fail (tvc != NULL);

	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gtk_tree_view_column_set_widget (tvc, image);

	if (tooltip) {
		GtkWidget *button;

		button = gtk_tree_view_column_get_button (tvc);
		if (button)
			gtk_widget_set_tooltip_text (button, tooltip);
	}
}

static void
setup_columns (EMessageList *self)
{
	static const gchar *vanilla_column_state =
		"[Column-status]\n"
		"order=0\n"
		"visible=true\n"
		"\n"
		"[Column-attachment]\n"
		"order=1\n"
		"visible=true\n"
		"\n"
		"[Column-flagged]\n"
		"order=2\n"
		"visible=true\n"
		"\n"
		"[Column-from]\n"
		"order=3\n"
		"visible=true\n"
		"width=1000\n"
		"\n"
		"[Column-subject]\n"
		"order=4\n"
		"visible=true\n"
		"width=1600\n"
		"\n"
		"[Column-date-sent]\n"
		"order=5\n"
		"visible=true\n"
		"width=400\n";
	GtkCellRenderer *renderer;
	GKeyFile *vanilla_key_file;
	guint col;

	col = e_virtual_tree_add_column (self->vtree, "status", "");
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, "xpad", 2, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, status_icon_data_func, NULL, NULL);
	e_virtual_tree_column_set_accessible_name (self->vtree, col, _("Status"));
	set_column_header_icon (self->vtree, col, "mail-unread", _("Status"));
	gtk_tree_view_column_set_alignment (e_virtual_tree_get_column (self->vtree, col), 0.5);
	gtk_tree_view_column_set_resizable (e_virtual_tree_get_column (self->vtree, col), FALSE);
	gtk_tree_view_column_set_reorderable (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "attachment", "");
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, "xpad", 2, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, attachment_icon_data_func, NULL, NULL);
	e_virtual_tree_column_set_accessible_name (self->vtree, col, _("Attachment"));
	set_column_header_icon (self->vtree, col, "mail-attachment", _("Attachment"));
	gtk_tree_view_column_set_alignment (e_virtual_tree_get_column (self->vtree, col), 0.5);
	gtk_tree_view_column_set_resizable (e_virtual_tree_get_column (self->vtree, col), FALSE);
	gtk_tree_view_column_set_reorderable (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "flagged", "");
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, "xpad", 2, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, flagged_icon_data_func, NULL, NULL);
	e_virtual_tree_column_set_accessible_name (self->vtree, col, _("Flagged"));
	set_column_header_icon (self->vtree, col, "emblem-important", _("Flagged"));
	gtk_tree_view_column_set_alignment (e_virtual_tree_get_column (self->vtree, col), 0.5);
	gtk_tree_view_column_set_resizable (e_virtual_tree_get_column (self->vtree, col), FALSE);
	gtk_tree_view_column_set_reorderable (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "from", _("From"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, from_data_func, NULL, NULL);

	col = e_virtual_tree_add_column (self->vtree, "subject", _("Subject"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, subject_data_func, NULL, NULL);
	e_virtual_tree_set_expander_column (self->vtree, col, 0);
	gtk_tree_view_column_set_expand (e_virtual_tree_get_column (self->vtree, col), TRUE);

	col = e_virtual_tree_add_column (self->vtree, "date-sent", _("Date"));
	e_virtual_tree_set_column_groupable (self->vtree, col, TRUE);
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, FALSE, date_data_func, NULL, NULL);

	col = e_virtual_tree_add_column (self->vtree, "size", _("Size"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xalign", 1.0f, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, FALSE, size_data_func, NULL, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "to", _("To"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_to, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "cc", _("CC"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_cc, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "date-received", _("Received"));
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, FALSE, date_received_data_func, NULL, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "sender", _("Sender"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_sender, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "sender-mail", _("Sender EMail"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_sender_mail, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "recipients", _("Recipients"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_recipients, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "recipients-mail", _("Recipients EMail"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_recipients_mail, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "correspondents", _("Correspondents"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_correspondents, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "subject-trimmed", _("Subject Trimmed"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_subject_trimmed, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "labels", _("Labels"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_labels, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "mailing-list", _("Mailing List"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_mlist, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "followup-flag-status", "");
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, "xpad", 2, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, followup_flag_status_data_func, NULL, NULL);
	e_virtual_tree_column_set_accessible_name (self->vtree, col, _("Flag Status"));
	set_column_header_icon (self->vtree, col, "stock_mail-flag-for-followup", _("Flag Status"));
	gtk_tree_view_column_set_alignment (e_virtual_tree_get_column (self->vtree, col), 0.5);
	gtk_tree_view_column_set_resizable (e_virtual_tree_get_column (self->vtree, col), FALSE);
	gtk_tree_view_column_set_reorderable (e_virtual_tree_get_column (self->vtree, col), FALSE);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "followup-flag", _("Follow Up Flag"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_followup_flag, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "followup-due-by", _("Follow Up Due By"));
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, FALSE, followup_due_by_data_func, NULL, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "score", "");
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, "xpad", 2, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, score_data_func, NULL, NULL);
	e_virtual_tree_column_set_accessible_name (self->vtree, col, _("Score"));
	set_column_header_icon (self->vtree, col, "stock_score-higher", _("Score"));
	gtk_tree_view_column_set_alignment (e_virtual_tree_get_column (self->vtree, col), 0.5);
	gtk_tree_view_column_set_resizable (e_virtual_tree_get_column (self->vtree, col), FALSE);
	gtk_tree_view_column_set_reorderable (e_virtual_tree_get_column (self->vtree, col), FALSE);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "location", _("Location"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_location, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "preview", _("Body Preview"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_preview, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "user-header-1", _("User Header 1"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, user_header_data_func, GUINT_TO_POINTER (0), NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "user-header-2", _("User Header 2"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, user_header_data_func, GUINT_TO_POINTER (1), NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "user-header-3", _("User Header 3"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, user_header_data_func, GUINT_TO_POINTER (2), NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "uid", _("UID"));
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, text_column_data_func, (gpointer) camel_folder_view_row_get_uid, NULL);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "composite", _("Messages"));

	/* Span: Avatar (spans all lines) */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "xpad", 2, "ypad", 4, "yalign", 0.0f, NULL);
	e_virtual_tree_column_pack_span (self->vtree, col,
		renderer, composite_avatar_data_func, self, NULL);

	/* Line 0: Subject or From (expanding), Attachment icon, Date (right-aligned) */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, composite_line0_text_data_func, self, NULL);
	e_virtual_tree_set_expander_column (self->vtree, col, 0);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, FALSE, attachment_icon_data_func, NULL, NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xalign", 1.0f, "xpad", 8, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, FALSE, composite_line0_date_data_func, NULL, NULL);

	/* Line 1: From or Subject (same size as line 0) */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 1,
		renderer, TRUE, composite_line1_text_data_func, self, NULL);

	/* Line 2: Body preview (dimmed, smaller) */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"scale", 0.8,
		NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 2,
		renderer, TRUE, composite_line2_preview_data_func, self, NULL);

	gtk_tree_view_column_set_expand (e_virtual_tree_get_column (self->vtree, col), TRUE);
	gtk_tree_view_column_set_sizing (e_virtual_tree_get_column (self->vtree, col), GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	col = e_virtual_tree_add_column (self->vtree, "composite-to", _("Messages To"));

	/* Span: Avatar (spans all lines) */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "xpad", 2, "ypad", 4, "yalign", 0.0f, NULL);
	e_virtual_tree_column_pack_span (self->vtree, col,
		renderer, composite_avatar_to_data_func, self, NULL);

	/* Line 0: Subject or To (expanding), Attachment icon, Date (right-aligned) */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, TRUE, composite_line0_text_to_data_func, self, NULL);
	e_virtual_tree_set_expander_column (self->vtree, col, 0);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, FALSE, attachment_icon_data_func, NULL, NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xalign", 1.0f, "xpad", 8, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 0,
		renderer, FALSE, composite_line0_date_data_func, NULL, NULL);

	/* Line 1: To or Subject (same size as line 0) */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 1,
		renderer, TRUE, composite_line1_text_to_data_func, self, NULL);

	/* Line 2: Body preview (dimmed, smaller) */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"scale", 0.8,
		NULL);
	e_virtual_tree_column_pack_start (self->vtree, col, 2,
		renderer, TRUE, composite_line2_preview_data_func, self, NULL);

	gtk_tree_view_column_set_expand (e_virtual_tree_get_column (self->vtree, col), TRUE);
	gtk_tree_view_column_set_sizing (e_virtual_tree_get_column (self->vtree, col), GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_visible (e_virtual_tree_get_column (self->vtree, col), FALSE);

	e_virtual_tree_set_selected_row_color_func (self->vtree, message_list_selected_row_color_func, self, NULL);

	vanilla_key_file = g_key_file_new ();
	if (g_key_file_load_from_data (vanilla_key_file, vanilla_column_state, -1, G_KEY_FILE_NONE, NULL))
		e_virtual_tree_load_column_state_from_key_file (self->vtree, vanilla_key_file);
	g_key_file_unref (vanilla_key_file);
}

static gboolean
update_actions_idle_cb (gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);

	self->update_actions_idle_id = 0;
	g_signal_emit (self, signals[UPDATE_ACTIONS], 0);

	return G_SOURCE_REMOVE;
}

static void
schedule_update_actions (EMessageList *self)
{
	if (!self->update_actions_idle_id) {
		self->update_actions_idle_id = g_idle_add_full (
			G_PRIORITY_DEFAULT_IDLE, update_actions_idle_cb, self, NULL);
	}
}

static gboolean
on_message_selected_idle (gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);
	const gchar *uid = NULL;

	self->idle_id = 0;

	if (self->last_sel_single)
		uid = (const gchar *) e_virtual_tree_get_cursor_key (self->vtree);

	g_signal_emit (self, signals[MESSAGE_SELECTED], 0, uid);
	schedule_update_actions (self);

	return G_SOURCE_REMOVE;
}

static void
schedule_message_selected (EMessageList *self)
{
	if (!self->idle_id)
		self->idle_id = g_idle_add (on_message_selected_idle, self);
}

static void
on_vtree_cursor_changed (EVirtualTree *vtree,
			 guint row,
			 GObject *row_object,
			 gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);

	schedule_message_selected (self);
}

static void
on_vtree_selection_changed (EVirtualTree *vtree,
			    gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);

	self->last_sel_single = e_virtual_tree_selected_count (self->vtree) == 1;

	schedule_message_selected (self);
}

static void
on_vtree_drag_data_get (EVirtualTree *vtree,
			GdkDragContext *context,
			GtkSelectionData *data,
			guint info,
			guint time,
			gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_message_list_ref_folder (self);
	if (!folder)
		return;

	uids = e_message_list_get_selected_with_collapsed_threads (self);

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

	g_object_unref (folder);
	g_ptr_array_unref (uids);
}

struct _drop_msg {
	MailMsg base;
	GdkDragContext *context;
	GtkSelectionData *selection;
	CamelFolder *folder;
	EMailSession *session;
	guint32 action;
	guint info;
	guint move : 1;
};

static gchar *
ml_drop_async_desc (struct _drop_msg *m)
{
	const gchar *full_name;

	full_name = camel_folder_get_full_name (m->folder);

	if (m->move)
		return g_strdup_printf (_("Moving messages into folder %s"), full_name);

	return g_strdup_printf (_("Copying messages into folder %s"), full_name);
}

static void
ml_drop_async_exec (struct _drop_msg *m,
		    GCancellable *cancellable,
		    GError **error)
{
	switch (m->info) {
	case DND_X_UID_LIST:
		em_utils_selection_get_uidlist (
			m->selection, m->session, m->folder,
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
	gboolean success;

	success = (m->base.error == NULL);
	gtk_drag_finish (m->context, success, success && m->move, GDK_CURRENT_TIME);
}

static void
ml_drop_async_free (struct _drop_msg *m)
{
	g_object_unref (m->context);
	g_object_unref (m->folder);
	g_clear_object (&m->session);
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
on_vtree_drag_data_received (EVirtualTree *vtree,
			     GdkDragContext *context,
			     guint row,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint time,
			     gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);
	CamelFolder *folder;
	struct _drop_msg *m;

	if (!gtk_selection_data_get_data (selection_data))
		return;

	if (gtk_selection_data_get_length (selection_data) == -1)
		return;

	folder = e_message_list_ref_folder (self);
	if (!folder)
		return;

	m = mail_msg_new (&ml_drop_async_info);
	m->context = g_object_ref (context);
	m->folder = g_object_ref (folder);
	m->session = self->session ? g_object_ref (self->session) : NULL;
	m->action = gdk_drag_context_get_selected_action (context);
	m->info = info;
	m->move = m->action == GDK_ACTION_MOVE;
	m->selection = gtk_selection_data_copy (selection_data);

	mail_msg_unordered_push (m);

	g_object_unref (folder);
}

static void
ml_clipboard_clear (EMessageList *self)
{
	g_clear_pointer (&self->clipboard_uids, g_ptr_array_unref);
	g_clear_object (&self->clipboard_folder);
}

static void
on_invisible_selection_get (GtkWidget *widget,
			    GtkSelectionData *data,
			    guint info,
			    guint time_stamp,
			    gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);

	if (!self->clipboard_uids || !self->clipboard_folder)
		return;

	em_utils_selection_set_uidlist (data, self->clipboard_folder, self->clipboard_uids);
}

static gboolean
on_invisible_selection_clear_event (GtkWidget *widget,
				    GdkEventSelection *event,
				    gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);

	ml_clipboard_clear (self);

	return TRUE;
}

static void
on_invisible_selection_received (GtkWidget *widget,
				 GtkSelectionData *selection_data,
				 guint time,
				 gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);
	CamelFolder *folder;
	GdkAtom target;

	target = gtk_selection_data_get_target (selection_data);

	if (target != gdk_atom_intern ("x-uid-list", FALSE))
		return;

	if (!self->session)
		return;

	folder = e_message_list_ref_folder (self);
	if (!folder)
		return;

	em_utils_selection_get_uidlist (
		selection_data, self->session, folder, FALSE, NULL, NULL);

	g_object_unref (folder);
}

static void
e_message_list_selectable_update_actions (ESelectable *selectable,
					  EFocusTracker *focus_tracker,
					  GdkAtom *clipboard_targets,
					  gint n_clipboard_targets)
{
	EMessageList *self = E_MESSAGE_LIST (selectable);
	EUIAction *action;
	guint count;

	count = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (self->model));

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	e_ui_action_set_tooltip (action, _("Select all visible messages"));
	e_ui_action_set_sensitive (action, count > 0);
}

static void
e_message_list_selectable_select_all (ESelectable *selectable)
{
	EMessageList *self = E_MESSAGE_LIST (selectable);

	e_virtual_tree_select_all (self->vtree);
}

static void
e_message_list_selectable_copy_clipboard (ESelectable *selectable)
{
	EMessageList *self = E_MESSAGE_LIST (selectable);
	GPtrArray *uids;

	uids = e_message_list_get_selected_with_collapsed_threads (self);

	ml_clipboard_clear (self);

	if (uids->len > 0) {
		self->clipboard_uids = g_ptr_array_ref (uids);
		self->clipboard_folder = e_message_list_ref_folder (self);

		gtk_selection_owner_set (
			self->invisible,
			GDK_SELECTION_CLIPBOARD,
			gtk_get_current_event_time ());
	} else {
		gtk_selection_owner_set (
			NULL, GDK_SELECTION_CLIPBOARD,
			gtk_get_current_event_time ());
	}

	g_ptr_array_unref (uids);
}

static void
e_message_list_selectable_cut_clipboard (ESelectable *selectable)
{
	EMessageList *self = E_MESSAGE_LIST (selectable);
	CamelFolder *folder;
	GPtrArray *uids;
	guint ii;

	e_message_list_selectable_copy_clipboard (selectable);

	folder = e_message_list_ref_folder (self);
	if (!folder)
		return;

	uids = e_message_list_get_selected_with_collapsed_threads (self);

	for (ii = 0; ii < uids->len; ii++) {
		camel_folder_set_message_flags (
			folder, g_ptr_array_index (uids, ii),
			CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED,
			CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED);
	}

	g_ptr_array_unref (uids);
	g_object_unref (folder);
}

static void
e_message_list_selectable_paste_clipboard (ESelectable *selectable)
{
	EMessageList *self = E_MESSAGE_LIST (selectable);

	gtk_selection_convert (
		self->invisible,
		GDK_SELECTION_CLIPBOARD,
		gdk_atom_intern ("x-uid-list", FALSE),
		GDK_CURRENT_TIME);
}

static void
e_message_list_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = e_message_list_selectable_update_actions;
	iface->select_all = e_message_list_selectable_select_all;
	iface->copy_clipboard = e_message_list_selectable_copy_clipboard;
	iface->cut_clipboard = e_message_list_selectable_cut_clipboard;
	iface->paste_clipboard = e_message_list_selectable_paste_clipboard;
}

static gboolean
on_vtree_drag_motion (EVirtualTree *vtree,
		      GdkDragContext *context,
		      guint row,
		      guint time,
		      gpointer user_data)
{
	EMessageList *self = E_MESSAGE_LIST (user_data);
	GtkWidget *source_widget;

	if (!self->folder) {
		gdk_drag_status (context, 0, time);
		return TRUE;
	}

	source_widget = gtk_drag_get_source_widget (context);
	if (source_widget && gtk_widget_is_ancestor (source_widget, GTK_WIDGET (self))) {
		gdk_drag_status (context, 0, time);
		return TRUE;
	}

	return FALSE;
}

static gpointer
message_list_get_legacy_etable_column_map_cb (EVirtualTree *vtree,
					      guint *out_n_column_ids,
					      gpointer user_data)
{
	return (gpointer) e_message_list_get_legacy_etable_column_ids (out_n_column_ids);
}

static void
e_message_list_constructed (GObject *object)
{
	EMessageList *self = E_MESSAGE_LIST (object);
	AtkObject *atk_obj;
	GtkWidget *sw;

	G_OBJECT_CLASS (e_message_list_parent_class)->constructed (object);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

	self->mail_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	self->show_subject_above_sender = g_settings_get_boolean (self->mail_settings, "show-subject-above-sender");
	self->show_email = g_settings_get_boolean (self->mail_settings, "show-email");
	self->show_avatar = g_settings_get_boolean (self->mail_settings, "show-avatar-in-message-list");
	self->show_body_preview = g_settings_get_boolean (self->mail_settings, "show-body-preview-in-message-list");
	self->expanded_default = g_settings_get_boolean (self->mail_settings, "thread-expand");
	self->thread_latest = g_settings_get_boolean (self->mail_settings, "thread-latest");
	self->sort_children_ascending = g_settings_get_boolean (self->mail_settings, "thread-children-ascending");
	self->show_deleted = g_settings_get_boolean (self->mail_settings, "show-deleted");
	self->show_junk = g_settings_get_boolean (self->mail_settings, "show-junk");
	g_signal_connect (self->mail_settings, "changed",
		G_CALLBACK (on_mail_settings_changed), self);

	self->vtree = E_VIRTUAL_TREE (e_virtual_tree_new (NULL));
	g_signal_connect (self->vtree, "destroy",
		G_CALLBACK (gtk_widget_destroyed), &self->vtree);
	e_virtual_tree_set_selection_mode (self->vtree, GTK_SELECTION_MULTIPLE);
	g_signal_connect (self->vtree, "get-legacy-etable-column-map",
		G_CALLBACK (message_list_get_legacy_etable_column_map_cb), NULL);
	g_settings_bind (self->mail_settings, "message-list-sort-on-header-click",
		self->vtree, "header-click-sort-policy",
		G_SETTINGS_BIND_DEFAULT);

	setup_columns (self);

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (self));
	if (atk_obj) {
		atk_object_set_name (atk_obj, _("Message List"));
		atk_object_set_description (atk_obj, _("List of email messages in the selected folder"));
	}

	self->eds_settings = e_util_ref_settings ("org.gnome.evolution-data-server");
	update_user_header_titles (self);
	g_signal_connect (self->eds_settings, "changed::camel-message-info-user-headers",
		G_CALLBACK (on_eds_settings_changed), self);

	self->model = g_object_new (E_TYPE_MESSAGE_LIST_MODEL, NULL);

	g_signal_connect (self->vtree, "cell-clicked",
		G_CALLBACK (on_cell_clicked), self);
	g_signal_connect (self->vtree, "column-state-changed",
		G_CALLBACK (on_column_state_changed), self);
	g_signal_connect (self->vtree, "cursor-changed",
		G_CALLBACK (on_vtree_cursor_changed), self);
	g_signal_connect (self->vtree, "selection-changed",
		G_CALLBACK (on_vtree_selection_changed), self);

	e_virtual_tree_enable_drag_source (self->vtree,
		GDK_BUTTON1_MASK,
		ml_drag_types, G_N_ELEMENTS (ml_drag_types),
		GDK_ACTION_MOVE | GDK_ACTION_COPY);
	e_virtual_tree_enable_drag_dest (self->vtree,
		ml_drop_types, G_N_ELEMENTS (ml_drop_types),
		GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect (self->vtree, "tree-drag-data-get",
		G_CALLBACK (on_vtree_drag_data_get), self);
	g_signal_connect (self->vtree, "tree-drag-data-received",
		G_CALLBACK (on_vtree_drag_data_received), self);
	g_signal_connect (self->vtree, "tree-drag-motion",
		G_CALLBACK (on_vtree_drag_motion), self);

	self->invisible = gtk_invisible_new ();
	gtk_selection_add_target (
		self->invisible, GDK_SELECTION_CLIPBOARD,
		gdk_atom_intern ("x-uid-list", FALSE), 0);
	g_signal_connect (self->invisible, "selection-get",
		G_CALLBACK (on_invisible_selection_get), self);
	g_signal_connect (self->invisible, "selection-clear-event",
		G_CALLBACK (on_invisible_selection_clear_event), self);
	g_signal_connect (self->invisible, "selection-received",
		G_CALLBACK (on_invisible_selection_received), self);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (self->vtree));

	gtk_box_pack_start (GTK_BOX (self), sw, TRUE, TRUE, 0);

	update_empty_message (self);

	gtk_widget_show_all (GTK_WIDGET (self));
}

static void
e_message_list_dispose (GObject *object)
{
	EMessageList *self = E_MESSAGE_LIST (object);

	if (self->idle_id) {
		g_source_remove (self->idle_id);
		self->idle_id = 0;
	}

	if (self->update_actions_idle_id) {
		g_source_remove (self->update_actions_idle_id);
		self->update_actions_idle_id = 0;
	}

	if (self->seen_id) {
		g_source_remove (self->seen_id);
		self->seen_id = 0;
	}

	message_list_regen_cancel (self);

	g_cancellable_cancel (self->avatar_fetch_cancellable);
	g_clear_object (&self->avatar_fetch_cancellable);
	g_clear_pointer (&self->avatar_fetch_pending, g_hash_table_destroy);
	if (self->avatar_pixbuf_cache_sweep_id) {
		g_source_remove (self->avatar_pixbuf_cache_sweep_id);
		self->avatar_pixbuf_cache_sweep_id = 0;
	}
	g_clear_pointer (&self->avatar_pixbuf_cache, g_hash_table_destroy);
	g_clear_pointer (&self->avatar_photo_miss_emails, g_hash_table_destroy);

	if (self->model) {
		e_message_list_model_disconnect (self->model);
		e_virtual_tree_set_model (self->vtree, NULL);
		g_clear_object (&self->model);
	}

	if (self->mail_settings) {
		g_signal_handlers_disconnect_by_func (self->mail_settings, on_mail_settings_changed, self);
		g_clear_object (&self->mail_settings);
	}

	if (self->eds_settings) {
		g_signal_handlers_disconnect_by_func (self->eds_settings, on_eds_settings_changed, self);
		g_clear_object (&self->eds_settings);
	}

	if (self->folder_view) {
		g_signal_handlers_disconnect_by_func (self->folder_view, on_folder_view_rebuild_needed, self);
		g_signal_handlers_disconnect_by_func (self->folder_view, on_folder_view_folder_changed, self);
	}
	g_clear_object (&self->folder_view);
	g_clear_object (&self->folder);
	g_clear_pointer (&self->search_sexp, g_free);
	camel_pstring_free (self->ensure_uid);
	self->ensure_uid = NULL;
	g_clear_object (&self->session);

	ml_clipboard_clear (self);
	g_clear_pointer (&self->invisible, gtk_widget_destroy);

	G_OBJECT_CLASS (e_message_list_parent_class)->dispose (object);
}

static void
e_message_list_style_updated (GtkWidget *widget)
{
	EMessageList *self = E_MESSAGE_LIST (widget);
	GdkRGBA *new_mail_fg_color = NULL;
	GdkRGBA *important_fg_color = NULL;
	GtkSettings *settings;
	gint xft_dpi = -1;

	GTK_WIDGET_CLASS (e_message_list_parent_class)->style_updated (widget);

	g_clear_pointer (&self->new_mail_bg_color, gdk_rgba_free);
	g_clear_pointer (&self->new_mail_fg_color, g_free);
	g_clear_pointer (&self->important_fg_color, g_free);

	gtk_widget_style_get (widget,
		"new-mail-bg-color", &self->new_mail_bg_color,
		"new-mail-fg-color", &new_mail_fg_color,
		"important-fg-color", &important_fg_color,
		NULL);

	if (new_mail_fg_color) {
		self->new_mail_fg_color = gdk_rgba_to_string (new_mail_fg_color);
		gdk_rgba_free (new_mail_fg_color);
	}

	if (important_fg_color) {
		self->important_fg_color = gdk_rgba_to_string (important_fg_color);
		gdk_rgba_free (important_fg_color);
	} else {
		self->important_fg_color = g_strdup (e_util_is_dark_theme (widget) ? DEFAULT_IMPORTANT_FG_COLOR_DARK : DEFAULT_IMPORTANT_FG_COLOR_LIGHT);
	}

	settings = gtk_widget_get_settings (widget);
	g_object_get (settings, "gtk-xft-dpi", &xft_dpi, NULL);
	self->avatar_font_scale = xft_dpi > 0 ? (xft_dpi / 1024.0) / 96.0 : 1.0;
}

static void
e_message_list_finalize (GObject *object)
{
	EMessageList *self = E_MESSAGE_LIST (object);

	g_mutex_clear (&self->regen_lock);
	camel_pstring_free (self->pending_select_uid);
	self->pending_select_uid = NULL;
	g_clear_pointer (&self->new_mail_bg_color, gdk_rgba_free);
	g_clear_pointer (&self->new_mail_fg_color, g_free);
	g_clear_pointer (&self->important_fg_color, g_free);

	G_OBJECT_CLASS (e_message_list_parent_class)->finalize (object);
}

static void
e_message_list_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_COPY_TARGET_LIST:
		g_value_set_boxed (value, NULL);
		return;
	case PROP_PASTE_TARGET_LIST:
		g_value_set_boxed (value, NULL);
		return;
	case PROP_THREADING:
		g_value_set_enum (value, e_message_list_get_threading (E_MESSAGE_LIST (object)));
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_message_list_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_COPY_TARGET_LIST:
	case PROP_PASTE_TARGET_LIST:
		return;
	case PROP_THREADING:
		e_message_list_set_threading (E_MESSAGE_LIST (object), g_value_get_enum (value));
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

const gchar * const *
e_message_list_get_legacy_etable_column_ids (guint *out_n_column_ids)
{
	static const gchar * const column_ids[] = {
		[0] = "status",
		[1] = "flagged",
		[2] = "score",
		[3] = "attachment",
		[4] = "from",
		[5] = "subject",
		[6] = "date-sent",
		[7] = "date-received",
		[8] = "to",
		[9] = "size",
		[10] = "followup-flag-status",
		[11] = "followup-flag",
		[12] = "followup-due-by",
		[13] = "location",
		[14] = "sender",
		[15] = "recipients",
		[16] = "composite",
		[17] = "composite-to",
		[18] = "labels",
		[19] = "subject-trimmed",
		[23] = "uid",
		[24] = "sender-mail",
		[25] = "recipients-mail",
		[26] = "user-header-1",
		[27] = "user-header-2",
		[28] = "user-header-3",
		[29] = "preview",
		[30] = "correspondents"
	};

	if (out_n_column_ids)
		*out_n_column_ids = G_N_ELEMENTS (column_ids);

	return column_ids;
}

static void
e_message_list_class_init (EMessageListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = e_message_list_constructed;
	object_class->dispose = e_message_list_dispose;
	object_class->finalize = e_message_list_finalize;
	object_class->get_property = e_message_list_get_property;
	object_class->set_property = e_message_list_set_property;

	widget_class->style_updated = e_message_list_style_updated;

	signals[MESSAGE_SELECTED] = g_signal_new (
		"message-selected",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[MESSAGE_LIST_BUILT] = g_signal_new (
		"message-list-built",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	gtk_widget_class_set_css_name (widget_class, "MessageList");

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_boxed (
			"new-mail-bg-color",
			"New Mail Background Color",
			"Background color to use for new mails",
			GDK_TYPE_RGBA,
			G_PARAM_READABLE));

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_boxed (
			"new-mail-fg-color",
			"New Mail Foreground Color",
			"Foreground color to use for new mails",
			GDK_TYPE_RGBA,
			G_PARAM_READABLE));

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_boxed (
			"important-fg-color",
			NULL,
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READABLE));

	g_object_class_override_property (
		object_class, PROP_COPY_TARGET_LIST, "copy-target-list");
	g_object_class_override_property (
		object_class, PROP_PASTE_TARGET_LIST, "paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_THREADING,
		g_param_spec_enum (
			"threading",
			"Threading",
			"Threading mode for the message list",
			CAMEL_TYPE_FOLDER_VIEW_THREADING,
			CAMEL_FOLDER_VIEW_THREADING_NONE,
			G_PARAM_READWRITE));
}

static void
e_message_list_init (EMessageList *self)
{
	self->threading = CAMEL_FOLDER_VIEW_THREADING_NONE;
	self->group_by = CAMEL_FOLDER_VIEW_GROUP_BY_NONE;
	self->sort_column = CAMEL_FOLDER_VIEW_COLUMN_DATE_SENT;
	self->sort_order = CAMEL_SORT_DESCENDING;
	self->avatar_font_scale = 1.0;

	self->avatar_fetch_pending = g_hash_table_new_full (g_str_hash, g_str_equal,
		g_free, (GDestroyNotify) g_ptr_array_unref);
	self->avatar_fetch_cancellable = g_cancellable_new ();
	self->avatar_pixbuf_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
		g_free, avatar_pixbuf_cache_entry_free);
	self->avatar_photo_miss_emails = g_hash_table_new_full (g_str_hash, g_str_equal,
		g_free, g_free);
	self->avatar_pixbuf_cache_sweep_id = g_timeout_add_seconds (
		AVATAR_PIXBUF_CACHE_SWEEP_INTERVAL_SECONDS, avatar_pixbuf_cache_sweep_cb, self);

	g_mutex_init (&self->regen_lock);
}

GtkWidget *
e_message_list_new (EMailSession *session)
{
	EMessageList *self;

	self = g_object_new (E_TYPE_MESSAGE_LIST, NULL);

	if (session)
		self->session = g_object_ref (session);

	return GTK_WIDGET (self);
}

EMailSession *
e_message_list_get_session (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);

	return self->session;
}

void
e_message_list_set_folder (EMessageList *self,
			   CamelFolder *folder)
{
	CamelFolderView *folder_view;
	guint col_idx;
	gchar *expand_filename;
	gchar *expand_state;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));
	g_return_if_fail (!folder || CAMEL_IS_FOLDER (folder));

	message_list_regen_cancel (self);

	e_message_list_model_disconnect (self->model);
	e_virtual_tree_set_model (self->vtree, NULL);
	if (self->folder_view) {
		g_signal_handlers_disconnect_by_func (self->folder_view, on_folder_view_rebuild_needed, self);
		g_signal_handlers_disconnect_by_func (self->folder_view, on_folder_view_folder_changed, self);
	}
	g_clear_object (&self->folder_view);
	g_clear_object (&self->folder);

	if (!folder) {
		self->is_trash_folder = FALSE;
		self->is_junk_folder = FALSE;
		update_empty_message (self);
		return;
	}

	self->folder = g_object_ref (folder);
	self->is_trash_folder = (camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_TRASH) != 0;
	self->is_junk_folder = (camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_JUNK) != 0;
	self->just_set_folder = TRUE;

	folder_view = camel_folder_view_new (folder, self->expanded_default);

	if (sort_column_to_column_index (self->sort_column, &col_idx))
		e_virtual_tree_set_column_sort (self->vtree, col_idx,
			self->sort_order == CAMEL_SORT_ASCENDING ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING, 1);

	e_virtual_tree_set_group_depth (self->vtree, self->group_by != CAMEL_FOLDER_VIEW_GROUP_BY_NONE ? 1 : 0);
	e_virtual_tree_set_expander_visible (self->vtree, self->threading != CAMEL_FOLDER_VIEW_THREADING_NONE);

	expand_filename = mail_config_folder_to_cachename (folder, "et-expanded-");
	expand_state = NULL;

	if (expand_filename && g_file_get_contents (expand_filename, &expand_state, NULL, NULL))
		camel_folder_view_load_expand_state (folder_view, expand_state);

	g_free (expand_state);
	g_free (expand_filename);

	self->folder_view = folder_view;

	update_empty_message (self);

	message_list_regen (self);
}

CamelFolder *
e_message_list_ref_folder (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);

	return self->folder ? g_object_ref (self->folder) : NULL;
}

void
e_message_list_set_show_deleted (EMessageList *self,
				 gboolean show_deleted)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (self->show_deleted == show_deleted)
		return;

	self->show_deleted = show_deleted;
	message_list_regen (self);
}

gboolean
e_message_list_get_show_deleted (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), FALSE);
	return self->show_deleted;
}

void
e_message_list_set_show_junk (EMessageList *self,
			      gboolean show_junk)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (self->show_junk == show_junk)
		return;

	self->show_junk = show_junk;
	message_list_regen (self);
}

gboolean
e_message_list_get_show_junk (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), FALSE);
	return self->show_junk;
}

void
e_message_list_set_search_sexp (EMessageList *self,
				const gchar *sexp)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (g_strcmp0 (self->search_sexp, sexp) == 0)
		return;

	g_free (self->search_sexp);
	self->search_sexp = g_strdup (sexp);

	message_list_regen (self);
}

const gchar *
e_message_list_get_search_sexp (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);
	return self->search_sexp;
}

void
e_message_list_set_ensure_uid (EMessageList *self,
			       const gchar *uid)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (g_strcmp0 (self->ensure_uid, uid) == 0)
		return;

	camel_pstring_free (self->ensure_uid);
	self->ensure_uid = uid ? camel_pstring_strdup (uid) : NULL;

	message_list_regen (self);
}

const gchar *
e_message_list_get_ensure_uid (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);
	return self->ensure_uid;
}

void
e_message_list_set_threading (EMessageList *self,
			      CamelFolderViewThreading mode)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (self->threading == mode)
		return;

	self->threading = mode;
	e_virtual_tree_set_expander_visible (self->vtree, mode != CAMEL_FOLDER_VIEW_THREADING_NONE);
	message_list_regen (self);

	g_object_notify (G_OBJECT (self), "threading");
}

CamelFolderViewThreading
e_message_list_get_threading (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), CAMEL_FOLDER_VIEW_THREADING_NONE);
	return self->threading;
}

void
e_message_list_set_thread_subject (EMessageList *self,
				   gboolean enabled)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (self->thread_subject == enabled)
		return;

	self->thread_subject = enabled;
	message_list_regen (self);
}

gboolean
e_message_list_get_thread_subject (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), FALSE);
	return self->thread_subject;
}

void
e_message_list_set_group_by (EMessageList *self,
			     CamelFolderViewGroupBy group_by)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (self->group_by == group_by)
		return;

	self->group_by = group_by;
	e_virtual_tree_set_group_depth (self->vtree, group_by != CAMEL_FOLDER_VIEW_GROUP_BY_NONE ? 1 : 0);
	message_list_regen (self);
}

CamelFolderViewGroupBy
e_message_list_get_group_by (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), CAMEL_FOLDER_VIEW_GROUP_BY_NONE);
	return self->group_by;
}

void
e_message_list_set_empty_message (EMessageList *self,
				  const gchar *message)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	e_virtual_tree_set_empty_message (self->vtree, message);
}

const gchar *
e_message_list_get_empty_message (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);

	return e_virtual_tree_get_empty_message (self->vtree);
}

EVirtualTree *
e_message_list_get_virtual_tree (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);

	return self->vtree;
}

GtkTreeViewColumn *
e_message_list_get_column (EMessageList *self,
			   EMessageListColumn column)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);
	g_return_val_if_fail (column < E_MESSAGE_LIST_N_COLUMNS, NULL);

	return e_virtual_tree_get_column (self->vtree, (guint) column);
}

const gchar *
e_message_list_get_cursor_uid (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);

	return (const gchar *) e_virtual_tree_get_cursor_key (self->vtree);
}

static gboolean
row_matches_flags (EVirtualTreeModel *model,
		   guint row,
		   guint32 flags,
		   guint32 mask)
{
	GObject *row_obj;
	EMessageListNode *node;
	gboolean matches = FALSE;

	row_obj = e_virtual_tree_model_dup_row (model, row);
	if (!row_obj)
		return FALSE;

	node = E_MESSAGE_LIST_NODE (row_obj);
	if (node->row) {
		guint32 row_flags = camel_folder_view_row_get_flags (node->row);
		matches = (row_flags & mask) == (flags & mask);
	}

	g_object_unref (row_obj);

	return matches;
}

guint
e_message_list_get_seen_id (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), 0);

	return self->seen_id;
}

void
e_message_list_set_seen_id (EMessageList *self,
			     guint seen_id)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	self->seen_id = seen_id;
}

gboolean
e_message_list_get_last_sel_single (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), FALSE);

	return self->last_sel_single;
}

gboolean
e_message_list_select (EMessageList *self,
		       EMessageListSelectDirection direction,
		       guint32 flags,
		       guint32 mask)
{
	EVirtualTreeModel *model;
	guint total, ii;
	gint cursor, step;
	gboolean forward, wrap;

	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), FALSE);

	model = E_VIRTUAL_TREE_MODEL (self->model);
	total = e_virtual_tree_model_get_row_count (model);
	if (total == 0)
		return FALSE;

	forward = (direction & E_MESSAGE_LIST_SELECT_DIRECTION) == E_MESSAGE_LIST_SELECT_NEXT;
	wrap = (direction & E_MESSAGE_LIST_SELECT_WRAP) != 0;
	step = forward ? 1 : -1;
	cursor = e_virtual_tree_get_cursor (self->vtree);

	if (cursor < 0)
		ii = forward ? 0 : total - 1;
	else
		ii = (guint) (cursor + step);

	while (TRUE) {
		if (forward && ii >= total) {
			if (!wrap)
				return FALSE;
			ii = 0;
			wrap = FALSE;
		} else if (!forward && ii >= total) {
			if (!wrap)
				return FALSE;
			ii = total - 1;
			wrap = FALSE;
		}

		if ((gint) ii == cursor)
			return FALSE;

		if (row_matches_flags (model, ii, flags, mask)) {
			e_virtual_tree_set_cursor (self->vtree, (gint) ii);
			e_virtual_tree_unselect_all (self->vtree);
			e_virtual_tree_select_row (self->vtree, ii);
			return TRUE;
		}

		ii = (guint) ((gint) ii + step);
	}
}

void
e_message_list_select_uid (EMessageList *self,
			    const gchar *uid,
			    gboolean with_fallback)
{
	EVirtualTreeModel *model;
	const gchar *puid;
	guint row = G_MAXUINT;
	gboolean regen_active;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	model = E_VIRTUAL_TREE_MODEL (self->model);

	g_mutex_lock (&self->regen_lock);
	regen_active = self->regen_task != NULL || self->regen_idle_source != NULL;
	g_mutex_unlock (&self->regen_lock);

	if (regen_active || self->thaw_needs_regen) {
		camel_pstring_free (self->pending_select_uid);
		self->pending_select_uid = uid ? camel_pstring_strdup (uid) : NULL;
		self->pending_select_fallback = with_fallback;
		return;
	}

	if (uid) {
		puid = camel_pstring_strdup (uid);
		row = e_virtual_tree_model_find_row_by_key (model, puid);
		camel_pstring_free (puid);
	}

	if (row != G_MAXUINT) {
		e_virtual_tree_set_cursor_centered (self->vtree, (gint) row);
		e_virtual_tree_select_row (self->vtree, row);
		return;
	}

	if (with_fallback) {
		guint total = e_virtual_tree_model_get_row_count (model);
		guint oldest_unread_row = G_MAXUINT;
		gint64 oldest_unread_date = 0;
		guint newest_read_row = G_MAXUINT;
		gint64 newest_read_date = 0;
		guint ii;

		for (ii = 0; ii < total; ii++) {
			GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);
			EMessageListNode *node;

			if (!row_obj)
				continue;

			node = E_MESSAGE_LIST_NODE (row_obj);
			if (node->row) {
				guint32 flags = camel_folder_view_row_get_flags (node->row);
				gint64 date_received = camel_folder_view_row_get_date_received (node->row);

				if (flags & CAMEL_MESSAGE_SEEN) {
					if (newest_read_row == G_MAXUINT || date_received > newest_read_date) {
						newest_read_date = date_received;
						newest_read_row = ii;
					}
				} else if (oldest_unread_row == G_MAXUINT || date_received < oldest_unread_date) {
					oldest_unread_date = date_received;
					oldest_unread_row = ii;
				}
			}

			g_object_unref (row_obj);
		}

		if (oldest_unread_row == G_MAXUINT)
			oldest_unread_row = newest_read_row;

		if (oldest_unread_row != G_MAXUINT) {
			e_virtual_tree_set_cursor_centered (self->vtree, (gint) oldest_unread_row);
			e_virtual_tree_select_row (self->vtree, oldest_unread_row);
			return;
		}
	}

	if (!uid)
		e_virtual_tree_set_cursor (self->vtree, -1);
}

void
e_message_list_select_next_thread (EMessageList *self)
{
	EVirtualTreeModel *model;
	guint total;
	gint cursor;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	model = E_VIRTUAL_TREE_MODEL (self->model);
	total = e_virtual_tree_model_get_row_count (model);
	cursor = e_virtual_tree_get_cursor (self->vtree);

	if (cursor < 0 || (guint) cursor + 1 >= total)
		return;

	for (guint ii = (guint) cursor + 1; ii < total; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);

		if (row_obj) {
			guint depth = e_virtual_tree_model_get_depth (model, row_obj);
			g_object_unref (row_obj);

			if (depth == 0) {
				e_virtual_tree_set_cursor (self->vtree, (gint) ii);
				e_virtual_tree_unselect_all (self->vtree);
				e_virtual_tree_select_row (self->vtree, ii);
				return;
			}
		}
	}
}

void
e_message_list_select_prev_thread (EMessageList *self)
{
	EVirtualTreeModel *model;
	gint cursor;
	gboolean passed_current = FALSE;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	model = E_VIRTUAL_TREE_MODEL (self->model);
	cursor = e_virtual_tree_get_cursor (self->vtree);

	if (cursor <= 0)
		return;

	for (gint ii = cursor - 1; ii >= 0; ii--) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, (guint) ii);

		if (row_obj) {
			guint depth = e_virtual_tree_model_get_depth (model, row_obj);
			g_object_unref (row_obj);

			if (depth == 0) {
				if (passed_current) {
					e_virtual_tree_set_cursor (self->vtree, ii);
					e_virtual_tree_unselect_all (self->vtree);
					e_virtual_tree_select_row (self->vtree, (guint) ii);
					return;
				}
				passed_current = TRUE;
			}
		}
	}
}

static const gchar *
get_node_uid (GObject *row_obj)
{
	EMessageListNode *node = E_MESSAGE_LIST_NODE (row_obj);

	if (node->row)
		return camel_folder_view_row_get_uid (node->row);

	return NULL;
}

GPtrArray *
e_message_list_get_selected (EMessageList *self)
{
	GPtrArray *selected_rows;
	GPtrArray *uids;
	guint ii;

	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);

	selected_rows = e_virtual_tree_get_selected_rows (self->vtree);
	uids = g_ptr_array_new_full (selected_rows->len, (GDestroyNotify) camel_pstring_free);

	for (ii = 0; ii < selected_rows->len; ii++) {
		GObject *row_obj = g_ptr_array_index (selected_rows, ii);
		const gchar *uid = get_node_uid (row_obj);

		if (uid)
			g_ptr_array_add (uids, (gpointer) camel_pstring_strdup (uid));
	}

	g_ptr_array_unref (selected_rows);

	return uids;
}

GPtrArray *
e_message_list_get_selected_with_collapsed_threads (EMessageList *self)
{
	EVirtualTreeModel *model;
	GPtrArray *selected_rows;
	GPtrArray *uids;
	GHashTable *seen;
	guint ii;

	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), NULL);

	model = E_VIRTUAL_TREE_MODEL (self->model);
	selected_rows = e_virtual_tree_get_selected_rows (self->vtree);
	uids = g_ptr_array_new_full (selected_rows->len, (GDestroyNotify) camel_pstring_free);
	seen = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (ii = 0; ii < selected_rows->len; ii++) {
		GObject *row_obj = g_ptr_array_index (selected_rows, ii);
		const gchar *uid = get_node_uid (row_obj);

		if (uid && !g_hash_table_contains (seen, uid)) {
			g_hash_table_add (seen, (gpointer) uid);
			g_ptr_array_add (uids, (gpointer) camel_pstring_strdup (uid));
		}

		if (e_virtual_tree_model_is_expandable (model, row_obj) &&
		    !e_virtual_tree_model_get_expanded (model, row_obj)) {
			guint row_idx = e_virtual_tree_model_find_row_by_key (model, uid);
			guint total = e_virtual_tree_model_get_row_count (model);
			guint jj;

			if (row_idx == G_MAXUINT)
				continue;

			for (jj = row_idx + 1; jj < total; jj++) {
				GObject *child_obj = e_virtual_tree_model_dup_row (model, jj);

				if (!child_obj)
					continue;

				if (e_virtual_tree_model_get_depth (model, child_obj) == 0) {
					g_object_unref (child_obj);
					break;
				}

				uid = get_node_uid (child_obj);
				if (uid && !g_hash_table_contains (seen, uid)) {
					g_hash_table_add (seen, (gpointer) uid);
					g_ptr_array_add (uids, (gpointer) camel_pstring_strdup (uid));
				}

				g_object_unref (child_obj);
			}
		}
	}

	g_hash_table_destroy (seen);
	g_ptr_array_unref (selected_rows);

	return uids;
}

guint
e_message_list_selected_count (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), 0);

	return e_virtual_tree_selected_count (self->vtree);
}

guint
e_message_list_count (EMessageList *self)
{
	guint total;

	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), 0);

	total = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (self->model));

	if (self->group_by != CAMEL_FOLDER_VIEW_GROUP_BY_NONE && self->folder_view)
		total -= camel_folder_view_get_group_count (self->folder_view);

	return total;
}

gboolean
e_message_list_contains_uid (EMessageList *self,
			      const gchar *uid)
{
	const gchar *puid;
	gboolean found;

	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	puid = camel_pstring_strdup (uid);
	found = e_virtual_tree_model_find_row_by_key (E_VIRTUAL_TREE_MODEL (self->model), puid) != G_MAXUINT;
	camel_pstring_free (puid);

	return found;
}

void
e_message_list_freeze (EMessageList *self)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	self->frozen++;
}

void
e_message_list_thaw (EMessageList *self)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));
	g_return_if_fail (self->frozen > 0);

	self->frozen--;

	if (self->frozen == 0 && self->thaw_needs_regen) {
		self->thaw_needs_regen = FALSE;
		message_list_regen (self);
	}
}

void
e_message_list_set_regen_selects_unread (EMessageList *self,
					 gboolean selects_unread)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	self->regen_selects_unread = selects_unread;
}

void
e_message_list_inc_setting_up_search_folder (EMessageList *self)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	g_atomic_int_inc (&self->setting_up_search_folder);
}

void
e_message_list_dec_setting_up_search_folder (EMessageList *self)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (g_atomic_int_dec_and_test (&self->setting_up_search_folder))
		update_empty_message (self);
}

gboolean
e_message_list_is_setting_up_search_folder (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), FALSE);

	return g_atomic_int_get (&self->setting_up_search_folder) > 0;
}

void
e_message_list_expand_all_threads (EMessageList *self)
{
	EVirtualTreeModel *model;
	guint total, ii;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	model = E_VIRTUAL_TREE_MODEL (self->model);
	total = e_virtual_tree_model_get_row_count (model);

	for (ii = 0; ii < total; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);

		if (row_obj) {
			if (e_virtual_tree_model_is_expandable (model, row_obj))
				e_virtual_tree_model_set_expanded (model, row_obj, TRUE);
			g_object_unref (row_obj);
		}
	}

	e_virtual_tree_model_emit_row_count_changed (model);
}

void
e_message_list_collapse_all_threads (EMessageList *self)
{
	EVirtualTreeModel *model;
	guint total, ii;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	model = E_VIRTUAL_TREE_MODEL (self->model);
	total = e_virtual_tree_model_get_row_count (model);

	for (ii = 0; ii < total; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);

		if (row_obj) {
			if (e_virtual_tree_model_is_expandable (model, row_obj))
				e_virtual_tree_model_set_expanded (model, row_obj, FALSE);
			g_object_unref (row_obj);
		}
	}

	e_virtual_tree_model_emit_row_count_changed (model);
}

void
e_message_list_select_thread (EMessageList *self)
{
	EVirtualTreeModel *model;
	guint total;
	gint cursor;
	guint root_row, ii;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	model = E_VIRTUAL_TREE_MODEL (self->model);
	total = e_virtual_tree_model_get_row_count (model);
	cursor = e_virtual_tree_get_cursor (self->vtree);

	if (cursor < 0 || (guint) cursor >= total)
		return;

	root_row = (guint) cursor;
	while (root_row > 0) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, root_row);

		if (!row_obj)
			break;

		if (e_virtual_tree_model_get_depth (model, row_obj) == 0) {
			g_object_unref (row_obj);
			break;
		}

		g_object_unref (row_obj);
		root_row--;
	}

	for (ii = root_row; ii < total; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);

		if (!row_obj)
			continue;

		if (ii > root_row && e_virtual_tree_model_get_depth (model, row_obj) == 0) {
			g_object_unref (row_obj);
			break;
		}

		e_virtual_tree_select_row (self->vtree, ii);
		g_object_unref (row_obj);
	}
}

void
e_message_list_select_subthread (EMessageList *self)
{
	EVirtualTreeModel *model;
	GObject *cursor_obj;
	guint total, cursor_depth, ii;
	gint cursor;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	model = E_VIRTUAL_TREE_MODEL (self->model);
	total = e_virtual_tree_model_get_row_count (model);
	cursor = e_virtual_tree_get_cursor (self->vtree);

	if (cursor < 0 || (guint) cursor >= total)
		return;

	cursor_depth = 0;
	cursor_obj = e_virtual_tree_model_dup_row (model, (guint) cursor);
	if (cursor_obj) {
		cursor_depth = e_virtual_tree_model_get_depth (model, cursor_obj);
		g_object_unref (cursor_obj);
	}

	e_virtual_tree_select_row (self->vtree, (guint) cursor);

	for (ii = (guint) cursor + 1; ii < total; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);

		if (!row_obj)
			continue;

		if (e_virtual_tree_model_get_depth (model, row_obj) <= cursor_depth) {
			g_object_unref (row_obj);
			break;
		}

		e_virtual_tree_select_row (self->vtree, ii);
		g_object_unref (row_obj);
	}
}

void
e_message_list_save_state (EMessageList *self)
{
	gchar *state;
	gchar *filename;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	if (!self->folder_view || !self->folder)
		return;

	if (self->search_sexp && *self->search_sexp)
		return;

	state = camel_folder_view_save_expand_state (self->folder_view);
	filename = mail_config_folder_to_cachename (self->folder, "et-expanded-");

	if (state && filename)
		g_file_set_contents (filename, state, -1, NULL);

	g_free (state);
	g_free (filename);
}

gboolean
e_message_list_get_just_set_folder (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), FALSE);

	return self->just_set_folder;
}

gint
e_message_list_get_cursor_row (EMessageList *self)
{
	g_return_val_if_fail (E_IS_MESSAGE_LIST (self), -1);

	return e_virtual_tree_get_cursor (self->vtree);
}

void
e_message_list_sort_uids (EMessageList *self,
			   GPtrArray *uids)
{
	GHashTable *uid_to_row;
	guint total, ii;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));
	g_return_if_fail (uids != NULL);

	if (uids->len <= 1 || !self->folder_view)
		return;

	total = camel_folder_view_get_row_count (self->folder_view);
	uid_to_row = g_hash_table_new (g_str_hash, g_str_equal);

	for (ii = 0; ii < total; ii++) {
		CamelFolderViewRow *row;

		if (camel_folder_view_is_group_row (self->folder_view, ii))
			continue;

		row = camel_folder_view_get_row (self->folder_view, ii);
		if (row) {
			g_hash_table_insert (uid_to_row,
				(gpointer) camel_folder_view_row_get_uid (row),
				GUINT_TO_POINTER (ii));
		}
	}

	for (ii = 0; ii < uids->len; ii++) {
		guint jj, min_row, min_idx;
		const gchar *uid;
		gpointer val;

		uid = g_ptr_array_index (uids, ii);
		if (!uid || !g_hash_table_lookup_extended (uid_to_row, uid, NULL, &val))
			min_row = G_MAXUINT;
		else
			min_row = GPOINTER_TO_UINT (val);

		min_idx = ii;

		for (jj = ii + 1; jj < uids->len; jj++) {
			uid = g_ptr_array_index (uids, jj);
			if (!uid || !g_hash_table_lookup_extended (uid_to_row, uid, NULL, &val))
				continue;

			if (GPOINTER_TO_UINT (val) < min_row) {
				min_row = GPOINTER_TO_UINT (val);
				min_idx = jj;
			}
		}

		if (min_idx != ii) {
			gpointer tmp = g_ptr_array_index (uids, ii);
			g_ptr_array_index (uids, ii) = g_ptr_array_index (uids, min_idx);
			g_ptr_array_index (uids, min_idx) = tmp;
		}
	}

	g_hash_table_destroy (uid_to_row);
}

void
e_message_list_ensure_cursor_visible (EMessageList *self)
{
	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	e_virtual_tree_scroll_cursor_into_view (self->vtree);
}

void
e_message_list_apply_sort_from_vtree (EMessageList *self)
{
	CamelFolderViewColumn new_column = CAMEL_FOLDER_VIEW_COLUMN_DATE_SENT;
	CamelSortType new_order = CAMEL_SORT_DESCENDING;
	gint best_priority = G_MAXINT;
	guint ii;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));

	for (ii = 0; ii < E_MESSAGE_LIST_N_COLUMNS; ii++) {
		GtkSortType sort_order;
		gint priority;

		if (e_virtual_tree_get_column_sort (self->vtree, ii, &sort_order, &priority)) {
			CamelFolderViewColumn column;

			if (column_index_to_sort_column (ii, &column) && priority < best_priority) {
				best_priority = priority;
				new_column = column;
				new_order = (sort_order == GTK_SORT_ASCENDING) ?
					CAMEL_SORT_ASCENDING : CAMEL_SORT_DESCENDING;
			}
		}
	}

	if (self->sort_column == new_column && self->sort_order == new_order)
		return;

	self->sort_column = new_column;
	self->sort_order = new_order;
	message_list_regen (self);
}

void
e_message_list_copy_state_from (EMessageList *self,
				EMessageList *source)
{
	GKeyFile *key_file;

	g_return_if_fail (E_IS_MESSAGE_LIST (self));
	g_return_if_fail (E_IS_MESSAGE_LIST (source));

	self->threading = source->threading;
	self->thread_subject = source->thread_subject;
	self->thread_latest = source->thread_latest;
	self->sort_children_ascending = source->sort_children_ascending;
	self->group_by = source->group_by;
	self->sort_column = source->sort_column;
	self->sort_order = source->sort_order;
	self->expanded_default = source->expanded_default;
	self->show_deleted = source->show_deleted;
	self->show_junk = source->show_junk;

	g_free (self->search_sexp);
	self->search_sexp = g_strdup (source->search_sexp);

	key_file = g_key_file_new ();
	e_virtual_tree_save_column_state_to_key_file (source->vtree, key_file);
	e_virtual_tree_load_column_state_from_key_file (self->vtree, key_file);
	g_key_file_unref (key_file);

	if (self->mail_settings) {
		g_signal_handlers_disconnect_by_func (self->mail_settings, on_mail_settings_changed, self);
		g_clear_object (&self->mail_settings);
	}

	if (self->eds_settings) {
		g_signal_handlers_disconnect_by_func (self->eds_settings, on_eds_settings_changed, self);
		g_clear_object (&self->eds_settings);
	}
}
