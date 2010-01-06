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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libxml/tree.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include <camel/camel-session.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-stream-fs.h>

#include "e-util/e-mktemp.h"
#include "e-util/e-request.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-error.h"
#include "e-util/e-util.h"

#include "em-vfolder-rule.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-component.h"
#include "mail-send-recv.h"
#include "mail-vfolder.h"

#include "em-utils.h"
#include "em-popup.h"
#include "em-folder-tree.h"
#include "em-folder-utils.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"
#include "em-folder-properties.h"
#include "em-event.h"

#define d(x)

struct _selected_uri {
	gchar *key;		/* store:path or account/path */
	gchar *uri;
	CamelStore *store;
	gchar *path;
};

struct _EMFolderTreePrivate {
	GtkTreeView *treeview;
	EMFolderTreeModel *model;

	GSList *select_uris;	/* selected_uri structures of each path pending selection. */
	GHashTable *select_uris_table; /*Removed as they're encountered, so use this to find uri's not presnet but selected */

	guint32 excluded;
	gboolean (*excluded_func)(EMFolderTree *emft, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
	gpointer excluded_data;

	guint do_multiselect:1;	/* multiple select mode */
	guint cursor_set:1;	/* set to TRUE means we or something
				 * else has set the cursor, otherwise
				 * we need to set it when we set the
				 * selection */

	guint save_state_id;

	guint autoscroll_id;
	guint autoexpand_id;
	GtkTreeRowReference *autoexpand_row;

	guint loading_row_id;
	guint loaded_row_id;

	GtkTreeRowReference *drag_row;
	gboolean skip_double_click;
};

enum {
	FOLDER_ACTIVATED,  /* aka double-clicked or user hit enter */
	FOLDER_SELECTED,
	HIDDEN_KEY_EVENT,
	LAST_SIGNAL
};

/* Drag & Drop types */
enum DndDragType {
	DND_DRAG_TYPE_FOLDER,          /* drag an evo folder */
	DND_DRAG_TYPE_TEXT_URI_LIST,   /* drag to an mbox file */
	NUM_DRAG_TYPES
};

enum DndDropType {
	DND_DROP_TYPE_UID_LIST,        /* drop a list of message uids */
	DND_DROP_TYPE_FOLDER,          /* drop an evo folder */
	DND_DROP_TYPE_MESSAGE_RFC822,  /* drop a message/rfc822 stream */
	DND_DROP_TYPE_TEXT_URI_LIST,   /* drop an mbox file */
	NUM_DROP_TYPES
};

static GtkTargetEntry drag_types[] = {
	{ (gchar *) "x-folder",         0, DND_DRAG_TYPE_FOLDER         },
	{ (gchar *) "text/uri-list",    0, DND_DRAG_TYPE_TEXT_URI_LIST  },
};

static GtkTargetEntry drop_types[] = {
	{ (gchar *) "x-uid-list" ,      0, DND_DROP_TYPE_UID_LIST       },
	{ (gchar *) "x-folder",         0, DND_DROP_TYPE_FOLDER         },
	{ (gchar *) "message/rfc822",   0, DND_DROP_TYPE_MESSAGE_RFC822 },
	{ (gchar *) "text/uri-list",    0, DND_DROP_TYPE_TEXT_URI_LIST  },
};

static GdkAtom drag_atoms[NUM_DRAG_TYPES];
static GdkAtom drop_atoms[NUM_DROP_TYPES];

static guint signals[LAST_SIGNAL] = { 0 };

extern CamelSession *session;
extern CamelStore *vfolder_store;

static void em_folder_tree_class_init (EMFolderTreeClass *klass);
static void em_folder_tree_init (EMFolderTree *emft);
static void em_folder_tree_destroy (GtkObject *obj);
static void em_folder_tree_finalize (GObject *obj);

static gboolean emft_save_state (EMFolderTree *emft);
static void emft_queue_save_state (EMFolderTree *emft);

static void emft_update_model_expanded_state (struct _EMFolderTreePrivate *priv, GtkTreeIter *iter, gboolean expanded);

static void emft_tree_row_activated (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, EMFolderTree *emft);
static gboolean emft_tree_test_collapse_row (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *emft);
static void emft_tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *emft);
static gboolean emft_tree_button_press (GtkTreeView *treeview, GdkEventButton *event, EMFolderTree *emft);
static void emft_tree_selection_changed (GtkTreeSelection *selection, EMFolderTree *emft);
static gboolean emft_tree_user_event (GtkTreeView *treeview, GdkEvent *e, EMFolderTree *emft);
static gboolean emft_popup_menu (GtkWidget *widget);

struct _emft_selection_data {
	GtkTreeModel *model;
	GtkTreeIter *iter;
	gboolean set;
};

static GtkVBoxClass *parent_class = NULL;

GType
em_folder_tree_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFolderTreeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_folder_tree_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFolderTree),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_folder_tree_init,
		};

		type = g_type_register_static (GTK_TYPE_VBOX, "EMFolderTree", &info, 0);
	}

	return type;
}

static void
em_folder_tree_class_init (EMFolderTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_ref (GTK_TYPE_VBOX);

	object_class->finalize = em_folder_tree_finalize;
	gtk_object_class->destroy = em_folder_tree_destroy;

	widget_class->popup_menu = emft_popup_menu;

	signals[FOLDER_SELECTED] =
		g_signal_new ("folder-selected",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeClass, folder_selected),
			      NULL, NULL,
			      e_marshal_VOID__STRING_STRING_UINT,
			      G_TYPE_NONE, 3,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_UINT);

	signals[FOLDER_ACTIVATED] =
		g_signal_new ("folder-activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeClass, folder_activated),
			      NULL, NULL,
			      e_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals[HIDDEN_KEY_EVENT] =
		g_signal_new ("hidden-key-event",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMFolderTreeClass, hidden_key_event),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE, 1, GDK_TYPE_EVENT);
}

static gboolean
subdirs_contain_unread (GtkTreeModel *model, GtkTreeIter *root)
{
	guint unread;
	GtkTreeIter iter;

	if (!gtk_tree_model_iter_children (model, &iter, root))
		return FALSE;

	do {
		gtk_tree_model_get (model, &iter, COL_UINT_UNREAD, &unread, -1);
		if (unread)
			return TRUE;

		if (gtk_tree_model_iter_has_child (model, &iter))
			if (subdirs_contain_unread (model, &iter))
				return TRUE;
	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

static void
render_display_name (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
		     GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gboolean is_store, bold, subdirs_unread = FALSE;
	guint unread;
	gchar *display;
	gchar *name;

	gtk_tree_model_get (model, iter, COL_STRING_DISPLAY_NAME, &name,
			    COL_BOOL_IS_STORE, &is_store,
			    COL_UINT_UNREAD, &unread, -1);

	bold = is_store || unread;

	if (gtk_tree_model_iter_has_child (model, iter)) {
		gboolean expanded = TRUE;

		g_object_get (renderer, "is-expanded", &expanded, NULL);

		if (!bold || !expanded)
			subdirs_unread = subdirs_contain_unread (model, iter);
	}

	bold = bold || subdirs_unread;

	if (!is_store && unread) {
		/* Translators: This is the string used for displaying the
		 * folder names in folder trees. The first "%s" will be
		 * replaced by the folder's name and "%u" will be replaced
		 * with the number of unread messages in the folder. The
		 * second %s will be replaced with a "+" letter for collapsed
		 * folders with unread messages in some subfolder too,
		 * or with an empty string for other cases.
		 *
		 * Most languages should translate this as "%s (%u%s)". The
		 * languages that use localized digits (like Persian) may
		 * need to replace "%u" with "%Iu". Right-to-left languages
		 * (like Arabic and Hebrew) may need to add bidirectional
		 * formatting codes to take care of the cases the folder
		 * name appears in either direction.
		 */
		display = g_strdup_printf (C_("folder-display", "%s (%u%s)"), name, unread, subdirs_unread ? "+" : "");
		g_free (name);
	} else
		display = name;

	g_object_set (renderer, "text", display,
		      "weight", bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		      NULL);

	g_free (display);
}

static void
render_icon (GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter)
{
	GIcon *icon;
	guint unread;
	guint old_unread;
	gchar *icon_name;

	gtk_tree_model_get (
		model, iter,
		COL_STRING_ICON_NAME, &icon_name,
		COL_UINT_UNREAD_LAST_SEL, &old_unread,
		COL_UINT_UNREAD, &unread, -1);

	if (icon_name == NULL)
		return;

	icon = g_themed_icon_new (icon_name);

	/* Show an emblem if there's new mail. */
	if (unread > old_unread) {
		GIcon *temp_icon;
		GEmblem *emblem;

		temp_icon = g_themed_icon_new ("emblem-new");
		emblem = g_emblem_new (temp_icon);
		g_object_unref (temp_icon);

		temp_icon = g_emblemed_icon_new (icon, emblem);
		g_object_unref (emblem);
		g_object_unref (icon);

		icon = temp_icon;
	}

	g_object_set (renderer, "gicon", icon, NULL);

	g_object_unref (icon);
	g_free (icon_name);
}

static gboolean
emft_select_func(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean selected, gpointer data)
{
	EMFolderTree *emft = data;
	gboolean is_store;
	guint32 flags;
	GtkTreeIter iter;

	/* NB: This will be called with selection==NULL from tree_row_activated */
	if (emft->priv->excluded == 0 && emft->priv->excluded_func == NULL)
		return TRUE;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return TRUE;

	if (emft->priv->excluded_func != NULL)
		return emft->priv->excluded_func(emft, model, &iter, emft->priv->excluded_data);

	gtk_tree_model_get(model, &iter, COL_UINT_FLAGS, &flags, COL_BOOL_IS_STORE, &is_store, -1);
	if (is_store)
		flags |= CAMEL_FOLDER_NOSELECT;

	return (flags & emft->priv->excluded) == 0;
}

static void
emft_free_select_uri(gpointer v, gpointer data)
{
	struct _selected_uri *u = v;

	g_free(u->uri);
	if (u->store)
		camel_object_unref(u->store);
	g_free(u->key);
	g_free(u->path);
	g_free(u);
}

static void
em_folder_tree_init (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv;

	priv = g_new0 (struct _EMFolderTreePrivate, 1);
	priv->select_uris_table = g_hash_table_new(g_str_hash, g_str_equal);
	priv->treeview = NULL;
	priv->model = NULL;
	priv->drag_row = NULL;
	priv->skip_double_click = FALSE;

	emft->priv = priv;
}

static void
em_folder_tree_finalize (GObject *obj)
{
	EMFolderTree *emft = (EMFolderTree *) obj;

	if (emft->priv->select_uris) {
		g_slist_foreach(emft->priv->select_uris, emft_free_select_uri, emft);
		g_slist_free(emft->priv->select_uris);
		g_hash_table_destroy(emft->priv->select_uris_table);
		emft->priv->select_uris = NULL;
	}

	g_free (emft->priv);

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
em_folder_tree_destroy (GtkObject *obj)
{
	EMFolderTree *emft = (EMFolderTree *) obj;
	struct _EMFolderTreePrivate *priv = emft->priv;

	if (priv->loading_row_id != 0) {
		g_signal_handler_disconnect (priv->model, priv->loading_row_id);
		priv->loading_row_id = 0;
	}

	if (priv->loaded_row_id != 0) {
		g_signal_handler_disconnect (priv->model, priv->loaded_row_id);
		priv->loaded_row_id = 0;
	}

	if (priv->save_state_id != 0) {
		g_source_remove (priv->save_state_id);
		emft_save_state (emft);
	}

	if (priv->autoscroll_id != 0) {
		g_source_remove (priv->autoscroll_id);
		priv->autoscroll_id = 0;
	}

	if (priv->autoexpand_id != 0) {
		gtk_tree_row_reference_free (priv->autoexpand_row);
		priv->autoexpand_row = NULL;

		g_source_remove (priv->autoexpand_id);
		priv->autoexpand_id = 0;
	}

	priv->treeview = NULL;
	priv->model = NULL;

	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static GtkTreeView *
folder_tree_new (EMFolderTree *emft, EMFolderTreeModel *model)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *tree;
	gboolean side_bar_search;
	GConfClient *gconf;

	gconf = mail_config_get_gconf_client ();
	side_bar_search = gconf_client_get_bool (gconf, "/apps/evolution/mail/display/side_bar_search", NULL);

	tree = gtk_tree_view_new_with_model ((GtkTreeModel *) model);
	GTK_WIDGET_SET_FLAGS(tree, GTK_CAN_FOCUS);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column ((GtkTreeView *) tree, column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COL_BOOL_IS_FOLDER);
	gtk_tree_view_column_set_cell_data_func (
		column, renderer, (GtkTreeCellDataFunc)
		render_icon, NULL, NULL);

	renderer = gtk_cell_renderer_text_new ();
	if (!gconf_client_get_bool (gconf, "/apps/evolution/mail/display/no_folder_dots", NULL))
		g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, render_display_name, NULL, NULL);

	selection = gtk_tree_view_get_selection ((GtkTreeView *) tree);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_selection_set_select_function(selection, emft_select_func, emft, NULL);
	gtk_tree_view_set_headers_visible ((GtkTreeView *) tree, FALSE);

	gtk_tree_view_set_search_column((GtkTreeView *)tree, COL_STRING_DISPLAY_NAME);
	gtk_tree_view_set_enable_search((GtkTreeView *)tree, side_bar_search);

	return (GtkTreeView *) tree;
}

static void
em_folder_tree_construct (EMFolderTree *emft, EMFolderTreeModel *model)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkWidget *scrolled;

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);

	priv->model = model;
	priv->treeview = folder_tree_new (emft, model);
	gtk_widget_show ((GtkWidget *) priv->treeview);

	g_signal_connect (priv->treeview, "row-expanded", G_CALLBACK (emft_tree_row_expanded), emft);
	g_signal_connect (priv->treeview, "test-collapse-row", G_CALLBACK (emft_tree_test_collapse_row), emft);
	g_signal_connect (priv->treeview, "row-activated", G_CALLBACK (emft_tree_row_activated), emft);
	g_signal_connect (priv->treeview, "button-press-event", G_CALLBACK (emft_tree_button_press), emft);
	g_signal_connect (priv->treeview, "key-press-event", G_CALLBACK (emft_tree_user_event), emft);

	selection = gtk_tree_view_get_selection ((GtkTreeView *) priv->treeview);
	g_signal_connect (selection, "changed", G_CALLBACK (emft_tree_selection_changed), emft);

	gtk_container_add ((GtkContainer *) scrolled, (GtkWidget *) priv->treeview);
	gtk_widget_show (scrolled);

	gtk_box_pack_start ((GtkBox *) emft, scrolled, TRUE, TRUE, 0);
}

GtkWidget *
em_folder_tree_new (void)
{
	EMFolderTreeModel *model;
	EMFolderTree *emft;

	model = em_folder_tree_model_new (e_get_user_data_dir ());
	emft = (EMFolderTree *) em_folder_tree_new_with_model (model);
	g_object_unref (model);

	return (GtkWidget *) emft;
}

/* NOTE: Removes and frees the selected uri structure */
static void
emft_select_uri(EMFolderTree *emft, GtkTreePath *path, struct _selected_uri *u)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(priv->treeview);
	gtk_tree_selection_select_path(selection, path);
	if (!priv->cursor_set) {
		gtk_tree_view_set_cursor (priv->treeview, path, NULL, FALSE);
		priv->cursor_set = TRUE;
	}
	gtk_tree_view_scroll_to_cell (priv->treeview, path, NULL, TRUE, 0.8f, 0.0f);
	g_hash_table_remove(priv->select_uris_table, u->key);
	priv->select_uris = g_slist_remove(priv->select_uris, u);
	emft_free_select_uri((gpointer)u, NULL);
}

static void
emft_expand_node (EMFolderTreeModel *model, const gchar *key, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	EAccount *account;
	CamelStore *store;
	const gchar *p;
	gchar *uid;
	gsize n;
	struct _selected_uri *u;

	if (!(p = strchr (key, '/')))
		n = strlen (key);
	else
		n = (p - key);

	uid = g_alloca (n + 1);
	memcpy (uid, key, n);
	uid[n] = '\0';

	if ((account = mail_config_get_account_by_uid (uid)) && account->enabled) {
		CamelException ex;

		camel_exception_init (&ex);
		store = (CamelStore *) camel_session_get_service (session, account->source->url, CAMEL_PROVIDER_STORE, &ex);
		camel_exception_clear (&ex);

		if (store == NULL)
			return;
	} else if (!strcmp (uid, "vfolder")) {
		if (!(store = vfolder_store))
			return;

		camel_object_ref (store);
	} else if (!strcmp (uid, "local")) {
		if (!(store = mail_component_peek_local_store (NULL)))
			return;

		camel_object_ref (store);
	} else {
		return;
	}

	if (!(si = g_hash_table_lookup (priv->model->store_hash, store))) {
		camel_object_unref (store);
		return;
	}

	camel_object_unref (store);

	if (p != NULL) {
		if (!(row = g_hash_table_lookup (si->full_hash, p + 1)))
			return;
	} else
		row = si->row;

	path = gtk_tree_row_reference_get_path (row);
	gtk_tree_view_expand_to_path (priv->treeview, path);

	u = g_hash_table_lookup(emft->priv->select_uris_table, key);
	if (u)
		emft_select_uri(emft, path, u);

	gtk_tree_path_free (path);
}

static void
emft_maybe_expand_row (EMFolderTreeModel *model, GtkTreePath *tree_path, GtkTreeIter *iter, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	gboolean is_store;
	CamelStore *store;
	EAccount *account;
	gchar *full_name;
	gchar *key;
	struct _selected_uri *u;
	gboolean is_expanded;

	gtk_tree_model_get ((GtkTreeModel *) model, iter,
			    COL_STRING_FULL_NAME, &full_name,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_BOOL_IS_STORE, &is_store,
			    -1);

	si = g_hash_table_lookup (model->store_hash, store);
	if ((account = mail_config_get_account_by_name (si->display_name))) {
		key = g_strdup_printf ("%s/%s", account->uid, full_name ? full_name : "");
	} else if (CAMEL_IS_VEE_STORE (store)) {
		/* vfolder store */
		key = g_strdup_printf ("vfolder/%s", full_name ? full_name : "");
	} else {
		/* local store */
		key = g_strdup_printf ("local/%s", full_name ? full_name : "");
	}

	is_expanded = em_folder_tree_model_get_expanded (model, key);
	u = g_hash_table_lookup(priv->select_uris_table, key);
	if (is_expanded || u) {
		if (is_expanded) {
			gtk_tree_view_expand_to_path (priv->treeview, tree_path);
			gtk_tree_view_expand_row (priv->treeview, tree_path, FALSE);
		} else {
			gchar *c = strrchr (key, '/');

			*c = '\0';
			emft_expand_node (model, key, emft);
		}

		if (u)
			emft_select_uri(emft, tree_path, u);
	}

	g_free (full_name);
	g_free (key);
}

GtkWidget *
em_folder_tree_new_with_model (EMFolderTreeModel *model)
{
	EMFolderTree *emft;
	AtkObject *a11y;

	emft = g_object_new (EM_TYPE_FOLDER_TREE, NULL);
	em_folder_tree_construct (emft, model);
	g_object_ref (model);

	em_folder_tree_model_expand_foreach (model, (EMFTModelExpandFunc)emft_expand_node, emft);

	emft->priv->loading_row_id = g_signal_connect (model, "loading-row", G_CALLBACK (emft_maybe_expand_row), emft);
	emft->priv->loaded_row_id = g_signal_connect (model, "loaded-row", G_CALLBACK (emft_maybe_expand_row), emft);

	a11y = gtk_widget_get_accessible (GTK_WIDGET (emft->priv->treeview));
	atk_object_set_name (a11y, _("Mail Folder Tree"));

	return (GtkWidget *) emft;
}

static void
tree_drag_begin (GtkWidget *widget, GdkDragContext *context, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection ((GtkTreeView *) widget);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	path = gtk_tree_model_get_path (model, &iter);
	priv->drag_row = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);

	/* FIXME: set a drag icon? */
}

static void
tree_drag_data_delete(GtkWidget *widget, GdkDragContext *context, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	gchar *full_name = NULL;
	GtkTreePath *src_path;
	gboolean is_store;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;

	if (!priv->drag_row || (src_path = gtk_tree_row_reference_get_path (priv->drag_row)))
		return;

	if (!gtk_tree_model_get_iter((GtkTreeModel *)priv->model, &iter, src_path))
		goto fail;

	gtk_tree_model_get((GtkTreeModel *)priv->model, &iter,
			   COL_POINTER_CAMEL_STORE, &store,
			   COL_STRING_FULL_NAME, &full_name,
			   COL_BOOL_IS_STORE, &is_store, -1);

	if (is_store)
		goto fail;

	camel_exception_init(&ex);
	camel_store_delete_folder(store, full_name, &ex);
	if (camel_exception_is_set(&ex))
		camel_exception_clear(&ex);
fail:
	gtk_tree_path_free(src_path);
	g_free (full_name);
}

static void
tree_drag_data_get(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection, guint info, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	gchar *full_name = NULL, *uri = NULL;
	GtkTreePath *src_path;
	CamelFolder *folder;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;

	if (!priv->drag_row || !(src_path = gtk_tree_row_reference_get_path(priv->drag_row)))
		return;

	if (!gtk_tree_model_get_iter((GtkTreeModel *)priv->model, &iter, src_path))
		goto fail;

	gtk_tree_model_get((GtkTreeModel *)priv->model, &iter,
			   COL_POINTER_CAMEL_STORE, &store,
			   COL_STRING_FULL_NAME, &full_name,
			   COL_STRING_URI, &uri, -1);

	/* make sure user isn't trying to drag on a placeholder row */
	if (full_name == NULL)
		goto fail;

	camel_exception_init(&ex);

	switch (info) {
	case DND_DRAG_TYPE_FOLDER:
		/* dragging to a new location in the folder tree */
		gtk_selection_data_set(selection, drag_atoms[info], 8, (guchar *)uri, strlen (uri) + 1);
		break;
	case DND_DRAG_TYPE_TEXT_URI_LIST:
		/* dragging to nautilus or something, probably */
		if ((folder = camel_store_get_folder(store, full_name, 0, &ex))) {
			GPtrArray *uids = camel_folder_get_uids(folder);

			em_utils_selection_set_urilist(selection, folder, uids);
			camel_folder_free_uids(folder, uids);
			camel_object_unref(folder);
		}
		break;
	default:
		abort();
	}

	if (camel_exception_is_set(&ex))
		camel_exception_clear(&ex);
fail:
	gtk_tree_path_free(src_path);
	g_free (full_name);
	g_free (uri);
}

/* TODO: Merge the drop handling code/menu's into one spot using a popup target for details */
/* Drop handling */
struct _DragDataReceivedAsync {
	MailMsg base;

	/* input data */
	GdkDragContext *context;

	/* Only selection->data and selection->length are valid */
	GtkSelectionData *selection;

	CamelStore *store;
	gchar *full_name;
	guint32 action;
	guint info;

	guint move:1;
	guint moved:1;
	guint aborted:1;
};

static void
emft_drop_folder(struct _DragDataReceivedAsync *m)
{
	CamelFolder *src;

	d(printf(" * Drop folder '%s' onto '%s'\n", m->selection->data, m->full_name));

	if (!(src = mail_tool_uri_to_folder((gchar *)m->selection->data, 0, &m->base.ex)))
		return;

	em_folder_utils_copy_folders(src->parent_store, src->full_name, m->store, m->full_name?m->full_name:"", m->move);
	camel_object_unref(src);
}

static gchar *
emft_drop_async__desc (struct _DragDataReceivedAsync *m)
{
	CamelURL *url;
	gchar *buf;

	if (m->info == DND_DROP_TYPE_FOLDER) {
		url = camel_url_new ((gchar *)m->selection->data, NULL);

		if (m->move)
			buf = g_strdup_printf (_("Moving folder %s"), url->fragment ? url->fragment : url->path + 1);
		else
			buf = g_strdup_printf (_("Copying folder %s"), url->fragment ? url->fragment : url->path + 1);

		camel_url_free (url);

		return buf;
	} else {
		if (m->move)
			return g_strdup_printf (_("Moving messages into folder %s"), m->full_name);
		else
			return g_strdup_printf (_("Copying messages into folder %s"), m->full_name);
	}
}

static void
emft_drop_async__exec (struct _DragDataReceivedAsync *m)
{
	CamelFolder *folder;

	/* for types other than folder, we can't drop to the root path */
	if (m->info == DND_DROP_TYPE_FOLDER) {
		/* copy or move (aka rename) a folder */
		emft_drop_folder(m);
	} else if (m->full_name == NULL) {
		camel_exception_set (&m->base.ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot drop message(s) into toplevel store"));
	} else if ((folder = camel_store_get_folder (m->store, m->full_name, 0, &m->base.ex))) {
		switch (m->info) {
		case DND_DROP_TYPE_UID_LIST:
			/* import a list of uids from another evo folder */
			em_utils_selection_get_uidlist(m->selection, folder, m->move, &m->base.ex);
			m->moved = m->move && !camel_exception_is_set(&m->base.ex);
			break;
		case DND_DROP_TYPE_MESSAGE_RFC822:
			/* import a message/rfc822 stream */
			em_utils_selection_get_message(m->selection, folder);
			break;
		case DND_DROP_TYPE_TEXT_URI_LIST:
			/* import an mbox, maildir, or mh folder? */
			em_utils_selection_get_urilist(m->selection, folder);
			break;
		default:
			abort();
		}
		camel_object_unref(folder);
	}
}

static void
emft_drop_async__free (struct _DragDataReceivedAsync *m)
{
	g_object_unref(m->context);
	camel_object_unref(m->store);
	g_free(m->full_name);

	g_free(m->selection->data);
	g_free(m->selection);
}

static MailMsgInfo emft_drop_async_info = {
	sizeof (struct _DragDataReceivedAsync),
	(MailMsgDescFunc) emft_drop_async__desc,
	(MailMsgExecFunc) emft_drop_async__exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) emft_drop_async__free
};

static void
tree_drag_data_action(struct _DragDataReceivedAsync *m)
{
	m->move = m->action == GDK_ACTION_MOVE;
	mail_msg_unordered_push (m);
}

static void
emft_drop_popup_copy(EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _DragDataReceivedAsync *m = data;

	m->action = GDK_ACTION_COPY;
	tree_drag_data_action(m);
}

static void
emft_drop_popup_move(EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _DragDataReceivedAsync *m = data;

	m->action = GDK_ACTION_MOVE;
	tree_drag_data_action(m);
}

static void
emft_drop_popup_cancel(EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _DragDataReceivedAsync *m = data;

	m->aborted = TRUE;
	mail_msg_unref(m);
}

static EPopupItem emft_drop_popup_menu[] = {
	{ E_POPUP_ITEM, (gchar *) "00.emc.00", (gchar *) N_("_Copy to Folder"), emft_drop_popup_copy, NULL, NULL, 1 },
	{ E_POPUP_ITEM, (gchar *) "00.emc.01", (gchar *) N_("_Move to Folder"), emft_drop_popup_move, NULL, NULL, 1 },
	{ E_POPUP_ITEM, (gchar *) "00.emc.02", (gchar *) N_("_Copy"), emft_drop_popup_copy, NULL, (gchar *) "folder-copy", 2 },
	{ E_POPUP_ITEM, (gchar *) "00.emc.03", (gchar *) N_("_Move"), emft_drop_popup_move, NULL, (gchar *) "folder-move", 2 },
	{ E_POPUP_BAR, (gchar *) "10.emc" },
	{ E_POPUP_ITEM, (gchar *) "99.emc.00", (gchar *) N_("Cancel _Drag"), emft_drop_popup_cancel, NULL, (gchar *) "dialog-cancel", 0 },
};

static void
emft_drop_popup_free(EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free(items);
}

static void
tree_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selection, guint info, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewDropPosition pos;
	GtkTreePath *dest_path;
	struct _DragDataReceivedAsync *m;
	gboolean is_store;
	CamelStore *store;
	GtkTreeIter iter;
	gchar *full_name;
	gint i;

	if (!gtk_tree_view_get_dest_row_at_pos (priv->treeview, x, y, &dest_path, &pos))
		return;

	/* this means we are receiving no data */
	if (!selection->data || selection->length == -1) {
		gtk_drag_finish(context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}

	if (!gtk_tree_model_get_iter((GtkTreeModel *)priv->model, &iter, dest_path)) {
		gtk_drag_finish(context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}

	gtk_tree_model_get((GtkTreeModel *)priv->model, &iter,
			   COL_POINTER_CAMEL_STORE, &store,
			   COL_BOOL_IS_STORE, &is_store,
			   COL_STRING_FULL_NAME, &full_name, -1);

	/* make sure user isn't try to drop on a placeholder row */
	if (full_name == NULL && !is_store) {
		gtk_drag_finish (context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}

	m = mail_msg_new (&emft_drop_async_info);
	m->context = context;
	g_object_ref(context);
	m->store = store;
	camel_object_ref(store);
	m->full_name = full_name;
	m->action = context->action;
	m->info = info;

	/* need to copy, goes away once we exit */
	m->selection = g_malloc0(sizeof(*m->selection));
	m->selection->data = g_malloc(selection->length);
	memcpy(m->selection->data, selection->data, selection->length);
	m->selection->length = selection->length;

	if (context->action == GDK_ACTION_ASK) {
		EMPopup *emp;
		gint mask;
		GSList *menus = NULL;
		GtkMenu *menu;

		emp = em_popup_new("org.gnome.mail.storageset.popup.drop");
		if (info != DND_DROP_TYPE_FOLDER)
			mask = ~1;
		else
			mask = ~2;

		for (i=0;i<sizeof(emft_drop_popup_menu)/sizeof(emft_drop_popup_menu[0]);i++) {
			EPopupItem *item = &emft_drop_popup_menu[i];

			if ((item->visible & mask) == 0)
				menus = g_slist_append(menus, item);
		}
		e_popup_add_items((EPopup *)emp, menus, NULL, emft_drop_popup_free, m);
		menu = e_popup_create_menu_once((EPopup *)emp, NULL, mask);
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
	} else {
		tree_drag_data_action(m);
	}
}

static gboolean
is_special_local_folder (const gchar *name)
{
	return (!strcmp (name, "Drafts") || !strcmp (name, "Inbox") || !strcmp (name, "Outbox") || !strcmp (name, "Sent") || !strcmp (name, "Templates"));
}

static GdkAtom
emft_drop_target(EMFolderTree *emft, GdkDragContext *context, GtkTreePath *path)
{
	struct _EMFolderTreePrivate *p = emft->priv;
	gchar *full_name = NULL, *uri = NULL, *src_uri = NULL;
	CamelStore *local, *sstore, *dstore;
	GdkAtom atom = GDK_NONE;
	gboolean is_store;
	GtkTreeIter iter;
	GList *targets;
	guint32 flags = 0;

	/* This is a bit of a mess, but should handle all the cases properly */

	if (!gtk_tree_model_get_iter((GtkTreeModel *)p->model, &iter, path))
		return GDK_NONE;

	gtk_tree_model_get((GtkTreeModel *)p->model, &iter, COL_BOOL_IS_STORE, &is_store,
			   COL_STRING_FULL_NAME, &full_name,
			   COL_UINT_FLAGS, &flags,
			   COL_POINTER_CAMEL_STORE, &dstore,
			   COL_STRING_URI, &uri, -1);

	local = mail_component_peek_local_store (NULL);

	targets = context->targets;

	/* Check for special destinations */
	if (uri && full_name) {
#if 0
		/* only allow copying/moving folders (not messages) into the local Outbox */
		if (dstore == local && !strcmp (full_name, "Outbox")) {
			GdkAtom xfolder;

			xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
			while (targets != NULL) {
				if (targets->data == (gpointer) xfolder) {
					atom = xfolder;
					goto done;
				}

				targets = targets->next;
			}

			goto done;
		}
#endif

		/* don't allow copying/moving into the UNMATCHED vfolder */
		if (!strncmp (uri, "vfolder:", 8) && !strcmp (full_name, CAMEL_UNMATCHED_NAME))
			goto done;

		/* don't allow copying/moving into a vTrash/vJunk folder */
		if (!strcmp (full_name, CAMEL_VTRASH_NAME)
		    || !strcmp (full_name, CAMEL_VJUNK_NAME))
			goto done;

		if (flags & CAMEL_FOLDER_NOSELECT)
			goto done;
	}

	if (p->drag_row) {
		GtkTreePath *src_path = gtk_tree_row_reference_get_path(p->drag_row);

		if (src_path) {
			if (gtk_tree_model_get_iter((GtkTreeModel *)p->model, &iter, src_path))
				gtk_tree_model_get((GtkTreeModel *)p->model, &iter,
						   COL_POINTER_CAMEL_STORE, &sstore,
						   COL_STRING_URI, &src_uri, -1);

			/* can't dnd onto itself or below itself - bad things happen,
			   no point dragging to where we were either */
			if (gtk_tree_path_compare(path, src_path) == 0
			    || gtk_tree_path_is_descendant(path, src_path)
			    || (gtk_tree_path_is_ancestor(path, src_path)
				&& gtk_tree_path_get_depth(path) == gtk_tree_path_get_depth(src_path)-1)) {
				gtk_tree_path_free(src_path);
				goto done;
			}

			gtk_tree_path_free(src_path);
		}
	}

	/* Check for special sources, and vfolder stuff */
	if (src_uri) {
		CamelURL *url;
		gchar *url_path;

		/* FIXME: this is a total hack, but i think all we can do at present */
		/* Check for dragging from special folders which can't be moved/copied */
		url = camel_url_new(src_uri, NULL);
		url_path = url->fragment?url->fragment:url->path;
		if (url_path && url_path[0]) {
			/* don't allow moving any of the the local special folders */
			if (sstore == local && is_special_local_folder (url_path)) {
				GdkAtom xfolder;

				camel_url_free (url);

				/* TODO: not sure if this is legal, but it works, force copy for special local folders */
				context->suggested_action = GDK_ACTION_COPY;
				xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
				while (targets != NULL) {
					if (targets->data == (gpointer) xfolder) {
						atom = xfolder;
						goto done;
					}

					targets = targets->next;
				}

				goto done;
			}

			/* don't allow copying/moving of the UNMATCHED vfolder */
			if (!strcmp (url->protocol, "vfolder") && !strcmp (url_path, CAMEL_UNMATCHED_NAME)) {
				camel_url_free (url);
				goto done;
			}

			/* don't allow copying/moving of any vTrash/vJunk folder nor maildir 'inbox' */
			if (strcmp(url_path, CAMEL_VTRASH_NAME) == 0
			    || strcmp(url_path, CAMEL_VJUNK_NAME) == 0
			    /* Dont allow drag from maildir 'inbox' */
			    || strcmp(url_path, ".") == 0) {
				camel_url_free(url);
				goto done;
			}
		}
		camel_url_free(url);

		/* Search Folders can only be dropped into other Search Folders */
		if (strncmp(src_uri, "vfolder:", 8) == 0) {
			/* TODO: not sure if this is legal, but it works, force move only for vfolders */
			context->suggested_action = GDK_ACTION_MOVE;

			if (uri && strncmp(uri, "vfolder:", 8) == 0) {
				GdkAtom xfolder;

				xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
				while (targets != NULL) {
					if (targets->data == (gpointer) xfolder) {
						atom = xfolder;
						goto done;
					}

					targets = targets->next;
				}
			}

			goto done;
		}
	}

	/* can't drag anything but a vfolder into a vfolder */
	if (uri && strncmp(uri, "vfolder:", 8) == 0)
		goto done;

	/* Now we either have a store or a normal folder */

	if (is_store) {
		GdkAtom xfolder;

		xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
		while (targets != NULL) {
			if (targets->data == (gpointer) xfolder) {
				atom = xfolder;
				goto done;
			}

			targets = targets->next;
		}
	} else {
		gint i;

		while (targets != NULL) {
			for (i = 0; i < NUM_DROP_TYPES; i++) {
				if (targets->data == (gpointer) drop_atoms[i]) {
					atom = drop_atoms[i];
					goto done;
				}
			}

			targets = targets->next;
		}
	}

 done:

	g_free (full_name);
	g_free (uri);

	return atom;
}

static gboolean
tree_drag_drop (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewColumn *column;
	gint cell_x, cell_y;
	GtkTreePath *path;
	GdkAtom target;

	if (priv->autoscroll_id != 0) {
		g_source_remove (priv->autoscroll_id);
		priv->autoscroll_id = 0;
	}

	if (priv->autoexpand_id != 0) {
		gtk_tree_row_reference_free (priv->autoexpand_row);
		priv->autoexpand_row = NULL;

		g_source_remove (priv->autoexpand_id);
		priv->autoexpand_id = 0;
	}

	if (!gtk_tree_view_get_path_at_pos (priv->treeview, x, y, &path, &column, &cell_x, &cell_y))
		return FALSE;

	target = emft_drop_target(emft, context, path);
	gtk_tree_path_free (path);
	if (target == GDK_NONE)
		return FALSE;

	return TRUE;
}

static void
tree_drag_end (GtkWidget *widget, GdkDragContext *context, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;

	if (priv->drag_row) {
		gtk_tree_row_reference_free (priv->drag_row);
		priv->drag_row = NULL;
	}

	/* FIXME: undo anything done in drag-begin */
}

static void
tree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;

	if (priv->autoscroll_id != 0) {
		g_source_remove (priv->autoscroll_id);
		priv->autoscroll_id = 0;
	}

	if (priv->autoexpand_id != 0) {
		gtk_tree_row_reference_free (priv->autoexpand_row);
		priv->autoexpand_row = NULL;

		g_source_remove (priv->autoexpand_id);
		priv->autoexpand_id = 0;
	}

	gtk_tree_view_set_drag_dest_row(emft->priv->treeview, NULL, GTK_TREE_VIEW_DROP_BEFORE);
}

#define SCROLL_EDGE_SIZE 15

static gboolean
tree_autoscroll (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkAdjustment *vadjustment;
	GdkRectangle rect;
	GdkWindow *window;
	gint offset, y;
	gfloat value;

	/* get the y pointer position relative to the treeview */
	window = gtk_tree_view_get_bin_window (priv->treeview);
	gdk_window_get_pointer (window, NULL, &y, NULL);

	/* rect is in coorinates relative to the scrolled window relative to the treeview */
	gtk_tree_view_get_visible_rect (priv->treeview, &rect);

	/* move y into the same coordinate system as rect */
	y += rect.y;

	/* see if we are near the top edge */
	if ((offset = y - (rect.y + 2 * SCROLL_EDGE_SIZE)) > 0) {
		/* see if we are near the bottom edge */
		if ((offset = y - (rect.y + rect.height - 2 * SCROLL_EDGE_SIZE)) < 0)
			return TRUE;
	}

	vadjustment = gtk_tree_view_get_vadjustment (priv->treeview);

	value = CLAMP (vadjustment->value + offset, 0.0, vadjustment->upper - vadjustment->page_size);
	gtk_adjustment_set_value (vadjustment, value);

	return TRUE;
}

static gboolean
tree_autoexpand (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreePath *path;

	path = gtk_tree_row_reference_get_path (priv->autoexpand_row);
	gtk_tree_view_expand_row (priv->treeview, path, FALSE);
	gtk_tree_path_free (path);

	return TRUE;
}

static gboolean
tree_drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeModel *model = (GtkTreeModel *) priv->model;
	GtkTreeViewDropPosition pos;
	GdkDragAction action = 0;
	GtkTreePath *path;
	GtkTreeIter iter;
	GdkAtom target;
	gint i;

	if (!gtk_tree_view_get_dest_row_at_pos(priv->treeview, x, y, &path, &pos))
		return FALSE;

	if (priv->autoscroll_id == 0)
		priv->autoscroll_id = g_timeout_add (150, (GSourceFunc) tree_autoscroll, emft);

	gtk_tree_model_get_iter (model, &iter, path);

	if (gtk_tree_model_iter_has_child (model, &iter) && !gtk_tree_view_row_expanded (priv->treeview, path)) {
		if (priv->autoexpand_id != 0) {
			GtkTreePath *autoexpand_path;

			autoexpand_path = gtk_tree_row_reference_get_path (priv->autoexpand_row);
			if (gtk_tree_path_compare (autoexpand_path, path) != 0) {
				/* row changed, restart timer */
				gtk_tree_row_reference_free (priv->autoexpand_row);
				priv->autoexpand_row = gtk_tree_row_reference_new (model, path);
				g_source_remove (priv->autoexpand_id);
				priv->autoexpand_id = g_timeout_add (600, (GSourceFunc) tree_autoexpand, emft);
			}

			gtk_tree_path_free (autoexpand_path);
		} else {
			priv->autoexpand_id = g_timeout_add (600, (GSourceFunc) tree_autoexpand, emft);
			priv->autoexpand_row = gtk_tree_row_reference_new (model, path);
		}
	} else if (priv->autoexpand_id != 0) {
		gtk_tree_row_reference_free (priv->autoexpand_row);
		priv->autoexpand_row = NULL;

		g_source_remove (priv->autoexpand_id);
		priv->autoexpand_id = 0;
	}

	target = emft_drop_target(emft, context, path);
	if (target != GDK_NONE) {
		for (i=0; i<NUM_DROP_TYPES; i++) {
			if (drop_atoms[i] == target) {
				switch (i) {
				case DND_DROP_TYPE_UID_LIST:
				case DND_DROP_TYPE_FOLDER:
					action = context->suggested_action;
					if (action == GDK_ACTION_COPY && (context->actions & GDK_ACTION_MOVE))
						action = GDK_ACTION_MOVE;
					gtk_tree_view_set_drag_dest_row(priv->treeview, path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
					break;
				default:
					gtk_tree_view_set_drag_dest_row(priv->treeview, path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
					action = context->suggested_action;
					break;
				}
				break;
			}
		}
	}

	gtk_tree_path_free(path);

	gdk_drag_status(context, action, time);

	return action != 0;
}

void
em_folder_tree_enable_drag_and_drop (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv;
	static gint setup = 0;
	gint i;

	g_return_if_fail (EM_IS_FOLDER_TREE (emft));

	priv = emft->priv;
	if (!setup) {
		for (i=0; i<NUM_DRAG_TYPES; i++)
			drag_atoms[i] = gdk_atom_intern(drag_types[i].target, FALSE);

		for (i=0; i<NUM_DROP_TYPES; i++)
			drop_atoms[i] = gdk_atom_intern(drop_types[i].target, FALSE);

		setup = 1;
	}

	gtk_drag_source_set((GtkWidget *)priv->treeview, GDK_BUTTON1_MASK, drag_types, NUM_DRAG_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_ASK);
	gtk_drag_dest_set((GtkWidget *)priv->treeview, GTK_DEST_DEFAULT_ALL, drop_types, NUM_DROP_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_ASK);

	g_signal_connect (priv->treeview, "drag-begin", G_CALLBACK (tree_drag_begin), emft);
	g_signal_connect (priv->treeview, "drag-data-delete", G_CALLBACK (tree_drag_data_delete), emft);
	g_signal_connect (priv->treeview, "drag-data-get", G_CALLBACK (tree_drag_data_get), emft);
	g_signal_connect (priv->treeview, "drag-data-received", G_CALLBACK (tree_drag_data_received), emft);
	g_signal_connect (priv->treeview, "drag-drop", G_CALLBACK (tree_drag_drop), emft);
	g_signal_connect (priv->treeview, "drag-end", G_CALLBACK (tree_drag_end), emft);
	g_signal_connect (priv->treeview, "drag-leave", G_CALLBACK (tree_drag_leave), emft);
	g_signal_connect (priv->treeview, "drag-motion", G_CALLBACK (tree_drag_motion), emft);
}

void
em_folder_tree_set_multiselect (EMFolderTree *tree, gboolean mode)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection ((GtkTreeView *) tree->priv->treeview);

	tree->priv->do_multiselect = mode;
	gtk_tree_selection_set_mode (sel, mode ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_SINGLE);
}

void em_folder_tree_set_excluded(EMFolderTree *emft, guint32 flags)
{
	emft->priv->excluded = flags;
}

void em_folder_tree_set_excluded_func(EMFolderTree *emft, EMFTExcludeFunc exclude, gpointer data)
{
	emft->priv->excluded_func = exclude;
	emft->priv->excluded_data = data;
}

GList *
em_folder_tree_get_selected_uris (EMFolderTree *emft)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection (emft->priv->treeview);
	GList *list = NULL, *rows, *l;
	GSList *sl;
	GtkTreeModel *model;

	/* at first, add lost uris */
	for (sl = emft->priv->select_uris; sl; sl = g_slist_next(sl))
		list = g_list_append (list, g_strdup (((struct _selected_uri *)sl->data)->uri));

	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	for (l=rows; l; l=g_list_next(l)) {
		GtkTreeIter iter;
		GtkTreePath *path = l->data;

		if (gtk_tree_model_get_iter(model, &iter, path)) {
			gchar *uri;

			gtk_tree_model_get(model, &iter, COL_STRING_URI, &uri, -1);
			list = g_list_prepend (list, uri);
		}
		gtk_tree_path_free(path);
	}
	g_list_free(rows);

	return g_list_reverse (list);
}

static void
get_selected_uris_path_iterate (GtkTreeModel *model, GtkTreePath *treepath, GtkTreeIter *iter, gpointer data)
{
	GList **list = (GList **) data;
	gchar *full_name;

	gtk_tree_model_get (model, iter, COL_STRING_FULL_NAME, &full_name, -1);
	*list = g_list_append (*list, full_name);
}

GList *
em_folder_tree_get_selected_paths (EMFolderTree *emft)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection (emft->priv->treeview);
	GList *list = NULL;

	gtk_tree_selection_selected_foreach (selection, get_selected_uris_path_iterate, &list);

	return list;
}

static void
emft_clear_selected_list(EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;

	g_slist_foreach(priv->select_uris, emft_free_select_uri, emft);
	g_slist_free(priv->select_uris);
	g_hash_table_destroy(priv->select_uris_table);
	priv->select_uris = NULL;
	priv->select_uris_table = g_hash_table_new(g_str_hash, g_str_equal);
	priv->cursor_set = FALSE;
}

void
em_folder_tree_set_selected_list (EMFolderTree *emft, GList *list, gboolean expand_only)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	gint id = 0;

	/* FIXME: need to remove any currently selected stuff? */
	if (!expand_only)
		emft_clear_selected_list(emft);

	for (;list;list = list->next) {
		struct _selected_uri *u = g_malloc0(sizeof(*u));
		CamelURL *url;
		CamelException ex = { 0 };

		u->uri = g_strdup(list->data);
		u->store = (CamelStore *)camel_session_get_service (session, u->uri, CAMEL_PROVIDER_STORE, &ex);
		camel_exception_clear(&ex);

		url = camel_url_new(u->uri, NULL);
		if (u->store == NULL || url == NULL) {
			if (!expand_only) {
				u->key = g_strdup_printf("dummy-%d:%s", id++, u->uri);
				g_hash_table_insert(priv->select_uris_table, u->key, u);
				priv->select_uris = g_slist_append(priv->select_uris, u);
			}
		} else {
			const gchar *path;
			gchar *expand_key, *end;
			EAccount *account;

			if (((CamelService *)u->store)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
				path = url->fragment;
			else
				path = url->path && url->path[0]=='/' ? url->path+1:url->path;
			if (path == NULL)
				path = "";

			/* This makes sure all our parents up to the root are expanded */
			/* FIXME: Why does the expanded state store this made up path rather than the euri? */
			if ( (account = mail_config_get_account_by_source_url(u->uri)) )
				expand_key = g_strdup_printf ("%s/%s", account->uid, path);
			else if (CAMEL_IS_VEE_STORE (u->store))
				expand_key = g_strdup_printf ("vfolder/%s", path);
			else
				expand_key = g_strdup_printf ("local/%s", path);

			if (!expand_only) {
				u->key = g_strdup(expand_key);

				g_hash_table_insert(priv->select_uris_table, u->key, u);
				priv->select_uris = g_slist_append(priv->select_uris, u);
			}

			end = strrchr(expand_key, '/');
			do {
				emft_expand_node(priv->model, expand_key, emft);
				em_folder_tree_model_set_expanded(priv->model, expand_key, TRUE);
				*end = 0;
				end = strrchr(expand_key, '/');
			} while (end);
			g_free(expand_key);
		}

		if (url)
			camel_url_free(url);
	}
}

#if 0
static void
dump_fi (CamelFolderInfo *fi, gint depth)
{
	gint i;

	while (fi != NULL) {
		for (i = 0; i < depth; i++)
			fputs ("  ", stdout);

		printf ("path='%s'; full_name='%s'\n", fi->path, fi->full_name);

		if (fi->child)
			dump_fi (fi->child, depth + 1);

		fi = fi->sibling;
	}
}
#endif

struct _EMFolderTreeGetFolderInfo {
	MailMsg base;

	/* input data */
	GtkTreeRowReference *root;
	EMFolderTree *emft;
	CamelStore *store;
	guint32 flags;
	gchar *top;

	/* output data */
	CamelFolderInfo *fi;
};

static gchar *
emft_get_folder_info__desc (struct _EMFolderTreeGetFolderInfo *m)
{
	gchar *ret, *name;

	name = camel_service_get_name((CamelService *)m->store, TRUE);
	ret = g_strdup_printf(_("Scanning folders in \"%s\""), name);
	g_free(name);
	return ret;
}

static void
emft_get_folder_info__exec (struct _EMFolderTreeGetFolderInfo *m)
{
	guint32 flags = m->flags | CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;

	m->fi = camel_store_get_folder_info (m->store, m->top, flags, &m->base.ex);
}

static void
emft_get_folder_info__done (struct _EMFolderTreeGetFolderInfo *m)
{
	struct _EMFolderTreePrivate *priv = m->emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeIter root, iter, titer;
	CamelFolderInfo *fi;
	GtkTreeStore *model;
	GtkTreePath *path;
	gboolean is_store;

	/* check that we haven't been destroyed */
	if (priv->treeview == NULL)
		return;

	/* check that our parent folder hasn't been deleted/unsubscribed */
	if (!gtk_tree_row_reference_valid (m->root))
		return;

	if (!(si = g_hash_table_lookup (priv->model->store_hash, m->store))) {
		/* store has been removed in the interim - do nothing */
		return;
	}

	model = (GtkTreeStore *) gtk_tree_view_get_model (priv->treeview);

	path = gtk_tree_row_reference_get_path (m->root);
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &root, path);

	/* if we had an error, then we need to re-set the load subdirs state and collapse the node */
	if (!m->fi && camel_exception_is_set(&m->base.ex)) {
		gtk_tree_store_set(model, &root, COL_BOOL_LOAD_SUBDIRS, TRUE, -1);
		gtk_tree_view_collapse_row (priv->treeview, path);
		gtk_tree_path_free (path);
		return;
	}

	gtk_tree_path_free (path);

	/* make sure we still need to load the tree subfolders... */
	gtk_tree_model_get ((GtkTreeModel *) model, &root,
			    COL_BOOL_IS_STORE, &is_store,
			    -1);

	/* get the first child (which will be a dummy node) */
	gtk_tree_model_iter_children ((GtkTreeModel *) model, &iter, &root);

	/* Traverse to the last valid iter */
	titer = iter;
	while (gtk_tree_model_iter_next((GtkTreeModel *) model, &iter))
		titer = iter; /* Preserve the last valid iter */

	iter = titer;

	/* FIXME: camel's IMAP code is totally on crack here, @top's
	 * folder info should be @fi and fi->child should be what we
	 * want to fill our tree with... *sigh* */
	if (m->top && m->fi && !strcmp (m->fi->full_name, m->top)) {
		if (!(fi = m->fi->child))
			fi = m->fi->next;
	} else
		fi = m->fi;

	if (fi == NULL) {
		/* no children afterall... remove the "Loading..." placeholder node */
		emft_update_model_expanded_state (priv, &root, FALSE);

		gtk_tree_store_remove (model, &iter);

		if (is_store) {
			path = gtk_tree_model_get_path ((GtkTreeModel *) model, &root);
			gtk_tree_view_collapse_row (priv->treeview, path);
			emft_queue_save_state (m->emft);
			gtk_tree_path_free (path);
			return;
		}
	} else {
		gint fully_loaded = (m->flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) ? TRUE : FALSE;

		do {
			gboolean known = g_hash_table_lookup (si->full_hash, fi->full_name) != NULL;

			if (!known)
				em_folder_tree_model_set_folder_info (priv->model, &iter, si, fi, fully_loaded);

			if ((fi = fi->next) != NULL && !known)
				gtk_tree_store_append (model, &iter, &root);
		} while (fi != NULL);
	}

	gtk_tree_store_set (model, &root, COL_BOOL_LOAD_SUBDIRS, FALSE, -1);
	emft_queue_save_state (m->emft);
}

static void
emft_get_folder_info__free (struct _EMFolderTreeGetFolderInfo *m)
{
	camel_store_free_folder_info (m->store, m->fi);

	gtk_tree_row_reference_free (m->root);
	g_object_unref(m->emft);
	camel_object_unref (m->store);
	g_free (m->top);
}

static MailMsgInfo get_folder_info_info = {
	sizeof (struct _EMFolderTreeGetFolderInfo),
	(MailMsgDescFunc) emft_get_folder_info__desc,
	(MailMsgExecFunc) emft_get_folder_info__exec,
	(MailMsgDoneFunc) emft_get_folder_info__done,
	(MailMsgFreeFunc) emft_get_folder_info__free
};

static void
emft_update_model_expanded_state (struct _EMFolderTreePrivate *priv, GtkTreeIter *iter, gboolean expanded)
{
	struct _EMFolderTreeModelStoreInfo *si;
	gboolean is_store;
	CamelStore *store;
	EAccount *account;
	gchar *full_name;
	gchar *key;

	gtk_tree_model_get ((GtkTreeModel *) priv->model, iter,
			    COL_STRING_FULL_NAME, &full_name,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_BOOL_IS_STORE, &is_store,
			    -1);

	si = g_hash_table_lookup (priv->model->store_hash, store);
	if ((account = mail_config_get_account_by_name (si->display_name))) {
		key = g_strdup_printf ("%s/%s", account->uid, full_name ? full_name : "");
	} else if (CAMEL_IS_VEE_STORE (store)) {
		/* vfolder store */
		key = g_strdup_printf ("vfolder/%s", full_name ? full_name : "");
	} else {
		/* local store */
		key = g_strdup_printf ("local/%s", full_name ? full_name : "");
	}

	em_folder_tree_model_set_expanded (priv->model, key, expanded);
	g_free (full_name);
	g_free (key);
}

static void
emft_tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *tree_path, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeGetFolderInfo *m;
	GtkTreeModel *model;
	CamelStore *store;
	gchar *full_name;
	gboolean load;

	model = gtk_tree_view_get_model (treeview);

	gtk_tree_model_get (model, root,
			    COL_STRING_FULL_NAME, &full_name,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_BOOL_LOAD_SUBDIRS, &load,
			    -1);

	emft_update_model_expanded_state (priv, root, TRUE);

	if (!load) {
		emft_queue_save_state (emft);
		g_free (full_name);
		return;
	}

	gtk_tree_store_set ((GtkTreeStore *)model, root, COL_BOOL_LOAD_SUBDIRS, FALSE, -1);

	m = mail_msg_new (&get_folder_info_info);
	m->root = gtk_tree_row_reference_new (model, tree_path);
	camel_object_ref (store);
	m->store = store;
	m->emft = emft;
	g_object_ref(emft);
	m->top = full_name;
	m->flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE|CAMEL_STORE_FOLDER_INFO_FAST;

	mail_msg_unordered_push (m);
}

static gboolean
emft_tree_test_collapse_row (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *tree_path, EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter cursor;

	selection = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_get_selected (selection, &model, &cursor)) {
		/* select the collapsed node IFF it is a parent of the currently selected folder */
		if (gtk_tree_store_is_ancestor ((GtkTreeStore *) model, root, &cursor))
			gtk_tree_view_set_cursor (treeview, tree_path, NULL, FALSE);
	}

	emft_update_model_expanded_state (emft->priv, root, FALSE);
	emft_queue_save_state (emft);

	return FALSE;
}

static void
emft_tree_row_activated (GtkTreeView *treeview, GtkTreePath *tree_path, GtkTreeViewColumn *column, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeModel *model = (GtkTreeModel *) priv->model;
	gchar *full_name, *uri;
	GtkTreeIter iter;
	guint32 flags;

	if (!emft_select_func(NULL, model, tree_path, FALSE, emft))
		return;

	if (!gtk_tree_model_get_iter (model, &iter, tree_path))
		return;

	gtk_tree_model_get (model, &iter, COL_STRING_FULL_NAME, &full_name,
			    COL_STRING_URI, &uri, COL_UINT_FLAGS, &flags, -1);

	emft_clear_selected_list(emft);

	g_signal_emit (emft, signals[FOLDER_SELECTED], 0, full_name, uri, flags);
	g_signal_emit (emft, signals[FOLDER_ACTIVATED], 0, full_name, uri);

	g_free(full_name);
	g_free(uri);
}

#if 0
static void
emft_popup_view (GtkWidget *item, EMFolderTree *emft)
{

}

static void
emft_popup_open_new (GtkWidget *item, EMFolderTree *emft)
{
}
#endif

static void
emft_popup_copy(EPopup *ep, EPopupItem *item, gpointer data)
{
	EMFolderTree *emft = data;
	CamelFolderInfo *fi = NULL;

	/* FIXME: use async apis */
	if ((fi = em_folder_tree_get_selected_folder_info (emft)) != NULL)
		em_folder_utils_copy_folder(fi, FALSE);
}

static void
emft_popup_move(EPopup *ep, EPopupItem *item, gpointer data)
{
	EMFolderTree *emft = data;
	CamelFolderInfo *fi = NULL;

	/* FIXME: use async apis */
	if ((fi = em_folder_tree_get_selected_folder_info (emft)) != NULL)
		em_folder_utils_copy_folder(fi, TRUE);
}

static void
emft_popup_new_folder (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderTree *emft = data;
	CamelFolderInfo *fi;

	if ((fi = em_folder_tree_get_selected_folder_info (emft)) != NULL) {
		em_folder_utils_create_folder (fi, emft, NULL);
		camel_folder_info_free(fi);
	}
}

static void
selfunc (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	struct _emft_selection_data *dat = (struct _emft_selection_data *) data;

	dat->model = model;
	if (!dat->set)
		*(dat->iter) = *iter;
	dat->set = TRUE;
}

static gboolean
emft_selection_get_selected (GtkTreeSelection *selection, GtkTreeModel **model, GtkTreeIter *iter)
{
	struct _emft_selection_data dat = { NULL, iter, FALSE };

	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE) {
		gtk_tree_selection_selected_foreach (selection, selfunc, &dat);
		if (model)
			*model = dat.model;
		return dat.set;
	} else {
		return gtk_tree_selection_get_selected (selection, model, iter);
	}
}

static void
emft_popup_delete_folder (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderTree *emft = data;
	CamelFolder *folder;

	if ((folder = em_folder_tree_get_selected_folder (emft)) != NULL) {
		em_folder_utils_delete_folder(folder);
	}
}

static void
emft_popup_rename_folder (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderTree *emft = data;
	CamelFolder *folder;

	if ((folder = em_folder_tree_get_selected_folder (emft)) != NULL) {
		em_folder_utils_rename_folder(folder);
	}
}

static void
emft_popup_refresh_folder (EPopup *ep, EPopupItem *pitem, gpointer data)
{
        EMFolderTree *emft = data;
        CamelFolder *folder;

        if ((folder = em_folder_tree_get_selected_folder (emft)) != NULL) {
		EMEvent *e = em_event_peek();
		EMEventTargetFolder *t = em_event_target_new_folder(e, folder->full_name, 0);
		e_event_emit((EEvent *)e, "folder.refresh", (EEventTarget *)t);
                mail_refresh_folder(folder, NULL, NULL);
	}
}

static void
emft_popup_flush_outbox (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	mail_send ();
}

static void
emft_popup_empty_trash (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderTree *emft = data;
	CamelFolder *folder;

	if ((folder = em_folder_tree_get_selected_folder (emft)) != NULL) {
		em_utils_expunge_folder (gtk_widget_get_toplevel ((GtkWidget *)emft), folder);
	}
}

static void
emft_popup_properties (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderTree *emft = data;
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *uri;

	selection = gtk_tree_view_get_selection (priv->treeview);
	if (!emft_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, -1);
	em_folder_properties_show (NULL, NULL, uri);
	g_free (uri);
}

static void
emft_popup_uvfolder (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderTree *emft = data;
	CamelFolder *folder;

	if ((folder = em_folder_tree_get_selected_folder (emft)) != NULL) {
		gchar *meta = camel_object_meta_get(folder, "vfolder:unread");
		if (!meta || strcmp(meta, "false") == 0)
			camel_object_meta_set(folder, "vfolder:unread", "true");
		else
			camel_object_meta_set(folder, "vfolder:unread", "false");
		camel_object_state_write (folder);
		g_free (meta);
	}
}

static EPopupItem emft_popup_items[] = {
#if 0
	{ E_POPUP_ITEM, "00.emc.00", N_("_View"), emft_popup_view, NULL, NULL, EM_POPUP_FOLDER_SELECT },
	{ E_POPUP_ITEM, "00.emc.01", N_("Open in _New Window"), emft_popup_open_new, NULL, NULL, EM_POPUP_FOLDER_SELECT },

	{ E_POPUP_BAR, "10.emc" },
#endif
	/* FIXME: need to disable for nochildren folders */
	{ E_POPUP_ITEM, (gchar *) "10.emc.00", (gchar *) N_("_New Folder..."), emft_popup_new_folder, NULL, (gchar *) "folder-new", 0, EM_POPUP_FOLDER_INFERIORS },

	{ E_POPUP_ITEM, (gchar *) "10.emc.05", (gchar *) N_("_Copy..."), emft_popup_copy, NULL, (gchar *) "folder-copy", 0, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_SELECT },
	{ E_POPUP_ITEM, (gchar *) "10.emc.06", (gchar *) N_("_Move..."), emft_popup_move, NULL, (gchar *) "folder-move", 0, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },

	/* FIXME: need to disable for undeletable folders */
	{ E_POPUP_BAR,  (gchar *) "20.emc" },
	{ E_POPUP_ITEM, (gchar *) "20.emc.01", (gchar *) N_("_Delete"), emft_popup_delete_folder, NULL, (gchar *) "edit-delete", 0, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },

	{ E_POPUP_BAR, (gchar *) "30.emc" },
	{ E_POPUP_ITEM, (gchar *) "30.emc.02", (gchar *) N_("_Rename..."), emft_popup_rename_folder, NULL, NULL, 0, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },
	{ E_POPUP_ITEM, (gchar *) "30.emc.03", (gchar *) N_("Re_fresh"), emft_popup_refresh_folder, NULL, (gchar *) "view-refresh", EM_POPUP_FOLDER_NONSTATIC, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_SELECT},
	{ E_POPUP_ITEM, (gchar *) "30.emc.04", (gchar *) N_("Fl_ush Outbox"), emft_popup_flush_outbox, NULL, (gchar *) "mail-send", EM_POPUP_FOLDER_OUTBOX, 0 },

	{ E_POPUP_BAR, (gchar *) "99.emc" },
	{ E_POPUP_ITEM, (gchar *) "99.emc.00", (gchar *) N_("_Properties"), emft_popup_properties, NULL, (gchar *) "document-properties", 0, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_SELECT }
};
static EPopupItem trash_popup_item = {E_POPUP_ITEM, (gchar *) "30.emc.05", (gchar *) N_("_Empty Trash"), emft_popup_empty_trash,NULL,NULL, 1, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_SELECT};

static void
emft_popup_free(EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free(items);
}

static gboolean
emft_popup (EMFolderTree *emft, GdkEvent *event)
{
	GtkTreeView *treeview;
	GtkTreeSelection *selection;
	CamelStore *local, *store;
	EMPopupTargetFolder *target;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *menus = NULL;
	guint32 info_flags = 0;
	guint32 flags = 0;
	guint32 folder_type_flags = 0;
	gboolean isstore;
	gchar *uri, *full_name;
	GtkMenu *menu;
	EMPopup *emp;
	CamelFolder *selfolder = NULL;
	gint i;

	treeview = emft->priv->treeview;

	/* this centralises working out when the user's done something */
	emft_tree_user_event(treeview, (GdkEvent *)event, emft);

	/* FIXME: we really need the folderinfo to build a proper menu */
	selection = gtk_tree_view_get_selection (treeview);
	if (!emft_selection_get_selected (selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get (model, &iter, COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_URI, &uri, COL_STRING_FULL_NAME, &full_name,
			    COL_BOOL_IS_STORE, &isstore, COL_UINT_FLAGS, &folder_type_flags, -1);

	/* Stores have full_name == NULL, otherwise its just a placeholder */
	/* NB: This is kind of messy */
	if (!isstore && full_name == NULL) {
		g_free (uri);
		return FALSE;
	}

	/* TODO: em_popup_target_folder_new? */
	if (isstore) {
		flags |= EM_POPUP_FOLDER_STORE;
	} else {
		flags |= EM_POPUP_FOLDER_FOLDER;

		local = mail_component_peek_local_store (NULL);

		/* don't allow deletion of special local folders */
		if (!(store == local && is_special_local_folder (full_name)))
			flags |= EM_POPUP_FOLDER_DELETE;

		/* hack for vTrash/vJunk */
		if (!strcmp (full_name, CAMEL_VTRASH_NAME) || !strcmp (full_name, CAMEL_VJUNK_NAME))
			info_flags |= CAMEL_FOLDER_VIRTUAL | CAMEL_FOLDER_NOINFERIORS;

		selfolder = em_folder_tree_get_selected_folder (emft);

		if (folder_type_flags & CAMEL_FOLDER_SYSTEM)
			flags &= ~EM_POPUP_FOLDER_DELETE;

		if (em_utils_folder_is_outbox (selfolder, NULL))
			info_flags |= CAMEL_FOLDER_TYPE_OUTBOX;
	}

	/** @HookPoint-EMPopup: Folder Tree Context Menu
	 * @Id: org.gnome.evolution.mail.foldertree.popup
	 * @Class: org.gnome.evolution.mail.popup:1.0
	 * @Target: EMPopupTargetFolder
	 *
	 * This is the context menu shown on the folder tree.
	 */
	emp = em_popup_new ("org.gnome.evolution.mail.foldertree.popup");

	/* FIXME: pass valid fi->flags here */
	target = em_popup_target_new_folder (emp, uri, info_flags, flags);

	for (i = 0; i < sizeof (emft_popup_items) / sizeof (emft_popup_items[0]); i++)
		menus = g_slist_prepend (menus, &emft_popup_items[i]);

	if ((folder_type_flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_TRASH)
		menus = g_slist_prepend (menus, &trash_popup_item);
	if (!isstore && strstr(uri, "vfolder")) {
		/* This is a vfolder, so lets add hacked up menu item. */
		static EPopupItem *item = NULL;
		gchar *meta = camel_object_meta_get (selfolder, "vfolder:unread");

		if (!item)
			item = g_malloc0(sizeof(*item));
		if (meta && (strcmp (meta, "true") == 0))
			item->type = E_POPUP_TOGGLE | E_POPUP_ACTIVE;
		else
			item->type = E_POPUP_TOGGLE & ~E_POPUP_ACTIVE;
		item->path = (gchar *) "99.emc.99";
		item->label = _("_Unread Search Folder");
		item->activate = emft_popup_uvfolder;
		item->visible = EM_POPUP_FOLDER_SELECT;
		item->user_data = NULL;
		menus = g_slist_prepend (menus, item);
		g_free (meta);
	}

	e_popup_add_items ((EPopup *)emp, menus, NULL, emft_popup_free, emft);

	menu = e_popup_create_menu_once ((EPopup *)emp, (EPopupTarget *)target, 0);

	if (event == NULL || event->type == GDK_KEY_PRESS) {
		/* FIXME: menu pos function */
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
	} else {
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button.button, event->button.time);
	}

	g_free (full_name);
	g_free (uri);

	return TRUE;
}

static gboolean
emft_popup_menu (GtkWidget *widget)
{
	return emft_popup (EM_FOLDER_TREE (widget), NULL);
}

static gboolean
emft_tree_button_press (GtkTreeView *treeview, GdkEventButton *event, EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreePath *tree_path;

	/* this centralises working out when the user's done something */
	emft_tree_user_event(treeview, (GdkEvent *)event, emft);

	if (event->button != 3 && !(event->button == 1 && event->type == GDK_2BUTTON_PRESS))
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos (treeview, (gint) event->x, (gint) event->y, &tree_path, NULL, NULL, NULL))
		return FALSE;

	/* select/focus the row that was right-clicked or double-clicked */
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_select_path(selection, tree_path);
	gtk_tree_view_set_cursor (treeview, tree_path, NULL, FALSE);

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		if (emft->priv->skip_double_click) {
			return FALSE;
		}
		emft_tree_row_activated (treeview, tree_path, NULL, emft);
		gtk_tree_path_free (tree_path);
		return TRUE;
	}

	gtk_tree_path_free (tree_path);

	return emft_popup (emft, (GdkEvent *)event);
}

/* This is called for keyboard and mouse events, it seems the only way
 * we know the user has done something to the selection as opposed to
 * code or initialisation processes */
static gboolean
emft_tree_user_event (GtkTreeView *treeview, GdkEvent *e, EMFolderTree *emft)
{
	if (e && e->type == GDK_KEY_PRESS && (e->key.keyval == GDK_space || e->key.keyval == '.' || e->key.keyval == ',' || e->key.keyval == '[' || e->key.keyval == ']')) {
		g_signal_emit (emft, signals [HIDDEN_KEY_EVENT], 0, e);

		return TRUE;
	}
	if (!emft->priv->do_multiselect)
		emft_clear_selected_list(emft);

	emft->priv->cursor_set = TRUE;

	return FALSE;
}

static void
emft_tree_selection_changed (GtkTreeSelection *selection, EMFolderTree *emft)
{
	gchar *full_name, *uri;
	GtkTreeModel *model;
	GtkTreeIter iter;
	guint32 flags;
	guint unread = 0;
	guint old_unread = 0;

	if (!emft_selection_get_selected (selection, &model, &iter)) {
		em_folder_tree_model_set_selected (emft->priv->model, NULL);
		g_signal_emit (emft, signals[FOLDER_SELECTED], 0, NULL, NULL, 0);
		emft_queue_save_state (emft);
		return;
	}

	gtk_tree_model_get (
		model, &iter,
		COL_STRING_FULL_NAME, &full_name,
		COL_STRING_URI, &uri, COL_UINT_FLAGS, &flags,
		COL_UINT_UNREAD, &unread, COL_UINT_UNREAD_LAST_SEL,
		&old_unread, -1);

	/* Sync unread counts to distinguish new incoming mail. */
	if (unread != old_unread)
		gtk_tree_store_set (
			GTK_TREE_STORE (model), &iter,
			COL_UINT_UNREAD_LAST_SEL, unread, -1);

	g_signal_emit (emft, signals[FOLDER_SELECTED], 0, full_name, uri, flags);
	g_free(uri);
	g_free(full_name);
}

void
em_folder_tree_set_selected (EMFolderTree *emft, const gchar *uri, gboolean expand_only)
{
	GList *l = NULL;

	if (uri && uri[0])
		l = g_list_append(l, (gpointer)uri);

	em_folder_tree_set_selected_list(emft, l, expand_only);
	g_list_free(l);
}

void
em_folder_tree_select_next_path (EMFolderTree *emft, gboolean skip_read_folders)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, parent, child;
	GtkTreePath *current_path, *path = NULL;
	guint unread = 0;
	struct _EMFolderTreePrivate *priv = emft->priv;

	g_return_if_fail (EM_IS_FOLDER_TREE (emft));

	selection = gtk_tree_view_get_selection(emft->priv->treeview);
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

		current_path = gtk_tree_model_get_path (model, &iter);

		do {
		if (gtk_tree_model_iter_has_child (model, &iter)) {
			gtk_tree_model_iter_children (model, &child, &iter);
			path = gtk_tree_model_get_path (model, &child);
			iter = child;
		} else {
			while (1) {
				gboolean has_parent = gtk_tree_model_iter_parent (model, &parent, &iter);
				if (gtk_tree_model_iter_next (model, &iter)) {
					path = gtk_tree_model_get_path (model, &iter);
					break;
				} else {
					if (has_parent) {
						iter =  parent;
					} else {
						/* Reached end. Wrapup*/
						gtk_tree_model_get_iter_first (model, &iter);
						path = gtk_tree_model_get_path (model, &iter);
						break;
					}
				}
			}
		}
		gtk_tree_model_get (model, &iter, COL_UINT_UNREAD, &unread, -1);

		/* TODO : Flags here for better options */
		} while (skip_read_folders && unread <=0 && gtk_tree_path_compare (current_path, path));
	}

	if (path) {
		if (!gtk_tree_view_row_expanded (emft->priv->treeview, path))
			gtk_tree_view_expand_to_path (emft->priv->treeview, path);

		gtk_tree_selection_select_path(selection, path);

		if (!priv->cursor_set) {
			gtk_tree_view_set_cursor (priv->treeview, path, NULL, FALSE);
			priv->cursor_set = TRUE;
		}
		gtk_tree_view_scroll_to_cell (priv->treeview, path, NULL, TRUE, 0.5f, 0.0f);
	}
	return;
}

static gboolean
folder_tree_descend (GtkTreeModel *model,
                     GtkTreeIter *iter,
                     GtkTreeIter *root)
{
	GtkTreeIter parent;
	gint n_children;

	/* Finds the rightmost descendant of the given root. */

	if (root == NULL) {
		n_children = gtk_tree_model_iter_n_children (model, NULL);

		/* This will invalidate the iterator and return FALSE. */
		if (n_children == 0)
			return gtk_tree_model_get_iter_first (model, iter);

		gtk_tree_model_iter_nth_child (
			model, &parent, NULL, n_children - 1);
	} else
		parent = *root;

	n_children = gtk_tree_model_iter_n_children (model, &parent);

	while (n_children > 0) {
		GtkTreeIter child;

		gtk_tree_model_iter_nth_child (
			model, &child, &parent, n_children - 1);

		parent = child;

		n_children = gtk_tree_model_iter_n_children (model, &parent);
	}

	*iter = parent;

	return TRUE;
}

void
em_folder_tree_select_prev_path (EMFolderTree *emft,
                                 gboolean skip_read_folders)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreePath *sentinel;
	GtkTreeIter iter;
	guint unread = 0;
	struct _EMFolderTreePrivate *priv = emft->priv;

	g_return_if_fail (EM_IS_FOLDER_TREE (emft));

	tree_view = emft->priv->treeview;
	selection = gtk_tree_view_get_selection(tree_view);

	/* Nothing selected means nothing to do. */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	/* This prevents us from looping over the model indefinitely,
	 * looking for unread messages when there are none. */
	sentinel = gtk_tree_model_get_path (model, &iter);

	do {
		GtkTreeIter descendant;

		if (path != NULL)
			gtk_tree_path_free (path);

		path = gtk_tree_model_get_path (model, &iter);

		if (gtk_tree_path_prev (path)) {
			gtk_tree_model_get_iter (model, &iter, path);
			folder_tree_descend (model, &descendant, &iter);

			gtk_tree_path_free (path);
			path = gtk_tree_model_get_path (model, &descendant);

		} else if (gtk_tree_path_get_depth (path) > 1) {
			gtk_tree_path_up (path);

		} else {
			folder_tree_descend (model, &descendant, NULL);

			gtk_tree_path_free (path);
			path = gtk_tree_model_get_path (model, &descendant);
		}

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, COL_UINT_UNREAD, &unread, -1);

	} while (skip_read_folders && unread <= 0 &&
		gtk_tree_path_compare (path, sentinel) != 0);

	if (!gtk_tree_view_row_expanded (tree_view, path))
		gtk_tree_view_expand_to_path (tree_view, path);

	gtk_tree_selection_select_path (selection, path);

	if (!priv->cursor_set) {
		gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
		priv->cursor_set = TRUE;
	}

	gtk_tree_view_scroll_to_cell (
		tree_view, path, NULL, TRUE, 0.5f, 0.0f);

	gtk_tree_path_free (sentinel);
	gtk_tree_path_free (path);
}

gchar *
em_folder_tree_get_selected_uri (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *uri = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	selection = gtk_tree_view_get_selection(emft->priv->treeview);
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get(model, &iter, COL_STRING_URI, &uri, -1);

	return uri;
}

gchar *
em_folder_tree_get_selected_path (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *name = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	selection = gtk_tree_view_get_selection(emft->priv->treeview);
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get(model, &iter, COL_STRING_FULL_NAME, &name, -1);

	return name;
}

CamelFolder *
em_folder_tree_get_selected_folder (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *full_name = NULL;
	CamelException ex;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	camel_exception_init (&ex);

	selection = gtk_tree_view_get_selection(emft->priv->treeview);
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get (model, &iter, COL_POINTER_CAMEL_STORE, &store,
				    COL_STRING_FULL_NAME, &full_name, -1);

	if (store && full_name)
		folder = camel_store_get_folder (store, full_name, CAMEL_STORE_FOLDER_INFO_FAST, &ex);

	camel_exception_clear (&ex);

	return folder;
}

CamelFolderInfo *
em_folder_tree_get_selected_folder_info (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *full_name = NULL, *name = NULL, *uri = NULL;
	CamelException ex;
	CamelStore *store = NULL;
	CamelFolderInfo *fi = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	camel_exception_init (&ex);

	selection = gtk_tree_view_get_selection(emft->priv->treeview);
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get (model, &iter, COL_POINTER_CAMEL_STORE, &store,
				    COL_STRING_FULL_NAME, &full_name,
				    COL_STRING_DISPLAY_NAME, &name,
				    COL_STRING_URI, &uri, -1);

	fi = camel_folder_info_new ();
	fi->full_name = g_strdup (full_name);
	fi->uri = g_strdup (uri);
	fi->name = g_strdup (name);

	d(g_print ("em_folder_tree_get_selected_folder_info: fi->full_name=[%s], fi->uri=[%s], fi->name=[%s]\n",
		   fi->full_name, fi->uri, fi->name));
	d(g_print ("em_folder_tree_get_selected_folder_info: full_name=[%s], uri=[%s], name=[%s]\n",
		   full_name, uri, name));

	if (!fi->full_name)
		goto done;

	g_free (fi->name);
        if (!g_ascii_strcasecmp (fi->full_name, "INBOX"))
                fi->name = g_strdup (_("Inbox"));
        else
                fi->name = g_strdup (name);
 done:
	return fi;
}

EMFolderTreeModel *
em_folder_tree_get_model (EMFolderTree *emft)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	return emft->priv->model;
}

EMFolderTreeModelStoreInfo *
em_folder_tree_get_model_storeinfo (EMFolderTree *emft, CamelStore *store)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;

	if (!(si = g_hash_table_lookup (priv->model->store_hash, store))) {
		g_return_val_if_reached (NULL);
	}
	return si;
}

static gboolean
emft_save_state (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;

	em_folder_tree_model_save_state (priv->model);
	priv->save_state_id = 0;

	return FALSE;
}

static void
emft_queue_save_state (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;

	if (priv->save_state_id != 0)
		return;

	priv->save_state_id = g_timeout_add_seconds (1, (GSourceFunc) emft_save_state, emft);
}

GtkWidget *
em_folder_tree_get_tree_view (EMFolderTree *emft)
{
	return (GtkWidget *)emft->priv->treeview;
}

void
em_folder_tree_set_skip_double_click (EMFolderTree *emft, gboolean skip)
{
	emft->priv->skip_double_click = skip;
}
