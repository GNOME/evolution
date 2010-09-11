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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* for getenv only, remove when getenv need removed */
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include "e-util/e-alert-dialog.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"

#include "e-rule-editor.h"

#define E_RULE_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_RULE_EDITOR, ERuleEditorPrivate))

static gint enable_undo = 0;

enum {
	BUTTON_ADD,
	BUTTON_EDIT,
	BUTTON_DELETE,
	BUTTON_TOP,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_BOTTOM,
	BUTTON_LAST
};

struct _ERuleEditorPrivate {
	GtkButton *buttons[BUTTON_LAST];
};

G_DEFINE_TYPE (
	ERuleEditor,
	e_rule_editor,
	GTK_TYPE_DIALOG)

static void
rule_editor_add_undo (ERuleEditor *editor,
                      gint type,
                      EFilterRule *rule,
                      gint rank,
                      gint newrank)
{
        ERuleEditorUndo *undo;

        if (!editor->undo_active && enable_undo) {
                undo = g_malloc0 (sizeof (*undo));
                undo->rule = rule;
                undo->type = type;
                undo->rank = rank;
                undo->newrank = newrank;

                undo->next = editor->undo_log;
                editor->undo_log = undo;
        } else {
                g_object_unref (rule);
        }
}

static void
rule_editor_play_undo (ERuleEditor *editor)
{
	ERuleEditorUndo *undo, *next;
	EFilterRule *rule;

	editor->undo_active = TRUE;
	undo = editor->undo_log;
	editor->undo_log = NULL;
	while (undo) {
		next = undo->next;
		switch (undo->type) {
		case E_RULE_EDITOR_LOG_EDIT:
			rule = e_rule_context_find_rank_rule (editor->context, undo->rank, undo->rule->source);
			if (rule) {
				e_filter_rule_copy (rule, undo->rule);
			} else {
				g_warning ("Could not find the right rule to undo against?");
			}
			break;
		case E_RULE_EDITOR_LOG_ADD:
			rule = e_rule_context_find_rank_rule (editor->context, undo->rank, undo->rule->source);
			if (rule)
				e_rule_context_remove_rule (editor->context, rule);
			break;
		case E_RULE_EDITOR_LOG_REMOVE:
			g_object_ref (undo->rule);
			e_rule_context_add_rule (editor->context, undo->rule);
			e_rule_context_rank_rule (editor->context, undo->rule, editor->source, undo->rank);
			break;
		case E_RULE_EDITOR_LOG_RANK:
			rule = e_rule_context_find_rank_rule (editor->context, undo->newrank, undo->rule->source);
			if (rule)
				e_rule_context_rank_rule (editor->context, rule, editor->source, undo->rank);
			break;
		}

		g_object_unref (undo->rule);
		g_free (undo);
		undo = next;
	}
	editor->undo_active = FALSE;
}

static void
dialog_rule_changed (EFilterRule *fr, GtkWidget *dialog)
{
	g_return_if_fail (dialog != NULL);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, fr && fr->parts);
}

static void
add_editor_response (GtkWidget *dialog, gint button, ERuleEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;

	if (button == GTK_RESPONSE_OK) {
		EAlert *alert = NULL;
		if (!e_filter_rule_validate (editor->edit, &alert)) {
			e_alert_run_dialog (GTK_WINDOW (dialog), alert);
			g_object_unref (alert);
			return;
		}

		if (e_rule_context_find_rule (editor->context, editor->edit->name, editor->edit->source)) {
			e_alert_run_dialog_for_args ((GtkWindow *)dialog,
						     "filter:bad-name-notunique",
						     editor->edit->name, NULL);
			return;
		}

		g_object_ref (editor->edit);

		gtk_list_store_append (editor->model, &iter);
		gtk_list_store_set (editor->model, &iter, 0, editor->edit->name, 1, editor->edit, 2, editor->edit->enabled, -1);
		selection = gtk_tree_view_get_selection (editor->list);
		gtk_tree_selection_select_iter (selection, &iter);

		/* scroll to the newly added row */
		path = gtk_tree_model_get_path ((GtkTreeModel *) editor->model, &iter);
		gtk_tree_view_scroll_to_cell (editor->list, path, NULL, TRUE, 1.0, 0.0);
		gtk_tree_path_free (path);

		editor->current = editor->edit;
		e_rule_context_add_rule (editor->context, editor->current);

		g_object_ref (editor->current);
		rule_editor_add_undo (editor, E_RULE_EDITOR_LOG_ADD, editor->current,
				      e_rule_context_get_rank_rule (editor->context, editor->current, editor->current->source), 0);
	}

	gtk_widget_destroy (dialog);
}

static void
editor_destroy (ERuleEditor *editor,
                GObject *deadbeef)
{
	if (editor->edit) {
		g_object_unref (editor->edit);
		editor->edit = NULL;
	}

	editor->dialog = NULL;

	gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
	e_rule_editor_set_sensitive (editor);
}

static gboolean
update_selected_rule (ERuleEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (editor->list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (editor->model), &iter, 1, &editor->current, -1);
		return TRUE;
	}

	return FALSE;
}

static void
cursor_changed (GtkTreeView *treeview, ERuleEditor *editor)
{
	if (update_selected_rule (editor)) {
		g_return_if_fail (editor->current);

		e_rule_editor_set_sensitive (editor);
	}
}

static void
editor_response (GtkWidget *dialog, gint button, ERuleEditor *editor)
{
	if (button == GTK_RESPONSE_CANCEL) {
		if (enable_undo)
			rule_editor_play_undo (editor);
		else {
			ERuleEditorUndo *undo, *next;

			undo = editor->undo_log;
			editor->undo_log = NULL;
			while (undo) {
				next = undo->next;
				g_object_unref (undo->rule);
				g_free (undo);
				undo = next;
			}
		}
	}
}

static void
rule_add (GtkWidget *widget, ERuleEditor *editor)
{
	GtkWidget *rules;
	GtkWidget *content_area;

	if (editor->edit != NULL)
		return;

	editor->edit = e_rule_editor_create_rule (editor);
	e_filter_rule_set_source (editor->edit, editor->source);
	rules = e_filter_rule_get_widget (editor->edit, editor->context);

	editor->dialog = gtk_dialog_new ();
	gtk_dialog_add_buttons ((GtkDialog *) editor->dialog,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (editor->dialog, "has-separator", FALSE, NULL);
#endif

	gtk_window_set_title ((GtkWindow *) editor->dialog, _("Add Rule"));
	gtk_window_set_default_size (GTK_WINDOW (editor->dialog), 650, 400);
	gtk_window_set_resizable (GTK_WINDOW (editor->dialog), TRUE);
	gtk_window_set_transient_for ((GtkWindow *) editor->dialog, (GtkWindow *) editor);
	gtk_container_set_border_width ((GtkContainer *) editor->dialog, 6);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (editor->dialog));
	gtk_box_pack_start (GTK_BOX (content_area), rules, TRUE, TRUE, 3);

	g_signal_connect (editor->dialog, "response", G_CALLBACK (add_editor_response), editor);
	g_object_weak_ref ((GObject *) editor->dialog, (GWeakNotify) editor_destroy, editor);

	g_signal_connect (editor->edit, "changed", G_CALLBACK (dialog_rule_changed), editor->dialog);
	dialog_rule_changed (editor->edit, editor->dialog);

	gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);

	gtk_widget_show (editor->dialog);
}

static void
edit_editor_response (GtkWidget *dialog, gint button, ERuleEditor *editor)
{
	EFilterRule *rule;
	GtkTreePath *path;
	GtkTreeIter iter;
	gint pos;

	if (button == GTK_RESPONSE_OK) {
		EAlert *alert = NULL;
		if (!e_filter_rule_validate (editor->edit, &alert)) {
			e_alert_run_dialog (GTK_WINDOW (dialog), alert);
			g_object_unref (alert);
			return;
		}

		rule = e_rule_context_find_rule (editor->context, editor->edit->name, editor->edit->source);
		if (rule != NULL && rule != editor->current) {
			e_alert_run_dialog_for_args ((GtkWindow *)dialog,
						     "filter:bad-name-notunique",
						     rule->name, NULL);

			return;
		}

		pos = e_rule_context_get_rank_rule (editor->context, editor->current, editor->source);
		if (pos != -1) {
			path = gtk_tree_path_new ();
			gtk_tree_path_append_index (path, pos);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (editor->model), &iter, path);
			gtk_tree_path_free (path);

			gtk_list_store_set (editor->model, &iter, 0, editor->edit->name, -1);

			rule_editor_add_undo (editor, E_RULE_EDITOR_LOG_EDIT, e_filter_rule_clone (editor->current),
					      pos, 0);

			/* replace the old rule with the new rule */
			e_filter_rule_copy (editor->current, editor->edit);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
rule_edit (GtkWidget *widget, ERuleEditor *editor)
{
	GtkWidget *rules;
	GtkWidget *content_area;

	update_selected_rule (editor);

	if (editor->current == NULL || editor->edit != NULL)
		return;

	editor->edit = e_filter_rule_clone (editor->current);

	rules = e_filter_rule_get_widget (editor->edit, editor->context);

	editor->dialog = gtk_dialog_new ();
	gtk_dialog_add_buttons ((GtkDialog *) editor->dialog,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (editor->dialog, "has-separator", FALSE, NULL);
#endif

	gtk_window_set_title ((GtkWindow *) editor->dialog, _("Edit Rule"));
	gtk_window_set_default_size (GTK_WINDOW (editor->dialog), 650, 400);
	gtk_window_set_resizable (GTK_WINDOW (editor->dialog), TRUE);
	gtk_widget_set_parent_window (
		GTK_WIDGET (editor->dialog),
		gtk_widget_get_window (GTK_WIDGET (editor)));
	gtk_container_set_border_width ((GtkContainer *) editor->dialog, 6);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (editor->dialog));
	gtk_box_pack_start (GTK_BOX (content_area), rules, TRUE, TRUE, 3);

	g_signal_connect (editor->dialog, "response", G_CALLBACK (edit_editor_response), editor);
	g_object_weak_ref ((GObject *) editor->dialog, (GWeakNotify) editor_destroy, editor);

	g_signal_connect (editor->edit, "changed", G_CALLBACK (dialog_rule_changed), editor->dialog);
	dialog_rule_changed (editor->edit, editor->dialog);

	gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);

	gtk_widget_show (editor->dialog);
}

static void
rule_delete (GtkWidget *widget, ERuleEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	gint pos, len;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (editor->context, editor->current, editor->source);
	if (pos != -1) {
		e_rule_context_remove_rule (editor->context, editor->current);

		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, pos);
		gtk_tree_model_get_iter (GTK_TREE_MODEL (editor->model), &iter, path);
		gtk_list_store_remove (editor->model, &iter);
		gtk_tree_path_free (path);

		rule_editor_add_undo (editor, E_RULE_EDITOR_LOG_REMOVE, editor->current,
				      e_rule_context_get_rank_rule (editor->context, editor->current, editor->current->source), 0);
#if 0
		g_object_unref (editor->current);
#endif
		editor->current = NULL;

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
rule_move (ERuleEditor *editor, gint from, gint to)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	EFilterRule *rule;

	rule_editor_add_undo (
		editor, E_RULE_EDITOR_LOG_RANK,
		g_object_ref (editor->current),
		e_rule_context_get_rank_rule (editor->context,
		editor->current, editor->source), to);

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
rule_top (GtkWidget *widget, ERuleEditor *editor)
{
	gint pos;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (
		editor->context, editor->current, editor->source);
	if (pos > 0)
		rule_move (editor, pos, 0);
}

static void
rule_up (GtkWidget *widget, ERuleEditor *editor)
{
	gint pos;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (
		editor->context, editor->current, editor->source);
	if (pos > 0)
		rule_move (editor, pos, pos - 1);
}

static void
rule_down (GtkWidget *widget, ERuleEditor *editor)
{
	gint pos;

	update_selected_rule (editor);

	pos = e_rule_context_get_rank_rule (
		editor->context, editor->current, editor->source);
	if (pos >= 0)
		rule_move (editor, pos, pos + 1);
}

static void
rule_bottom (GtkWidget *widget, ERuleEditor *editor)
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
	ERuleEditorUndo *undo, *next;

	g_object_unref (editor->context);

	undo = editor->undo_log;
	while (undo) {
		next = undo->next;
		g_object_unref (undo->rule);
		g_free (undo);
		undo = next;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_rule_editor_parent_class)->finalize (object);
}

static void
rule_editor_destroy (GtkObject *gtk_object)
{
	ERuleEditor *editor = E_RULE_EDITOR (gtk_object);

	if (editor->dialog != NULL) {
		gtk_widget_destroy (GTK_WIDGET (editor->dialog));
		editor->dialog = NULL;
	}

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (e_rule_editor_parent_class)->destroy (gtk_object);
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
	GtkObjectClass *gtk_object_class;

	g_type_class_add_private (class, sizeof (ERuleEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = rule_editor_finalize;

	gtk_object_class = GTK_OBJECT_CLASS (class);
	gtk_object_class->destroy = rule_editor_destroy;

	class->set_source = rule_editor_set_source;
	class->set_sensitive = rule_editor_set_sensitive;
	class->create_rule = rule_editor_create_rule;

	/* TODO: Remove when it works (or never will) */
	enable_undo = getenv ("EVOLUTION_RULE_UNDO") != NULL;
}

static void
e_rule_editor_init (ERuleEditor *editor)
{
	editor->priv = E_RULE_EDITOR_GET_PRIVATE (editor);
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
	g_return_if_fail (class->set_source != NULL);

	class->set_source (editor, source);
}

EFilterRule *
e_rule_editor_create_rule (ERuleEditor *editor)
{
	ERuleEditorClass *class;

	g_return_val_if_fail (E_IS_RULE_EDITOR (editor), NULL);

	class = E_RULE_EDITOR_GET_CLASS (editor);
	g_return_val_if_fail (class->create_rule != NULL, NULL);

	return class->create_rule (editor);
}

static void
double_click (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, ERuleEditor *editor)
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

void
e_rule_editor_construct (ERuleEditor *editor,
                         ERuleContext *context,
                         GtkBuilder *builder,
                         const gchar *source,
                         const gchar *label)
{
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
	list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	renderer = GTK_CELL_RENDERER (list->data);
	g_warn_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (renderer));

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

	g_signal_connect (
		editor, "response",
		G_CALLBACK (editor_response), editor);
	rule_editor_set_source (editor, source);

#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (editor, "has-separator", FALSE, NULL);
#endif
	gtk_dialog_add_buttons ((GtkDialog *) editor,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
}
