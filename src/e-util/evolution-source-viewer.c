/*
 * evolution-source-viewer.c
 *
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
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libedataserver/libedataserver.h>

#include "e-dialog-widgets.h"
#include "e-misc-utils.h"

/* XXX Even though this is all one file, I'm still being pedantic about data
 *     encapsulation (except for a private struct, even I'm not that anal!).
 *     I expect this program will eventually be too complex for one file
 *     and we'll want to split off an e-source-viewer.[ch]. */

/* Standard GObject macros */
#define E_TYPE_SOURCE_VIEWER \
	(e_source_viewer_get_type ())
#define E_SOURCE_VIEWER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_VIEWER, ESourceViewer))
#define E_SOURCE_VIEWER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_VIEWER, ESourceViewerClass))
#define E_IS_SOURCE_VIEWER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_VIEWER))
#define E_IS_SOURCE_VIEWER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_VIEWER))
#define E_SOURCE_VIEWER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_VIEWER, ESourceViewerClass))

typedef struct _ESourceViewer ESourceViewer;
typedef struct _ESourceViewerClass ESourceViewerClass;

struct _ESourceViewer {
	GtkWindow parent;
	ESourceRegistry *registry;

	GtkTreeStore *tree_store;
	GHashTable *source_index;

	GCancellable *delete_operation;

	GtkWidget *tree_view;		/* not referenced */
	GtkWidget *text_view;		/* not referenced */
	GtkWidget *top_panel;		/* not referenced */

	/* Viewing Page */
	GtkWidget *viewing_label;	/* not referenced */
	GtkWidget *delete_button;	/* not referenced */

	/* Deleting Page */
	GtkWidget *deleting_label;	/* not referenced */
	GtkWidget *deleting_cancel;	/* not referenced */
};

struct _ESourceViewerClass {
	GtkWindowClass parent_class;
};

enum {
	PAGE_VIEWING,
	PAGE_DELETING
};

enum {
	PROP_0,
	PROP_REGISTRY
};

enum {
	COLUMN_DISPLAY_NAME,
	COLUMN_SOURCE_UID,
	COLUMN_REMOVABLE,
	COLUMN_WRITABLE,
	COLUMN_REMOTE_CREATABLE,
	COLUMN_REMOTE_DELETABLE,
	COLUMN_SOURCE,
	NUM_COLUMNS
};

/* Forward Declarations */
GType		e_source_viewer_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_source_viewer_new		(GCancellable *cancellable,
						 GError **error);
ESourceRegistry *
		e_source_viewer_get_registry	(ESourceViewer *viewer);
GtkTreePath *	e_source_viewer_dup_selected_path
						(ESourceViewer *viewer);
gboolean	e_source_viewer_set_selected_path
						(ESourceViewer *viewer,
						 GtkTreePath *path);
ESource *	e_source_viewer_ref_selected_source
						(ESourceViewer *viewer);
gboolean	e_source_viewer_set_selected_source
						(ESourceViewer *viewer,
						 ESource *source);
GNode *		e_source_viewer_build_display_tree
						(ESourceViewer *viewer);

static void	e_source_viewer_initable_init	(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (
	ESourceViewer,
	e_source_viewer,
	GTK_TYPE_WINDOW,
	G_IMPLEMENT_INTERFACE (
		G_TYPE_INITABLE,
		e_source_viewer_initable_init));

static GIcon *
source_view_new_remote_creatable_icon (void)
{
	GEmblem *emblem;
	GIcon *emblem_icon;
	GIcon *folder_icon;
	GIcon *icon;

	emblem_icon = g_themed_icon_new ("emblem-new");
	folder_icon = g_themed_icon_new ("folder-remote");

	emblem = g_emblem_new (emblem_icon);
	icon = g_emblemed_icon_new (folder_icon, emblem);
	g_object_unref (emblem);

	g_object_unref (folder_icon);
	g_object_unref (emblem_icon);

	return icon;
}

static GIcon *
source_view_new_remote_deletable_icon (void)
{
	GEmblem *emblem;
	GIcon *emblem_icon;
	GIcon *folder_icon;
	GIcon *icon;

	emblem_icon = g_themed_icon_new ("edit-delete");
	folder_icon = g_themed_icon_new ("folder-remote");

	emblem = g_emblem_new (emblem_icon);
	icon = g_emblemed_icon_new (folder_icon, emblem);
	g_object_unref (emblem);

	g_object_unref (folder_icon);
	g_object_unref (emblem_icon);

	return icon;
}

static gchar *
source_viewer_get_monospace_font_name (void)
{
	GSettings *settings;
	gchar *font_name;

	settings = e_util_ref_settings ("org.gnome.desktop.interface");
	font_name = g_settings_get_string (settings, "monospace-font-name");
	g_object_unref (settings);

	/* Fallback to a reasonable default. */
	if (font_name == NULL)
		font_name = g_strdup ("Monospace 10");

	return font_name;
}

static void
source_viewer_set_text (ESourceViewer *viewer,
                        ESource *source)
{
	GtkTextView *text_view;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;

	text_view = GTK_TEXT_VIEW (viewer->text_view);
	buffer = gtk_text_view_get_buffer (text_view);

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);
	gtk_text_buffer_delete (buffer, &start, &end);

	if (source != NULL) {
		gchar *string;
		gsize length;

		gtk_text_buffer_get_start_iter (buffer, &start);

		string = e_source_to_string (source, &length);
		gtk_text_buffer_insert (buffer, &start, string, length);
		g_free (string);
	}
}

static void
source_viewer_update_row (ESourceViewer *viewer,
                          ESource *source)
{
	GHashTable *source_index;
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	const gchar *display_name;
	const gchar *source_uid;
	gboolean removable;
	gboolean writable;
	gboolean remote_creatable;
	gboolean remote_deletable;

	source_index = viewer->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* We show all sources, so the reference should be valid. */
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	source_uid = e_source_get_uid (source);
	display_name = e_source_get_display_name (source);
	removable = e_source_get_removable (source);
	writable = e_source_get_writable (source);
	remote_creatable = e_source_get_remote_creatable (source);
	remote_deletable = e_source_get_remote_deletable (source);

	gtk_tree_store_set (
		GTK_TREE_STORE (model), &iter,
		COLUMN_DISPLAY_NAME, display_name,
		COLUMN_SOURCE_UID, source_uid,
		COLUMN_REMOVABLE, removable,
		COLUMN_WRITABLE, writable,
		COLUMN_REMOTE_CREATABLE, remote_creatable,
		COLUMN_REMOTE_DELETABLE, remote_deletable,
		COLUMN_SOURCE, source,
		-1);
}

static gboolean
source_viewer_traverse (GNode *node,
                        gpointer user_data)
{
	ESourceViewer *viewer;
	ESource *source;
	GHashTable *source_index;
	GtkTreeRowReference *reference = NULL;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	/* Skip the root node. */
	if (G_NODE_IS_ROOT (node))
		return FALSE;

	viewer = E_SOURCE_VIEWER (user_data);

	source_index = viewer->source_index;

	tree_view = GTK_TREE_VIEW (viewer->tree_view);
	model = gtk_tree_view_get_model (tree_view);

	if (node->parent != NULL && node->parent->data != NULL)
		reference = g_hash_table_lookup (
			source_index, node->parent->data);

	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreeIter parent;

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_model_get_iter (model, &parent, path);
		gtk_tree_path_free (path);

		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent);
	} else
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

	/* Source index takes ownership. */
	source = g_object_ref (node->data);

	path = gtk_tree_model_get_path (model, &iter);
	reference = gtk_tree_row_reference_new (model, path);
	g_hash_table_insert (source_index, source, reference);
	gtk_tree_path_free (path);

	source_viewer_update_row (viewer, source);

	return FALSE;
}

static void
source_viewer_save_expanded (GtkTreeView *tree_view,
                             GtkTreePath *path,
                             GQueue *queue)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESource *source;

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);
	g_queue_push_tail (queue, source);
}

static void
source_viewer_build_model (ESourceViewer *viewer)
{
	GQueue queue = G_QUEUE_INIT;
	GHashTable *source_index;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *sel_path;
	ESource *sel_source;
	GNode *root;

	tree_view = GTK_TREE_VIEW (viewer->tree_view);

	source_index = viewer->source_index;
	sel_path = e_source_viewer_dup_selected_path (viewer);
	sel_source = e_source_viewer_ref_selected_source (viewer);

	/* Save expanded sources to restore later. */
	gtk_tree_view_map_expanded_rows (
		tree_view, (GtkTreeViewMappingFunc)
		source_viewer_save_expanded, &queue);

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_store_clear (GTK_TREE_STORE (model));

	g_hash_table_remove_all (source_index);

	root = e_source_viewer_build_display_tree (viewer);

	g_node_traverse (
		root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		(GNodeTraverseFunc) source_viewer_traverse, viewer);

	e_source_registry_free_display_tree (root);

	/* Restore previously expanded sources. */
	while (!g_queue_is_empty (&queue)) {
		GtkTreeRowReference *reference;
		ESource *source;

		source = g_queue_pop_head (&queue);
		reference = g_hash_table_lookup (source_index, source);

		if (gtk_tree_row_reference_valid (reference)) {
			GtkTreePath *path;

			path = gtk_tree_row_reference_get_path (reference);
			gtk_tree_view_expand_to_path (tree_view, path);
			gtk_tree_path_free (path);
		}

		g_object_unref (source);
	}

	/* Restore the selection. */
	if (sel_source != NULL && sel_path != NULL) {
		if (!e_source_viewer_set_selected_source (viewer, sel_source))
			e_source_viewer_set_selected_path (viewer, sel_path);
	}

	if (sel_path != NULL)
		gtk_tree_path_free (sel_path);

	if (sel_source != NULL)
		g_object_unref (sel_source);
}

static void
source_viewer_expand_to_source (ESourceViewer *viewer,
                                ESource *source)
{
	GHashTable *source_index;
	GtkTreeRowReference *reference;
	GtkTreeView *tree_view;
	GtkTreePath *path;

	source_index = viewer->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* We show all sources, so the reference should be valid. */
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	/* Expand the tree view to the path containing the ESource. */
	tree_view = GTK_TREE_VIEW (viewer->tree_view);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_view_expand_to_path (tree_view, path);
	gtk_tree_path_free (path);
}

static void
source_viewer_source_added_cb (ESourceRegistry *registry,
                               ESource *source,
                               ESourceViewer *viewer)
{
	source_viewer_build_model (viewer);

	source_viewer_expand_to_source (viewer, source);
}

static void
source_viewer_source_changed_cb (ESourceRegistry *registry,
                                 ESource *source,
                                 ESourceViewer *viewer)
{
	ESource *selected;

	source_viewer_update_row (viewer, source);

	selected = e_source_viewer_ref_selected_source (viewer);
	if (selected != NULL) {
		if (e_source_equal (source, selected))
			source_viewer_set_text (viewer, source);
		g_object_unref (selected);
	}
}

static void
source_viewer_source_removed_cb (ESourceRegistry *registry,
                                 ESource *source,
                                 ESourceViewer *viewer)
{
	source_viewer_build_model (viewer);
}

static void
source_viewer_selection_changed_cb (GtkTreeSelection *selection,
                                    ESourceViewer *viewer)
{
	ESource *source;
	const gchar *uid = NULL;
	gboolean removable = FALSE;

	source = e_source_viewer_ref_selected_source (viewer);

	source_viewer_set_text (viewer, source);

	if (source != NULL) {
		uid = e_source_get_uid (source);
		removable = e_source_get_removable (source);
	}

	gtk_label_set_text (GTK_LABEL (viewer->viewing_label), uid);
	gtk_widget_set_visible (viewer->delete_button, removable);

	if (source != NULL)
		g_object_unref (source);
}

static void
source_viewer_delete_done_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	ESource *source;
	ESourceViewer *viewer;
	GError *error = NULL;

	source = E_SOURCE (source_object);
	viewer = E_SOURCE_VIEWER (user_data);

	e_source_remove_finish (source, result, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);

	/* FIXME Show an info bar with the error message. */
	} else if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	}

	gtk_notebook_set_current_page (
		GTK_NOTEBOOK (viewer->top_panel), PAGE_VIEWING);
	gtk_widget_set_sensitive (viewer->tree_view, TRUE);

	g_object_unref (viewer->delete_operation);
	viewer->delete_operation = NULL;

	g_object_unref (viewer);
}

static void
source_viewer_delete_button_clicked_cb (GtkButton *delete_button,
                                        ESourceViewer *viewer)
{
	ESource *source;
	const gchar *uid;

	g_return_if_fail (viewer->delete_operation == NULL);

	source = e_source_viewer_ref_selected_source (viewer);
	g_return_if_fail (source != NULL);

	uid = e_source_get_uid (source);
	gtk_label_set_text (GTK_LABEL (viewer->deleting_label), uid);

	gtk_notebook_set_current_page (
		GTK_NOTEBOOK (viewer->top_panel), PAGE_DELETING);
	gtk_widget_set_sensitive (viewer->tree_view, FALSE);

	viewer->delete_operation = g_cancellable_new ();

	e_source_remove (
		source,
		viewer->delete_operation,
		source_viewer_delete_done_cb,
		g_object_ref (viewer));

	g_object_unref (source);
}

static void
source_viewer_deleting_cancel_clicked_cb (GtkButton *deleting_cancel,
                                          ESourceViewer *viewer)
{
	g_return_if_fail (viewer->delete_operation != NULL);

	g_cancellable_cancel (viewer->delete_operation);
}

static void
source_viewer_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_source_viewer_get_registry (
				E_SOURCE_VIEWER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_viewer_dispose (GObject *object)
{
	ESourceViewer *viewer = E_SOURCE_VIEWER (object);

	if (viewer->registry != NULL) {
		g_signal_handlers_disconnect_matched (
			viewer->registry,
			G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (viewer->registry);
		viewer->registry = NULL;
	}

	g_clear_object (&viewer->tree_store);

	g_hash_table_remove_all (viewer->source_index);

	g_clear_object (&viewer->delete_operation);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_source_viewer_parent_class)->dispose (object);
}

static void
source_viewer_finalize (GObject *object)
{
	ESourceViewer *viewer = E_SOURCE_VIEWER (object);

	g_hash_table_destroy (viewer->source_index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_source_viewer_parent_class)->finalize (object);
}

static void
source_viewer_constructed (GObject *object)
{
	ESourceViewer *viewer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkWidget *container;
	GtkWidget *paned;
	GtkWidget *widget;
	PangoAttribute *attr;
	PangoAttrList *bold;
	PangoFontDescription *desc;
	GIcon *icon;
	const gchar *title;
	gchar *font_name;
	gint page_num;

	viewer = E_SOURCE_VIEWER (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_source_viewer_parent_class)->constructed (object);

	bold = pango_attr_list_new ();
	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	pango_attr_list_insert (bold, attr);

	title = _("Evolution Source Viewer");
	gtk_window_set_title (GTK_WINDOW (viewer), title);
	gtk_window_set_default_size (GTK_WINDOW (viewer), 800, 600);

	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_position (GTK_PANED (paned), 400);
	gtk_container_add (GTK_CONTAINER (viewer), paned);
	gtk_widget_show (paned);

	/* Left panel */

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_add1 (GTK_PANED (paned), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_tree_view_new_with_model (
		GTK_TREE_MODEL (viewer->tree_store));
	gtk_container_add (GTK_CONTAINER (container), widget);
	viewer->tree_view = widget;  /* do not reference */
	gtk_widget_show (widget);

	column = gtk_tree_view_column_new ();
	/* Translators: The name that is displayed in the user interface */
	gtk_tree_view_column_set_title (column, _("Display Name"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COLUMN_DISPLAY_NAME);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Flags"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (
		renderer,
		"icon-name", "media-record",
		"stock-size", GTK_ICON_SIZE_MENU,
		NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_WRITABLE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (
		renderer,
		"icon-name", "list-remove",
		"stock-size", GTK_ICON_SIZE_MENU,
		NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_REMOVABLE);

	icon = source_view_new_remote_creatable_icon ();
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (
		renderer,
		"gicon", icon,
		"stock-size", GTK_ICON_SIZE_MENU,
		NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_REMOTE_CREATABLE);
	g_object_unref (icon);

	icon = source_view_new_remote_deletable_icon ();
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (
		renderer,
		"gicon", icon,
		"stock-size", GTK_ICON_SIZE_MENU,
		NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_REMOTE_DELETABLE);
	g_object_unref (icon);

	/* Append an empty pixbuf renderer to fill leftover space. */
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Identity"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COLUMN_SOURCE_UID);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	/* Right panel */

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_add2 (GTK_PANED (paned), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_notebook_new ();
	gtk_widget_set_margin_top (widget, 3);
	gtk_widget_set_margin_end (widget, 3);
	gtk_widget_set_margin_bottom (widget, 3);
	/* leave left margin at zero */
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	viewer->top_panel = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	viewer->text_view = widget;  /* do not reference */
	gtk_widget_show (widget);

	font_name = source_viewer_get_monospace_font_name ();
	desc = pango_font_description_from_string (font_name);
	gtk_widget_override_font (widget, desc);
	pango_font_description_free (desc);
	g_free (font_name);

	/* Top panel: Viewing */

	container = viewer->top_panel;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	page_num = gtk_notebook_append_page (
		GTK_NOTEBOOK (container), widget, NULL);
	g_warn_if_fail (page_num == PAGE_VIEWING);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new ("Identity:");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_attributes (GTK_LABEL (widget), bold);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	viewer->viewing_label = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = e_dialog_button_new_with_icon ("edit-delete", _("_Delete"));
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	viewer->delete_button = widget;  /* do not reference */
	gtk_widget_hide (widget);

	/* Top panel: Deleting */

	container = viewer->top_panel;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	page_num = gtk_notebook_append_page (
		GTK_NOTEBOOK (container), widget, NULL);
	g_warn_if_fail (page_num == PAGE_DELETING);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new ("Deleting");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_attributes (GTK_LABEL (widget), bold);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	viewer->deleting_label = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = e_dialog_button_new_with_icon ("process-stop", _("_Cancel"));
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	viewer->deleting_cancel = widget;  /* do not reference */
	gtk_widget_show (widget);

	pango_attr_list_unref (bold);

	g_signal_connect (
		selection, "changed",
		G_CALLBACK (source_viewer_selection_changed_cb), viewer);

	g_signal_connect (
		viewer->delete_button, "clicked",
		G_CALLBACK (source_viewer_delete_button_clicked_cb), viewer);

	g_signal_connect (
		viewer->deleting_cancel, "clicked",
		G_CALLBACK (source_viewer_deleting_cancel_clicked_cb), viewer);
}

static gboolean
source_viewer_initable_init (GInitable *initable,
                             GCancellable *cancellable,
                             GError **error)
{
	ESourceViewer *viewer;
	ESourceRegistry *registry;

	viewer = E_SOURCE_VIEWER (initable);

	registry = e_source_registry_new_sync (cancellable, error);

	if (registry == NULL)
		return FALSE;

	viewer->registry = registry;  /* takes ownership */

	g_signal_connect (
		registry, "source-added",
		G_CALLBACK (source_viewer_source_added_cb), viewer);

	g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (source_viewer_source_changed_cb), viewer);

	g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (source_viewer_source_removed_cb), viewer);

	source_viewer_build_model (viewer);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (viewer->tree_view));

	return TRUE;
}

static void
e_source_viewer_class_init (ESourceViewerClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = source_viewer_get_property;
	object_class->dispose = source_viewer_dispose;
	object_class->finalize = source_viewer_finalize;
	object_class->constructed = source_viewer_constructed;

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_source_viewer_initable_init (GInitableIface *iface)
{
	iface->init = source_viewer_initable_init;
}

static void
e_source_viewer_init (ESourceViewer *viewer)
{
	viewer->tree_store = gtk_tree_store_new (
		NUM_COLUMNS,
		G_TYPE_STRING,		/* COLUMN_DISPLAY_NAME */
		G_TYPE_STRING,		/* COLUMN_SOURCE_UID */
		G_TYPE_BOOLEAN,		/* COLUMN_REMOVABLE */
		G_TYPE_BOOLEAN,		/* COLUMN_WRITABLE */
		G_TYPE_BOOLEAN,		/* COLUMN_REMOTE_CREATABLE */
		G_TYPE_BOOLEAN,		/* COLUMN_REMOTE_DELETABLE */
		E_TYPE_SOURCE);		/* COLUMN_SOURCE */

	viewer->source_index = g_hash_table_new_full (
		(GHashFunc) e_source_hash,
		(GEqualFunc) e_source_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);
}

GtkWidget *
e_source_viewer_new (GCancellable *cancellable,
                     GError **error)
{
	return g_initable_new (
		E_TYPE_SOURCE_VIEWER,
		cancellable, error, NULL);
}

ESourceRegistry *
e_source_viewer_get_registry (ESourceViewer *viewer)
{
	g_return_val_if_fail (E_IS_SOURCE_VIEWER (viewer), NULL);

	return viewer->registry;
}

GtkTreePath *
e_source_viewer_dup_selected_path (ESourceViewer *viewer)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_SOURCE_VIEWER (viewer), NULL);

	tree_view = GTK_TREE_VIEW (viewer->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	return gtk_tree_model_get_path (model, &iter);
}

gboolean
e_source_viewer_set_selected_path (ESourceViewer *viewer,
                                   GtkTreePath *path)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_SOURCE_VIEWER (viewer), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	tree_view = GTK_TREE_VIEW (viewer->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	/* Check that the path is valid. */
	model = gtk_tree_view_get_model (tree_view);
	if (!gtk_tree_model_get_iter (model, &iter, path))
		return FALSE;

	gtk_tree_selection_unselect_all (selection);

	gtk_tree_view_expand_to_path (tree_view, path);
	gtk_tree_selection_select_path (selection, path);

	return TRUE;
}

ESource *
e_source_viewer_ref_selected_source (ESourceViewer *viewer)
{
	ESource *source;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_SOURCE_VIEWER (viewer), NULL);

	tree_view = GTK_TREE_VIEW (viewer->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	return source;
}

gboolean
e_source_viewer_set_selected_source (ESourceViewer *viewer,
                                     ESource *source)
{
	GHashTable *source_index;
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	gboolean success;

	g_return_val_if_fail (E_IS_SOURCE_VIEWER (viewer), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	source_index = viewer->source_index;
	reference = g_hash_table_lookup (source_index, source);

	if (!gtk_tree_row_reference_valid (reference))
		return FALSE;

	path = gtk_tree_row_reference_get_path (reference);
	success = e_source_viewer_set_selected_path (viewer, path);
	gtk_tree_path_free (path);

	return success;
}

/* Helper for e_source_viewer_build_display_tree() */
static gint
source_viewer_compare_nodes (GNode *node_a,
                             GNode *node_b)
{
	ESource *source_a = E_SOURCE (node_a->data);
	ESource *source_b = E_SOURCE (node_b->data);

	return e_source_compare_by_display_name (source_a, source_b);
}

/* Helper for e_source_viewer_build_display_tree() */
static gboolean
source_viewer_sort_nodes (GNode *node,
                          gpointer unused)
{
	GQueue queue = G_QUEUE_INIT;
	GNode *child_node;

	/* Unlink all the child nodes and place them in a queue. */
	while ((child_node = g_node_first_child (node)) != NULL) {
		g_node_unlink (child_node);
		g_queue_push_tail (&queue, child_node);
	}

	/* Sort the queue by source name. */
	g_queue_sort (
		&queue, (GCompareDataFunc)
		source_viewer_compare_nodes, NULL);

	/* Pop nodes off the head of the queue and put them back
	 * under the parent node (preserving the sorted order). */
	while ((child_node = g_queue_pop_head (&queue)) != NULL)
		g_node_append (node, child_node);

	return FALSE;
}

GNode *
e_source_viewer_build_display_tree (ESourceViewer *viewer)
{
	GNode *root;
	GHashTable *index;
	GList *list, *link;
	GHashTableIter iter;
	gpointer value;

	/* This is just like e_source_registry_build_display_tree()
	 * except it includes all data sources, even disabled ones.
	 * Free the tree with e_source_registry_free_display_tree(). */

	g_return_val_if_fail (E_IS_SOURCE_VIEWER (viewer), NULL);

	root = g_node_new (NULL);
	index = g_hash_table_new (g_str_hash, g_str_equal);

	/* Add a GNode for each ESource to the index.
	 * The GNodes take ownership of the ESource references. */
	list = e_source_registry_list_sources (viewer->registry, NULL);
	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		gpointer key = (gpointer) e_source_get_uid (source);
		g_hash_table_insert (index, key, g_node_new (source));
	}
	g_list_free (list);

	/* Traverse the index and link the nodes together. */
	g_hash_table_iter_init (&iter, index);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		ESource *source;
		GNode *source_node;
		GNode *parent_node;
		const gchar *parent_uid;

		source_node = (GNode *) value;
		source = E_SOURCE (source_node->data);
		parent_uid = e_source_get_parent (source);

		if (parent_uid == NULL || *parent_uid == '\0') {
			parent_node = root;
		} else {
			parent_node = g_hash_table_lookup (index, parent_uid);
		}

		/* This could be NULL if the registry service was
		 * shutdown or reloaded.  All sources will vanish. */
		if (parent_node != NULL)
			g_node_append (parent_node, source_node);
	}

	/* Sort nodes by display name in post order. */
	g_node_traverse (
		root, G_POST_ORDER, G_TRAVERSE_ALL,
		-1, source_viewer_sort_nodes, NULL);

	g_hash_table_destroy (index);

	return root;
}

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	GtkWidget *viewer;
	GError *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	viewer = e_source_viewer_new (NULL, &error);

	if (error != NULL) {
		g_warn_if_fail (viewer == NULL);
		g_error ("%s", error->message);
		g_return_val_if_reached (-1);
	}

	g_signal_connect (
		viewer, "delete-event",
		G_CALLBACK (window_delete_event_cb), NULL);

	gtk_widget_show (viewer);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}
