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

#include "e-rule-editor.h"

/* for getenv only, remove when getenv need removed */
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include "e-alert-dialog.h"
#include "e-misc-utils.h"

enum {
	BUTTON_ADD,
	BUTTON_EDIT,
	BUTTON_DUPLICATE,
	BUTTON_DELETE,
	BUTTON_TOP,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_BOTTOM,
	BUTTON_LAST
};

struct _ERuleEditorPrivate {
	GtkButton *buttons[BUTTON_LAST];
	gint drag_index;
};

G_DEFINE_TYPE_WITH_PRIVATE (ERuleEditor, e_rule_editor, GTK_TYPE_DIALOG)

static gboolean
update_selected_rule (ERuleEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (editor->list);
	if (selection && gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (editor->model), &iter, 1, &editor->current, -1);
		return TRUE;
	}

	return FALSE;
}

static void
dialog_rule_changed (EFilterRule *fr,
                     GtkWidget *dialog)
{
	g_return_if_fail (dialog != NULL);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, fr && fr->parts);
}

static void
add_editor_response (GtkWidget *dialog,
                     gint button,
                     ERuleEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_signal_handlers_disconnect_by_func (editor->edit, G_CALLBACK (dialog_rule_changed), editor->dialog);

	if (button == GTK_RESPONSE_OK) {
		EAlert *alert = NULL;
		if (!e_filter_rule_validate (editor->edit, &alert)) {
			e_alert_run_dialog (GTK_WINDOW (dialog), alert);
			g_object_unref (alert);
			return;
		}

		if (e_rule_context_find_rule (editor->context, editor->edit->name, editor->edit->source)) {
			e_alert_run_dialog_for_args (
				GTK_WINDOW (dialog),
				"filter:bad-name-notunique",
				editor->edit->name, NULL);
			return;
		}

		g_object_ref (editor->edit);

		e_filter_rule_persist_customizations (editor->edit);
		e_rule_context_add_rule (editor->context, editor->edit);

		if (g_strcmp0 (editor->source, editor->edit->source) == 0) {
			gtk_list_store_append (editor->model, &iter);
			gtk_list_store_set (
				editor->model, &iter,
				0, editor->edit->name,
				1, editor->edit,
				2, editor->edit->enabled, -1);
			selection = gtk_tree_view_get_selection (editor->list);
			gtk_tree_selection_select_iter (selection, &iter);

			/* scroll to the newly added row */
			path = gtk_tree_model_get_path (
				GTK_TREE_MODEL (editor->model), &iter);
			gtk_tree_view_scroll_to_cell (
				editor->list, path, NULL, TRUE, 1.0, 0.0);
			gtk_tree_path_free (path);

			editor->current = editor->edit;
		} else {
			editor->current = NULL;
			update_selected_rule (editor);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
editor_destroy (ERuleEditor *editor,
                GObject *deadbeef)
{
	g_clear_object (&editor->edit);

	editor->dialog = NULL;

	gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
	e_rule_editor_set_sensitive (editor);
}

static void
cursor_changed (GtkTreeView *treeview,
                ERuleEditor *editor)
{
	if (update_selected_rule (editor)) {
		g_return_if_fail (editor->current);

		e_rule_editor_set_sensitive (editor);
	}
}

static void
rule_add (GtkWidget *widget,
          ERuleEditor *editor)
{
	GtkWidget *rules;
	GtkWidget *content_area;

	if (editor->edit != NULL)
		return;

	editor->edit = e_rule_editor_create_rule (editor);
	e_filter_rule_set_source (editor->edit, editor->source);
	rules = e_filter_rule_get_widget (editor->edit, editor->context);

	editor->dialog = gtk_dialog_new ();
	gtk_dialog_add_buttons (
		GTK_DIALOG (editor->dialog),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	gtk_window_set_title ((GtkWindow *) editor->dialog, _("Add Rule"));
	gtk_window_set_default_size (GTK_WINDOW (editor->dialog), 650, 400);
	gtk_window_set_resizable (GTK_WINDOW (editor->dialog), TRUE);
	gtk_window_set_transient_for ((GtkWindow *) editor->dialog, (GtkWindow *) editor);
	gtk_container_set_border_width ((GtkContainer *) editor->dialog, 6);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (editor->dialog));
	gtk_box_pack_start (GTK_BOX (content_area), rules, TRUE, TRUE, 3);

	g_signal_connect (
		editor->dialog, "response",
		G_CALLBACK (add_editor_response), editor);
	g_object_weak_ref ((GObject *) editor->dialog, (GWeakNotify) editor_destroy, editor);

	g_signal_connect (
		editor->edit, "changed",
		G_CALLBACK (dialog_rule_changed), editor->dialog);
	dialog_rule_changed (editor->edit, editor->dialog);

	gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);

	gtk_widget_show (editor->dialog);
}

static void
edit_editor_response (GtkWidget *dialog,
                      gint button,
                      ERuleEditor *editor)
{
	EFilterRule *rule;
	GtkTreePath *path;
	GtkTreeIter iter;
	gint pos;

	g_signal_handlers_disconnect_by_func (editor->edit, G_CALLBACK (dialog_rule_changed), editor->dialog);

	if (button == GTK_RESPONSE_OK) {
		EAlert *alert = NULL;
		if (!e_filter_rule_validate (editor->edit, &alert)) {
			e_alert_run_dialog (GTK_WINDOW (dialog), alert);
			g_object_unref (alert);
			return;
		}

		rule = e_rule_context_find_rule (
			editor->context,
			editor->edit->name,
			editor->edit->source);

		if (rule != NULL && rule != editor->current) {
			e_alert_run_dialog_for_args (
				GTK_WINDOW (dialog),
				"filter:bad-name-notunique",
				rule->name, NULL);
			return;
		}

		pos = e_rule_context_get_rank_rule (
			editor->context,
			editor->current,
			editor->source);

		if (pos != -1) {
			path = gtk_tree_path_new ();
			gtk_tree_path_append_index (path, pos);
			gtk_tree_model_get_iter (
				GTK_TREE_MODEL (editor->model), &iter, path);
			gtk_tree_path_free (path);

			e_filter_rule_persist_customizations (editor->edit);

			/* replace the old rule with the new rule */
			e_filter_rule_copy (editor->current, editor->edit);

			if (g_strcmp0 (editor->source, editor->edit->source) == 0) {
				gtk_list_store_set (
					editor->model, &iter,
					0, editor->edit->name, -1);
			} else {
				gtk_list_store_remove (editor->model, &iter);
				editor->current = NULL;

				update_selected_rule (editor);
			}
		}
	}

	gtk_widget_destroy (dialog);
}

static void
rule_edit (GtkWidget *widget,
           ERuleEditor *editor)
{
	GtkWidget *rules;
	GtkWidget *content_area;

	update_selected_rule (editor);

	if (editor->current == NULL || editor->edit != NULL)
		return;

	editor->edit = e_filter_rule_clone (editor->current);

	rules = e_filter_rule_get_widget (editor->edit, editor->context);

	editor->dialog = gtk_dialog_new ();
	gtk_dialog_add_buttons (
		(GtkDialog *) editor->dialog,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_OK"), GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_title ((GtkWindow *) editor->dialog, _("Edit Rule"));
	gtk_window_set_default_size (GTK_WINDOW (editor->dialog), 650, 400);
	gtk_window_set_resizable (GTK_WINDOW (editor->dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (editor->dialog), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))));
	gtk_container_set_border_width ((GtkContainer *) editor->dialog, 6);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (editor->dialog));
	gtk_box_pack_start (GTK_BOX (content_area), rules, TRUE, TRUE, 3);

	g_signal_connect (
		editor->dialog, "response",
		G_CALLBACK (edit_editor_response), editor);
	g_object_weak_ref ((GObject *) editor->dialog, (GWeakNotify) editor_destroy, editor);

	g_signal_connect (
		editor->edit, "changed",
		G_CALLBACK (dialog_rule_changed), editor->dialog);
	dialog_rule_changed (editor->edit, editor->dialog);

	gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);

	gtk_widget_show (editor->dialog);
}

static void
rule_duplicate (GtkWidget *widget,
		ERuleEditor *editor)
{
	GtkWidget *rules;
	GtkWidget *content_area;
	gchar *new_name;

	update_selected_rule (editor);

	if (editor->current == NULL || editor->edit != NULL)
		return;

	editor->edit = e_filter_rule_clone (editor->current);
	/* Translators: the '%s' is replaced with a rule name, making it for example "Copy of Subject contains work";
	   the text itself is provided by the user. */
	new_name = g_strdup_printf (_("Copy of %s"), editor->edit->name);
	e_filter_rule_set_name (editor->edit, new_name);
	g_clear_pointer (&new_name, g_free);

	rules = e_filter_rule_get_widget (editor->edit, editor->context);

	editor->dialog = gtk_dialog_new ();
	gtk_dialog_add_buttons (
		(GtkDialog *) editor->dialog,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_OK"), GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_title ((GtkWindow *) editor->dialog, _("Edit Rule"));
	gtk_window_set_default_size (GTK_WINDOW (editor->dialog), 650, 400);
	gtk_window_set_resizable (GTK_WINDOW (editor->dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (editor->dialog), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))));
	gtk_container_set_border_width ((GtkContainer *) editor->dialog, 6);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (editor->dialog));
	gtk_box_pack_start (GTK_BOX (content_area), rules, TRUE, TRUE, 3);

	g_signal_connect (
		editor->dialog, "response",
		G_CALLBACK (add_editor_response), editor);
	g_object_weak_ref ((GObject *) editor->dialog, (GWeakNotify) editor_destroy, editor);

	g_signal_connect (
		editor->edit, "changed",
		G_CALLBACK (dialog_rule_changed), editor->dialog);
	dialog_rule_changed (editor->edit, editor->dialog);

	gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);

	gtk_widget_show (editor->dialog);
}

static void
rule_delete (GtkWidget *widget,
             ERuleEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	gint pos, len;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (editor->context, editor->current, editor->source);
	if (pos != -1) {
		GtkWindow *parent;
		GtkWidget *toplevel;
		gint response;

		toplevel = gtk_widget_get_toplevel (widget);
		parent = GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL;

		response = e_alert_run_dialog_for_args (parent, "filter:remove-rule-question",
			(editor->current && editor->current->name) ? editor->current->name : "", NULL);

		if (response != GTK_RESPONSE_YES)
			pos = -1;
	}

	if (pos != -1) {
		EFilterRule *delete_rule = editor->current;

		editor->current = NULL;

		e_rule_context_remove_rule (editor->context, delete_rule);

		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, pos);
		gtk_tree_model_get_iter (GTK_TREE_MODEL (editor->model), &iter, path);
		gtk_list_store_remove (editor->model, &iter);
		gtk_tree_path_free (path);

		g_object_unref (delete_rule);

		/* now select the next rule */
		len = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (editor->model), NULL);
		pos = pos >= len ? len - 1 : pos;

		if (pos >= 0) {
			path = gtk_tree_path_new ();
			gtk_tree_path_append_index (path, pos);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (editor->model), &iter, path);
			gtk_tree_path_free (path);

			/* select the new row */
			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->list));
			gtk_tree_selection_select_iter (selection, &iter);

			/* scroll to the selected row */
			path = gtk_tree_model_get_path ((GtkTreeModel *) editor->model, &iter);
			gtk_tree_view_scroll_to_cell (editor->list, path, NULL, FALSE, 0.0, 0.0);
			gtk_tree_path_free (path);

			/* update our selection state */
			cursor_changed (editor->list, editor);
			return;
		}
	}

	e_rule_editor_set_sensitive (editor);
}

static void
rule_move (ERuleEditor *editor,
           gint from,
           gint to)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	EFilterRule *rule;

	e_rule_context_rank_rule (
		editor->context, editor->current, editor->source, to);

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, from);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (editor->model), &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (GTK_TREE_MODEL (editor->model), &iter, 1, &rule, -1);
	g_return_if_fail (rule != NULL);

	/* remove and then re-insert the row at the new location */
	gtk_list_store_remove (editor->model, &iter);
	gtk_list_store_insert (editor->model, &iter, to);

	/* set the data on the row */
	gtk_list_store_set (editor->model, &iter, 0, rule->name, 1, rule, 2, rule->enabled, -1);

	/* select the row */
	selection = gtk_tree_view_get_selection (editor->list);
	gtk_tree_selection_select_iter (selection, &iter);

	/* scroll to the selected row */
	path = gtk_tree_model_get_path ((GtkTreeModel *) editor->model, &iter);
	gtk_tree_view_scroll_to_cell (editor->list, path, NULL, FALSE, 0.0, 0.0);
	gtk_tree_path_free (path);

	e_rule_editor_set_sensitive (editor);
}

static void
rule_top (GtkWidget *widget,
          ERuleEditor *editor)
{
	gint pos;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (
		editor->context, editor->current, editor->source);
	if (pos > 0)
		rule_move (editor, pos, 0);
}

static void
rule_up (GtkWidget *widget,
         ERuleEditor *editor)
{
	gint pos;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (
		editor->context, editor->current, editor->source);
	if (pos > 0)
		rule_move (editor, pos, pos - 1);
}

static void
rule_down (GtkWidget *widget,
           ERuleEditor *editor)
{
	gint pos;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (
		editor->context, editor->current, editor->source);
	if (pos >= 0)
		rule_move (editor, pos, pos + 1);
}

static void
rule_bottom (GtkWidget *widget,
             ERuleEditor *editor)
{
	gint pos;
	gint count = 0;
	EFilterRule *rule = NULL;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (
		editor->context, editor->current, editor->source);
	/* There's probably a better/faster way to get the count of the list here */
	while ((rule = e_rule_context_next_rule (editor->context, rule, editor->source)))
		count++;
	count--;
	if (pos >= 0)
		rule_move (editor, pos, count);
}

static struct {
	const gchar *name;
	GCallback func;
} edit_buttons[] = {
	{ "rule_add",    G_CALLBACK (rule_add)    },
	{ "rule_edit",   G_CALLBACK (rule_edit)   },
	{ "rule_duplicate", G_CALLBACK (rule_duplicate) },
	{ "rule_delete", G_CALLBACK (rule_delete) },
	{ "rule_top",    G_CALLBACK (rule_top)    },
	{ "rule_up",     G_CALLBACK (rule_up)     },
	{ "rule_down",   G_CALLBACK (rule_down)   },
	{ "rule_bottom", G_CALLBACK (rule_bottom) },
};

static void
rule_editor_finalize (GObject *object)
{
	ERuleEditor *editor = E_RULE_EDITOR (object);

	g_object_unref (editor->context);

	g_clear_pointer (&editor->source, g_free);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_rule_editor_parent_class)->finalize (object);
}

static void
rule_editor_dispose (GObject *object)
{
	ERuleEditor *editor = E_RULE_EDITOR (object);

	if (editor->dialog != NULL) {
		gtk_widget_destroy (GTK_WIDGET (editor->dialog));
		editor->dialog = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_rule_editor_parent_class)->dispose (object);
}

static void
rule_editor_set_source (ERuleEditor *editor,
                        const gchar *source)
{
	EFilterRule *rule = NULL;
	GtkTreeIter iter;

	gtk_list_store_clear (editor->model);

	while ((rule = e_rule_context_next_rule (editor->context, rule, source)) != NULL) {
		gtk_list_store_append (editor->model, &iter);
		gtk_list_store_set (
			editor->model, &iter,
			0, rule->name, 1, rule, 2, rule->enabled, -1);
	}

	g_free (editor->source);
	editor->source = g_strdup (source);
	editor->current = NULL;
	e_rule_editor_set_sensitive (editor);
}

static void
rule_editor_set_sensitive (ERuleEditor *editor)
{
	EFilterRule *rule = NULL;
	gint index = -1, count = 0;

	while ((rule = e_rule_context_next_rule (editor->context, rule, editor->source))) {
		if (rule == editor->current)
			index = count;
		count++;
	}

	count--;

	gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->buttons[BUTTON_EDIT]), index != -1);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->buttons[BUTTON_DUPLICATE]), index != -1);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->buttons[BUTTON_DELETE]), index != -1);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->buttons[BUTTON_TOP]), index > 0);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->buttons[BUTTON_UP]), index > 0);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->buttons[BUTTON_DOWN]), index >= 0 && index < count);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->buttons[BUTTON_BOTTOM]), index >= 0 && index < count);
}

static EFilterRule *
rule_editor_create_rule (ERuleEditor *editor)
{
	EFilterRule *rule;
	EFilterPart *part;

	/* create a rule with 1 part in it */
	rule = e_filter_rule_new ();
	part = e_rule_context_next_part (editor->context, NULL);
	e_filter_rule_add_part (rule, e_filter_part_clone (part));

	return rule;
}

static void
e_rule_editor_class_init (ERuleEditorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = rule_editor_finalize;
	object_class->dispose = rule_editor_dispose;

	class->set_source = rule_editor_set_source;
	class->set_sensitive = rule_editor_set_sensitive;
	class->create_rule = rule_editor_create_rule;
}

static void
e_rule_editor_init (ERuleEditor *editor)
{
	editor->priv = e_rule_editor_get_instance_private (editor);
	editor->priv->drag_index = -1;
}

/**
 * rule_editor_new:
 *
 * Create a new ERuleEditor object.
 *
 * Return value: A new #ERuleEditor object.
 **/
ERuleEditor *
e_rule_editor_new (ERuleContext *context,
                   const gchar *source,
                   const gchar *label)
{
	ERuleEditor *editor = (ERuleEditor *) g_object_new (E_TYPE_RULE_EDITOR, NULL);
	GtkBuilder *builder;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "filter.ui");
	e_rule_editor_construct (editor, context, builder, source, label);
	gtk_widget_hide (e_builder_get_widget (builder, "label17"));
	gtk_widget_hide (e_builder_get_widget (builder, "filter_source_combobox"));
	g_object_unref (builder);

	return editor;
}

void
e_rule_editor_set_sensitive (ERuleEditor *editor)
{
	ERuleEditorClass *class;

	g_return_if_fail (E_IS_RULE_EDITOR (editor));

	class = E_RULE_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_sensitive != NULL);

	class->set_sensitive (editor);
}

void
e_rule_editor_set_source (ERuleEditor *editor,
                          const gchar *source)
{
	ERuleEditorClass *class;

	g_return_if_fail (E_IS_RULE_EDITOR (editor));

	class = E_RULE_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_source != NULL);

	class->set_source (editor, source);
}

EFilterRule *
e_rule_editor_create_rule (ERuleEditor *editor)
{
	ERuleEditorClass *class;

	g_return_val_if_fail (E_IS_RULE_EDITOR (editor), NULL);

	class = E_RULE_EDITOR_GET_CLASS (editor);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->create_rule != NULL, NULL);

	return class->create_rule (editor);
}

static void
double_click (GtkTreeView *treeview,
              GtkTreePath *path,
              GtkTreeViewColumn *column,
              ERuleEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (editor->list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (GTK_TREE_MODEL (editor->model), &iter, 1, &editor->current, -1);

	if (editor->current)
		rule_edit ((GtkWidget *) treeview, editor);
}

static void
rule_able_toggled (GtkCellRendererToggle *renderer,
                   gchar *path_string,
                   gpointer user_data)
{
	GtkWidget *table = user_data;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_path_new_from_string (path_string);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (table));

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		EFilterRule *rule = NULL;

		gtk_tree_model_get (model, &iter, 1, &rule, -1);

		if (rule) {
			rule->enabled = !rule->enabled;
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, 2, rule->enabled, -1);
		}
	}

	gtk_tree_path_free (path);
}

static void
editor_tree_drag_begin_cb (GtkWidget *widget,
			   GdkDragContext *context,
			   gpointer user_data)
{
	ERuleEditor *editor = user_data;
	GtkTreeSelection *selection;
	cairo_surface_t *surface;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	EFilterRule *rule = NULL;

	g_return_if_fail (editor != NULL);

	selection = gtk_tree_view_get_selection (editor->list);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		editor->priv->drag_index = -1;
		return;
	}

	gtk_tree_model_get (model, &iter, 1, &rule, -1);
	if (!rule) {
		editor->priv->drag_index = -1;
		return;
	}

	editor->priv->drag_index = e_rule_context_get_rank_rule (editor->context, rule, editor->source);

	path = gtk_tree_model_get_path (model, &iter);

	surface = gtk_tree_view_create_row_drag_icon (editor->list, path);
	gtk_drag_set_icon_surface (context, surface);
	cairo_surface_destroy (surface);

	gtk_tree_path_free (path);
}

static gboolean
editor_tree_drag_drop_cb (GtkWidget *widget,
			  GdkDragContext *context,
			  gint x,
			  gint y,
			  guint time,
			  gpointer user_data)
{
	ERuleEditor *editor = user_data;

	g_return_val_if_fail (editor != NULL, FALSE);

	editor->priv->drag_index = -1;

	return TRUE;
}

static void
editor_tree_drag_end_cb (GtkWidget *widget,
			 GdkDragContext *context,
			 gpointer user_data)
{
	ERuleEditor *editor = user_data;

	g_return_if_fail (editor != NULL);

	editor->priv->drag_index = -1;
}

static gboolean
editor_tree_drag_motion_cb (GtkWidget *widget,
			    GdkDragContext *context,
			    gint x,
			    gint y,
			    guint time,
			    gpointer user_data)
{
	ERuleEditor *editor = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	EFilterRule *rule = NULL;

	g_return_val_if_fail (editor != NULL, FALSE);

	if (editor->priv->drag_index == -1 ||
	    !gtk_tree_view_get_dest_row_at_pos (editor->list, x, y, &path, NULL))
		return FALSE;

	model = gtk_tree_view_get_model (editor->list);

	g_warn_if_fail (gtk_tree_model_get_iter (model, &iter, path));

	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter, 1, &rule, -1);

	if (rule) {
		gint drop_index;

		drop_index = e_rule_context_get_rank_rule (editor->context, rule, editor->source);

		if (drop_index != editor->priv->drag_index && drop_index >= 0) {
			editor->current = e_rule_context_find_rank_rule (editor->context, editor->priv->drag_index, editor->source);
			rule_move (editor, editor->priv->drag_index, drop_index);

			editor->priv->drag_index = drop_index;

			/* to update the editor->current */
			cursor_changed (NULL, editor);
		}
	}

	gdk_drag_status (context, (!rule || editor->priv->drag_index == -1) ? 0 : GDK_ACTION_MOVE, time);

	return TRUE;
}

void
e_rule_editor_construct (ERuleEditor *editor,
                         ERuleContext *context,
                         GtkBuilder *builder,
                         const gchar *source,
                         const gchar *label)
{
	const GtkTargetEntry row_targets[] = {
		{ (gchar *) "ERuleEditorRow", GTK_TARGET_SAME_WIDGET, 0 }
	};
	GtkWidget *widget;
	GtkWidget *action_area;
	GtkWidget *content_area;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GObject *object;
	GList *list;
	gint i;

	g_return_if_fail (E_IS_RULE_EDITOR (editor));
	g_return_if_fail (E_IS_RULE_CONTEXT (context));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	editor->context = g_object_ref (context);

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (editor));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (editor));

	gtk_window_set_resizable ((GtkWindow *) editor, TRUE);
	gtk_window_set_default_size ((GtkWindow *) editor, 350, 400);
	gtk_widget_realize ((GtkWidget *) editor);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);

	widget = e_builder_get_widget (builder, "rule_editor");
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);

	for (i = 0; i < BUTTON_LAST; i++) {
		widget = e_builder_get_widget (builder, edit_buttons[i].name);
		editor->priv->buttons[i] = GTK_BUTTON (widget);
		g_signal_connect (
			widget, "clicked",
			G_CALLBACK (edit_buttons[i].func), editor);
	}

	object = gtk_builder_get_object (builder, "rule_tree_view");
	editor->list = GTK_TREE_VIEW (object);

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (object), 0);
	g_return_if_fail (column != NULL);

	gtk_tree_view_column_set_visible (column, FALSE);
	list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	g_return_if_fail (list != NULL);

	renderer = GTK_CELL_RENDERER (list->data);
	g_warn_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (renderer));

	g_list_free (list);

	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (rule_able_toggled), editor->list);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	object = gtk_builder_get_object (builder, "rule_list_store");
	editor->model = GTK_LIST_STORE (object);

	g_signal_connect (
		editor->list, "cursor-changed",
		G_CALLBACK (cursor_changed), editor);
	g_signal_connect (
		editor->list, "row-activated",
		G_CALLBACK (double_click), editor);

	widget = e_builder_get_widget (builder, "rule_label");
	gtk_label_set_label (GTK_LABEL (widget), label);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), GTK_WIDGET (editor->list));

	rule_editor_set_source (editor, source);

	gtk_dialog_add_buttons (
		GTK_DIALOG (editor),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	gtk_drag_source_set (GTK_WIDGET (editor->list), GDK_BUTTON1_MASK, row_targets, G_N_ELEMENTS (row_targets), GDK_ACTION_MOVE);
	gtk_drag_dest_set (GTK_WIDGET (editor->list), GTK_DEST_DEFAULT_MOTION, row_targets, G_N_ELEMENTS (row_targets), GDK_ACTION_MOVE);

	g_signal_connect (editor->list, "drag-begin",
		G_CALLBACK (editor_tree_drag_begin_cb), editor);
	g_signal_connect (editor->list, "drag-drop",
		G_CALLBACK (editor_tree_drag_drop_cb), editor);
	g_signal_connect (editor->list, "drag-end",
		G_CALLBACK (editor_tree_drag_end_cb), editor);
	g_signal_connect (editor->list, "drag-motion",
		G_CALLBACK (editor_tree_drag_motion_cb), editor);
}
