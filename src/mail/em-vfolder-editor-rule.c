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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <shell/e-shell.h>

#include <e-util/e-util.h>
#include <e-util/e-util-private.h>

#include "em-folder-selector.h"
#include "em-folder-tree.h"
#include "em-utils.h"
#include "em-vfolder-editor-context.h"

#include "em-vfolder-editor-rule.h"

#define EM_VFOLDER_EDITOR_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_VFOLDER_EDITOR_RULE, EMVFolderEditorRulePrivate))

#define EM_VFOLDER_EDITOR_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_VFOLDER_EDITOR_RULE, EMVFolderEditorRulePrivate))

struct _EMVFolderEditorRulePrivate {
	EMailSession *session;
};

enum {
	PROP_0,
	PROP_SESSION
};

static GtkWidget *get_widget (EFilterRule *fr, ERuleContext *f);

G_DEFINE_TYPE (
	EMVFolderEditorRule,
	em_vfolder_editor_rule,
	EM_TYPE_VFOLDER_RULE)

static void
vfolder_editor_rule_set_session (EMVFolderEditorRule *rule,
                          EMailSession *session)
{
	if (session == NULL) {
		EShell *shell;
		EShellBackend *shell_backend;
		EMailBackend *backend;

		shell = e_shell_get_default ();
		shell_backend = e_shell_get_backend_by_name (shell, "mail");

		backend = E_MAIL_BACKEND (shell_backend);
		session = e_mail_backend_get_session (backend);
	}

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (rule->priv->session == NULL);

	rule->priv->session = g_object_ref (session);
}

static void
vfolder_editor_rule_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			vfolder_editor_rule_set_session (
				EM_VFOLDER_EDITOR_RULE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
vfolder_editor_rule_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_vfolder_editor_rule_get_session (
				EM_VFOLDER_EDITOR_RULE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
vfolder_editor_rule_dispose (GObject *object)
{
	EMVFolderEditorRulePrivate *priv;

	priv = EM_VFOLDER_EDITOR_RULE_GET_PRIVATE (object);
	g_clear_object (&priv->session);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_vfolder_editor_rule_parent_class)->dispose (object);
}

static void
vfolder_editor_rule_finalize (GObject *object)
{
	/* EMVFolderEditorRule *rule = EM_VFOLDER_EDITOR_RULE (object); */

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_vfolder_editor_rule_parent_class)->finalize (object);
}

static void
em_vfolder_editor_rule_class_init (EMVFolderEditorRuleClass *class)
{
	GObjectClass *object_class;
	EFilterRuleClass *filter_rule_class;

	g_type_class_add_private (class, sizeof (EMVFolderEditorRulePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = vfolder_editor_rule_set_property;
	object_class->get_property = vfolder_editor_rule_get_property;
	object_class->dispose = vfolder_editor_rule_dispose;
	object_class->finalize = vfolder_editor_rule_finalize;

	filter_rule_class = E_FILTER_RULE_CLASS (class);
	filter_rule_class->get_widget = get_widget;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
em_vfolder_editor_rule_init (EMVFolderEditorRule *rule)
{
	rule->priv = EM_VFOLDER_EDITOR_RULE_GET_PRIVATE (rule);
}

EFilterRule *
em_vfolder_editor_rule_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_VFOLDER_EDITOR_RULE, "session", session, NULL);
}

EMailSession *
em_vfolder_editor_rule_get_session (EMVFolderEditorRule *rule)
{
	g_return_val_if_fail (EM_IS_VFOLDER_RULE (rule), NULL);

	return rule->priv->session;
}

enum {
	BUTTON_ADD,
	BUTTON_REMOVE,
	BUTTON_LAST
};

struct _source_data {
	ERuleContext *rc;
	EMVFolderRule *vr;
	GtkListStore *model;
	GtkTreeView *tree_view;
	GtkWidget *source_selector;
	GtkWidget *buttons[BUTTON_LAST];
};

static void
source_data_free (gpointer ptr)
{
	struct _source_data *sd = ptr;

	if (sd) {
		g_clear_object (&sd->model);
		g_free (sd);
	}
}

static void
set_sensitive (struct _source_data *data)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (data->tree_view);

	gtk_widget_set_sensitive (
		GTK_WIDGET (data->buttons[BUTTON_ADD]), TRUE);
	gtk_widget_set_sensitive (
		GTK_WIDGET (data->buttons[BUTTON_REMOVE]),
		selection && gtk_tree_selection_count_selected_rows (selection) > 0);
}

static void
selection_changed_cb (GtkTreeSelection *selection,
                      struct _source_data *data)
{
	set_sensitive (data);
}

static void
select_source_with_changed (GtkWidget *widget,
                            struct _source_data *data)
{
	em_vfolder_rule_with_t with;

	with = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	if (with > EM_VFOLDER_RULE_WITH_LOCAL)
		with = 0;

	with = 3 - with;

	gtk_widget_set_sensitive (data->source_selector, !with);

	em_vfolder_rule_set_with (data->vr, with);
}

static void
autoupdate_toggled_cb (GtkToggleButton *toggle,
                       struct _source_data *data)
{
	em_vfolder_rule_set_autoupdate (data->vr, gtk_toggle_button_get_active (toggle));
}

static void
include_subfolders_toggled_cb (GtkCellRendererToggle *cell_renderer,
                               const gchar *path_string,
                               struct _source_data *data)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	gtk_cell_renderer_toggle_set_active (
		cell_renderer,
		!gtk_cell_renderer_toggle_get_active (cell_renderer));

	model = gtk_tree_view_get_model (data->tree_view);
	path = gtk_tree_path_new_from_string (path_string);

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		gchar *source = NULL;

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			2, gtk_cell_renderer_toggle_get_active (cell_renderer),
			-1);

		gtk_tree_model_get (model, &iter, 1, &source, -1);
		if (source) {
			em_vfolder_rule_source_set_include_subfolders (
				data->vr, source,
				gtk_cell_renderer_toggle_get_active (cell_renderer));
			g_free (source);
		}
	}

	gtk_tree_path_free (path);
}

static void
vfr_folder_response (EMFolderSelector *selector,
                     gint button,
                     struct _source_data *data)
{
	EMFolderTreeModel *model;
	EMFolderTree *folder_tree;
	CamelSession *session;
	GList *selected_uris;

	folder_tree = em_folder_selector_get_folder_tree (selector);
	model = em_folder_selector_get_model (selector);
	session = CAMEL_SESSION (em_folder_tree_model_get_session (model));

	selected_uris = em_folder_tree_get_selected_uris (folder_tree);

	if (button == GTK_RESPONSE_OK && selected_uris != NULL) {
		GList *uris_iter;
		GHashTable *known_uris;
		GtkTreeIter iter;
		GtkTreeSelection *selection;
		gboolean changed = FALSE;

		selection = gtk_tree_view_get_selection (data->tree_view);
		gtk_tree_selection_unselect_all (selection);

		known_uris = g_hash_table_new_full (
			(GHashFunc) g_str_hash,
			(GEqualFunc) g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) NULL);

		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (data->model), &iter)) {
			GtkTreeModel *model = GTK_TREE_MODEL (data->model);
			do {
				gchar *known = NULL;

				gtk_tree_model_get (model, &iter, 1, &known, -1);

				if (known)
					g_hash_table_add (known_uris, known);
			} while (gtk_tree_model_iter_next (model, &iter));
		}

		for (uris_iter = selected_uris; uris_iter != NULL; uris_iter = uris_iter->next) {
			const gchar *uri = uris_iter->data;
			gchar *markup;

			if (uri == NULL)
				continue;

			if (g_hash_table_contains (known_uris, uri))
				continue;

			g_hash_table_add (known_uris, g_strdup (uri));

			changed = TRUE;
			g_queue_push_tail (em_vfolder_rule_get_sources (data->vr), g_strdup (uri));

			markup = e_mail_folder_uri_to_markup (session, uri, NULL);

			gtk_list_store_append (data->model, &iter);
			gtk_list_store_set (data->model, &iter, 0, markup, 1, uri, -1);
			g_free (markup);

			/* select all newly added folders */
			gtk_tree_selection_select_iter (selection, &iter);
		}

		g_hash_table_destroy (known_uris);
		if (changed)
			em_vfolder_rule_sources_changed (data->vr);

		set_sensitive (data);
	}

	gtk_widget_destroy (GTK_WIDGET (selector));
	g_list_free_full (selected_uris, g_free);
}

static void
source_add (GtkWidget *widget,
            struct _source_data *data)
{
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	EMFolderSelector *selector;
	GtkTreeSelection *selection;
	GtkWidget *dialog;
	gpointer parent;

	parent = gtk_widget_get_toplevel (widget);
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	model = em_folder_tree_model_get_default ();

	dialog = em_folder_selector_new (parent, model);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Add Folder"));

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_can_create (selector, TRUE);
	em_folder_selector_set_default_button_label (selector, _("_Add"));

	folder_tree = em_folder_selector_get_folder_tree (selector);

	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOSELECT);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (vfr_folder_response), data);

	gtk_widget_show (dialog);
}

static void
source_remove (GtkWidget *widget,
               struct _source_data *data)
{
	GtkTreeSelection *selection;
	const gchar *source, *prev_source;
	GtkTreePath *path;
	GtkTreeIter iter;
	GHashTable *to_remove;
	gint index = 0, first_selected = -1, removed;
	gint n;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->tree_view));
	to_remove = g_hash_table_new (g_direct_hash, g_direct_equal);

	source = NULL;
	while ((source = em_vfolder_rule_next_source (data->vr, source))) {
		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, index);

		if (gtk_tree_selection_path_is_selected (selection, path)) {
			g_hash_table_add (to_remove, GINT_TO_POINTER (index));

			if (first_selected == -1)
				first_selected = index;
		}

		index++;

		gtk_tree_path_free (path);
	}

	/* do not depend on selection when removing */
	gtk_tree_selection_unselect_all (selection);

	index = 0;
	source = NULL;
	removed = 0;
	prev_source = NULL;
	while ((source = em_vfolder_rule_next_source (data->vr, source))) {
		if (g_hash_table_contains (to_remove, GINT_TO_POINTER (index + removed))) {
			path = gtk_tree_path_new ();
			gtk_tree_path_append_index (path, index);
			gtk_tree_model_get_iter (
				GTK_TREE_MODEL (data->model), &iter, path);

			em_vfolder_rule_remove_source (data->vr, source);
			gtk_list_store_remove (data->model, &iter);
			gtk_tree_path_free (path);

			/* try again from the previous source */
			removed++;
			source = prev_source;
		} else {
			index++;
			prev_source = source;
		}
	}

	g_hash_table_destroy (to_remove);

	/* now select the next rule */
	n = gtk_tree_model_iter_n_children (
		GTK_TREE_MODEL (data->model), NULL);
	index = first_selected >= n ? n - 1 : first_selected;

	if (index >= 0) {
		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, index);
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (data->model), &iter, path)) {
			gtk_tree_selection_select_iter (selection, &iter);
			gtk_tree_view_set_cursor (data->tree_view, path, NULL, FALSE);
		}
		gtk_tree_path_free (path);
	}

	set_sensitive (data);
}

static GtkWidget *
get_widget (EFilterRule *fr,
            ERuleContext *rc)
{
	EMVFolderRule *vr = (EMVFolderRule *) fr;
	EMailSession *session;
	GtkWidget *widget, *frame, *label, *combobox, *hgrid, *vgrid, *tree_view, *scrolled_window;
	GtkWidget *autoupdate;
	GtkListStore *model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	struct _source_data *data;
	const gchar *source;
	gchar *tmp;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	widget = E_FILTER_RULE_CLASS (em_vfolder_editor_rule_parent_class)->
		get_widget (fr, rc);

	data = g_malloc0 (sizeof (*data));
	data->rc = rc;
	data->vr = vr;

	frame = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (frame), GTK_ORIENTATION_VERTICAL);
	gtk_grid_set_row_spacing (GTK_GRID (frame), 6);

	g_object_set_data_full (G_OBJECT (frame), "data", data, source_data_free);

	tmp = g_strdup_printf ("<b>%s</b>", _("Search Folder Sources"));
	label = gtk_label_new (tmp);
	g_free (tmp);
	g_object_set (
		G_OBJECT (label),
		"use-markup", TRUE,
		"xalign", 0.0,
		NULL);

	gtk_container_add (GTK_CONTAINER (frame), label);

	hgrid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (hgrid), GTK_ORIENTATION_HORIZONTAL);
	gtk_container_add (GTK_CONTAINER (frame), hgrid);

	label = gtk_label_new ("    ");
	gtk_container_add (GTK_CONTAINER (hgrid), label);

	vgrid = gtk_grid_new ();
	g_object_set (
		G_OBJECT (vgrid),
		"orientation", GTK_ORIENTATION_VERTICAL,
		"border-width", 6,
		"row-spacing", 6,
		NULL);
	gtk_container_add (GTK_CONTAINER (hgrid), vgrid);

	hgrid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (hgrid), GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_set_column_spacing (GTK_GRID (hgrid), 6);
	gtk_container_add (GTK_CONTAINER (vgrid), hgrid);

	autoupdate = gtk_check_button_new_with_mnemonic (_("Automatically update on any _source folder change"));
	gtk_container_add (GTK_CONTAINER (hgrid), autoupdate);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autoupdate), em_vfolder_rule_get_autoupdate (vr));
	g_signal_connect (autoupdate, "toggled", G_CALLBACK (autoupdate_toggled_cb), data);

	hgrid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (hgrid), GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_set_column_spacing (GTK_GRID (hgrid), 6);
	gtk_container_add (GTK_CONTAINER (vgrid), hgrid);

	combobox = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combobox), NULL, _("All local folders"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combobox), NULL, _("All active remote folders"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combobox), NULL, _("All local and active remote folders"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combobox), NULL, _("Specific folders"));
	gtk_container_add (GTK_CONTAINER (hgrid), combobox);

	hgrid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (hgrid), GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_set_column_spacing (GTK_GRID (hgrid), 6);
	gtk_container_add (GTK_CONTAINER (vgrid), hgrid);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (
		G_OBJECT (scrolled_window),
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (hgrid), scrolled_window);

	model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	renderer = gtk_cell_renderer_text_new ();
	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (tree_view),
		-1, "column", renderer, "markup", 0, NULL);

	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes (
		"include subfolders", renderer, "active", 2, NULL);
	g_signal_connect (renderer, "toggled", G_CALLBACK (include_subfolders_toggled_cb), data);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (
		G_OBJECT (renderer),
		"editable", FALSE,
		"text", _("include subfolders"),
		NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (tree_view), column, -1);

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), 0);
	gtk_tree_view_column_set_expand (column, TRUE);

	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), tree_view);

	vgrid = gtk_grid_new ();
	g_object_set (
		G_OBJECT (vgrid),
		"orientation", GTK_ORIENTATION_VERTICAL,
		"border-width", 6,
		"row-spacing", 6,
		NULL);
	gtk_container_add (GTK_CONTAINER (hgrid), vgrid);

	data->buttons[BUTTON_ADD] = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	g_signal_connect (
		data->buttons[BUTTON_ADD], "clicked",
		G_CALLBACK (source_add), data);

	data->buttons[BUTTON_REMOVE] = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	g_signal_connect (
		data->buttons[BUTTON_REMOVE], "clicked",
		G_CALLBACK (source_remove), data);

	gtk_container_add (GTK_CONTAINER (vgrid), data->buttons[BUTTON_ADD]);
	gtk_container_add (GTK_CONTAINER (vgrid), data->buttons[BUTTON_REMOVE]);

	data->tree_view = GTK_TREE_VIEW (tree_view);
	data->model = model;

	session = em_vfolder_editor_context_get_session (EM_VFOLDER_EDITOR_CONTEXT (rc));

	source = NULL;
	while ((source = em_vfolder_rule_next_source (vr, source))) {
		gchar *markup;

		markup = e_mail_folder_uri_to_markup (
			CAMEL_SESSION (session), source, NULL);

		gtk_list_store_append (data->model, &iter);
		gtk_list_store_set (
			data->model, &iter,
			0, markup,
			1, source,
			2, em_vfolder_rule_source_get_include_subfolders (vr, source),
			-1);
		g_free (markup);
	}

	selection = gtk_tree_view_get_selection (data->tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (
		selection, "changed",
		G_CALLBACK (selection_changed_cb), data);

	data->source_selector = hgrid;

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 3 - em_vfolder_rule_get_with (vr));
	g_signal_connect (
		combobox, "changed",
		G_CALLBACK (select_source_with_changed), data);
	select_source_with_changed (combobox, data);

	set_sensitive (data);

	gtk_widget_set_valign (frame, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (frame, TRUE);
	gtk_widget_show_all (frame);

	gtk_container_add (GTK_CONTAINER (widget), frame);

	return widget;
}
