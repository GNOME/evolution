/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* for getenv only, remove when getenv need removed */
#include <stdlib.h>
#include <string.h>

#include <libgnome/gnome-i18n.h>

#include "widgets/misc/e-error.h"
#include "rule-editor.h"

static int enable_undo = 0;

void rule_editor_add_undo (RuleEditor *re, int type, FilterRule *rule, int rank, int newrank);
void rule_editor_play_undo (RuleEditor *re);

#define d(x)

static void set_source (RuleEditor *re, const char *source);
static void set_sensitive (RuleEditor *re);
static FilterRule *create_rule (RuleEditor *re);

static void cursor_changed (GtkTreeView *treeview, RuleEditor *re);

static void rule_editor_class_init (RuleEditorClass *klass);
static void rule_editor_init (RuleEditor *re);
static void rule_editor_finalise (GObject *obj);
static void rule_editor_destroy (GtkObject *obj);

#define _PRIVATE(x)(((RuleEditor *)(x))->priv)

enum {
	BUTTON_ADD,
	BUTTON_EDIT,
	BUTTON_DELETE,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LAST
};

struct _RuleEditorPrivate {
	GtkButton *buttons[BUTTON_LAST];
};

static GtkDialogClass *parent_class = NULL;


GtkType
rule_editor_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (RuleEditorClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) rule_editor_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (RuleEditor),
			0,    /* n_preallocs */
			(GInstanceInitFunc) rule_editor_init,
		};
		
		/* TODO: Remove when it works (or never will) */
		enable_undo = getenv ("EVOLUTION_RULE_UNDO") != NULL;
		
		type = g_type_register_static (gtk_dialog_get_type (), "RuleEditor", &info, 0);
	}
	
	return type;
}

static void
rule_editor_class_init (RuleEditorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	parent_class = g_type_class_ref(gtk_dialog_get_type ());
	
	gobject_class->finalize = rule_editor_finalise;
	object_class->destroy = rule_editor_destroy;
	
	/* override methods */
	klass->set_source = set_source;
	klass->set_sensitive = set_sensitive;
	klass->create_rule = create_rule;
}

static void
rule_editor_init (RuleEditor *re)
{
	re->priv = g_malloc0 (sizeof (*re->priv));
}

static void
rule_editor_finalise (GObject *obj)
{
	RuleEditor *re = (RuleEditor *)obj;
	RuleEditorUndo *undo, *next;
	
	g_object_unref (re->context);
	g_free (re->priv);
	
	undo = re->undo_log;
	while (undo) {
		next = undo->next;
		g_object_unref (undo->rule);
		g_free (undo);
		undo = next;
	}
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
rule_editor_destroy (GtkObject *obj)
{
	RuleEditor *re = (RuleEditor *) obj;
	
	if (re->dialog) {
		gtk_widget_destroy (GTK_WIDGET (re->dialog));
		re->dialog = NULL;
	}
	
	((GtkObjectClass *)(parent_class))->destroy (obj);
}

/**
 * rule_editor_new:
 *
 * Create a new RuleEditor object.
 * 
 * Return value: A new #RuleEditor object.
 **/
RuleEditor *
rule_editor_new (RuleContext *rc, const char *source, const char *label)
{
	RuleEditor *re = (RuleEditor *) g_object_new (RULE_TYPE_EDITOR, NULL);
	GladeXML *gui;
	
	gui = glade_xml_new (FILTER_GLADEDIR "/filter.glade", "rule_editor", NULL);
	rule_editor_construct (re, rc, gui, source, label);
        gtk_widget_hide(glade_xml_get_widget (gui, "filter_source"));	
	g_object_unref (gui);
	
	return re;
}

/* used internally by implementations if required */
void
rule_editor_set_sensitive (RuleEditor *re)
{
	RULE_EDITOR_GET_CLASS (re)->set_sensitive (re);
}

/* used internally by implementations */
void
rule_editor_set_source (RuleEditor *re, const char *source)
{
	RULE_EDITOR_GET_CLASS (re)->set_source (re, source);
}

/* factory method for "add" button */
FilterRule *
rule_editor_create_rule (RuleEditor *re)
{
	return RULE_EDITOR_GET_CLASS (re)->create_rule (re);
}

static FilterRule *
create_rule (RuleEditor *re)
{
	FilterRule *rule = filter_rule_new ();
	FilterPart *part;
	
	/* create a rule with 1 part in it */
	part = rule_context_next_part (re->context, NULL);
	filter_rule_add_part (rule, filter_part_clone (part));
	
	return rule;
}

static void
editor_destroy (RuleEditor *re, GObject *deadbeef)
{
	if (re->edit) {
		g_object_unref (re->edit);
		re->edit = NULL;
	}
	
	re->dialog = NULL;
	
	gtk_widget_set_sensitive (GTK_WIDGET (re), TRUE);
	rule_editor_set_sensitive (re);
}

static void
add_editor_response (GtkWidget *dialog, int button, RuleEditor *re)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (button == GTK_RESPONSE_OK) {
		if (!filter_rule_validate (re->edit)) {
			/* no need to popup a dialog because the validate code does that. */
			return;
		}
		
		if (rule_context_find_rule (re->context, re->edit->name, re->edit->source)) {
			e_error_run((GtkWindow *)dialog, "filter:bad-name-notunique", re->edit->name, NULL);
			return;
		}
		
		g_object_ref (re->edit);
		
		gtk_list_store_append (re->model, &iter);
		gtk_list_store_set (re->model, &iter, 0, re->edit->name, 1, re->edit, -1);
		selection = gtk_tree_view_get_selection (re->list);
		gtk_tree_selection_select_iter (selection, &iter);
		
		/* scroll to the newly added row */
		path = gtk_tree_model_get_path ((GtkTreeModel *) re->model, &iter);
		gtk_tree_view_scroll_to_cell (re->list, path, NULL, TRUE, 1.0, 0.0);
		gtk_tree_path_free (path);
		
		re->current = re->edit;
		rule_context_add_rule (re->context, re->current);
		
		g_object_ref (re->current);
		rule_editor_add_undo (re, RULE_EDITOR_LOG_ADD, re->current,
				      rule_context_get_rank_rule (re->context, re->current, re->current->source), 0);
	}
	
	gtk_widget_destroy (dialog);
}

static void
rule_add (GtkWidget *widget, RuleEditor *re)
{
	GtkWidget *rules;
	
	if (re->edit != NULL)
		return;
	
	re->edit = rule_editor_create_rule (re);
	filter_rule_set_source (re->edit, re->source);
	rules = filter_rule_get_widget (re->edit, re->context);
	
	re->dialog = gtk_dialog_new ();
	gtk_dialog_add_buttons ((GtkDialog *) re->dialog,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_has_separator ((GtkDialog *) re->dialog, FALSE);
	
	gtk_window_set_title ((GtkWindow *) re->dialog, _("Add Rule"));
	gtk_window_set_default_size (GTK_WINDOW (re->dialog), 650, 400);
	gtk_window_set_resizable (GTK_WINDOW (re->dialog), TRUE);
	gtk_window_set_transient_for ((GtkWindow *) re->dialog, (GtkWindow *) re);
	gtk_container_set_border_width ((GtkContainer *) re->dialog, 6);
	
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (re->dialog)->vbox), rules, TRUE, TRUE, 3);
	
	g_signal_connect (re->dialog, "response", G_CALLBACK (add_editor_response), re);
	g_object_weak_ref ((GObject *) re->dialog, (GWeakNotify) editor_destroy, re);
	
	gtk_widget_set_sensitive (GTK_WIDGET (re), FALSE);
	
	gtk_widget_show (re->dialog);
}

static void
edit_editor_response (GtkWidget *dialog, int button, RuleEditor *re)
{
	FilterRule *rule;
	GtkTreePath *path;
	GtkTreeIter iter;
	int pos;
	
	if (button == GTK_RESPONSE_OK) {
		if (!filter_rule_validate (re->edit)) {
			/* no need to popup a dialog because the validate code does that. */
			return;
		}
		
		rule = rule_context_find_rule (re->context, re->edit->name, re->edit->source);
		if (rule != NULL && rule != re->current) {
			e_error_run((GtkWindow *)dialog, "filter:bad-name-notunique", rule->name, NULL);
			
			return;
		}
		
		pos = rule_context_get_rank_rule (re->context, re->current, re->source);
		if (pos != -1) {
			path = gtk_tree_path_new ();
			gtk_tree_path_append_index (path, pos);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (re->model), &iter, path);
			gtk_tree_path_free (path);
			
			gtk_list_store_set (re->model, &iter, 0, re->edit->name, -1);
			
			rule_editor_add_undo (re, RULE_EDITOR_LOG_EDIT, filter_rule_clone (re->current),
					      pos, 0);
			
			/* replace the old rule with the new rule */
			filter_rule_copy (re->current, re->edit);
		}
	}
	
	gtk_widget_destroy (dialog);
}

static void
rule_edit (GtkWidget *widget, RuleEditor *re)
{
	GtkWidget *rules;
	
	if (re->current == NULL || re->edit != NULL)
		return;
	
	re->edit = filter_rule_clone (re->current);
	
	rules = filter_rule_get_widget (re->edit, re->context);
	
	re->dialog = gtk_dialog_new ();
	gtk_dialog_add_buttons ((GtkDialog *) re->dialog,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_has_separator ((GtkDialog *) re->dialog, FALSE);
	
	gtk_window_set_title ((GtkWindow *) re->dialog, _("Edit Rule"));
	gtk_window_set_default_size (GTK_WINDOW (re->dialog), 650, 400);
	gtk_window_set_resizable (GTK_WINDOW (re->dialog), TRUE);
	gtk_widget_set_parent_window (GTK_WIDGET (re->dialog), GTK_WIDGET (re)->window);
	gtk_container_set_border_width ((GtkContainer *) re->dialog, 6);
	
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (re->dialog)->vbox), rules, TRUE, TRUE, 3);
	
	g_signal_connect (re->dialog, "response", G_CALLBACK (edit_editor_response), re);
	g_object_weak_ref ((GObject *) re->dialog, (GWeakNotify) editor_destroy, re);
	
	gtk_widget_set_sensitive (GTK_WIDGET (re), FALSE);
	
	gtk_widget_show (re->dialog);
}

static void
rule_delete (GtkWidget *widget, RuleEditor *re)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	int pos, len;
	
	d(printf ("delete rule\n"));
	pos = rule_context_get_rank_rule (re->context, re->current, re->source);
	if (pos != -1) {
		rule_context_remove_rule (re->context, re->current);
		
		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, pos);
		gtk_tree_model_get_iter (GTK_TREE_MODEL (re->model), &iter, path);
		gtk_list_store_remove (re->model, &iter);
		gtk_tree_path_free (path);
		
		rule_editor_add_undo (re, RULE_EDITOR_LOG_REMOVE, re->current,
				      rule_context_get_rank_rule (re->context, re->current, re->current->source), 0);
#if 0		
		g_object_unref (re->current);
#endif
		re->current = NULL;
		
		/* now select the next rule */
		len = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (re->model), NULL);
		pos = pos >= len ? len - 1 : pos;
		
		if (pos >= 0) {
			path = gtk_tree_path_new ();
			gtk_tree_path_append_index (path, pos);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (re->model), &iter, path);
			gtk_tree_path_free (path);
			
			/* select the new row */
			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (re->list));
			gtk_tree_selection_select_iter (selection, &iter);
			
			/* scroll to the selected row */
			path = gtk_tree_model_get_path ((GtkTreeModel *) re->model, &iter);
			gtk_tree_view_scroll_to_cell (re->list, path, NULL, FALSE, 0.0, 0.0);
			gtk_tree_path_free (path);
			
			/* update our selection state */
			cursor_changed (re->list, re);
			return;
		}
	}
	
	rule_editor_set_sensitive (re);
}

static void
rule_move (RuleEditor *re, int from, int to)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	FilterRule *rule;
	
	g_object_ref (re->current);
	rule_editor_add_undo (re, RULE_EDITOR_LOG_RANK, re->current,
			      rule_context_get_rank_rule (re->context, re->current, re->source), to);
	
	d(printf ("moving %d to %d\n", from, to));
	rule_context_rank_rule (re->context, re->current, re->source, to);
	
	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, from);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (re->model), &iter, path);
	gtk_tree_path_free (path);
	
	gtk_tree_model_get (GTK_TREE_MODEL (re->model), &iter, 1, &rule, -1);
	g_assert (rule != NULL);
	
	/* remove and then re-insert the row at the new location */
	gtk_list_store_remove (re->model, &iter);
	gtk_list_store_insert (re->model, &iter, to);
	
	/* set the data on the row */
	gtk_list_store_set (re->model, &iter, 0, rule->name, 1, rule, -1);
	
	/* select the row */
	selection = gtk_tree_view_get_selection (re->list);
	gtk_tree_selection_select_iter (selection, &iter);
	
	/* scroll to the selected row */
	path = gtk_tree_model_get_path ((GtkTreeModel *) re->model, &iter);
	gtk_tree_view_scroll_to_cell (re->list, path, NULL, FALSE, 0.0, 0.0);
	gtk_tree_path_free (path);
	
	rule_editor_set_sensitive (re);
}

static void
rule_up (GtkWidget *widget, RuleEditor *re)
{
	int pos;
	
	d(printf ("up rule\n"));
	pos = rule_context_get_rank_rule (re->context, re->current, re->source);
	if (pos > 0)
		rule_move (re, pos, pos - 1);
}

static void
rule_down (GtkWidget *widget, RuleEditor *re)
{
	int pos;
	
	d(printf ("down rule\n"));
	pos = rule_context_get_rank_rule (re->context, re->current, re->source);
	if (pos >= 0)
		rule_move (re, pos, pos + 1);
}

static struct {
	char *name;
	GtkSignalFunc func;
} edit_buttons[] = {
	{ "rule_add",    G_CALLBACK (rule_add)    },
	{ "rule_edit",   G_CALLBACK (rule_edit)   },
	{ "rule_delete", G_CALLBACK (rule_delete) },
	{ "rule_up",     G_CALLBACK (rule_up)     },
	{ "rule_down",   G_CALLBACK (rule_down)   },
};

static void
set_sensitive (RuleEditor *re)
{
	FilterRule *rule = NULL;
	int index = -1, count = 0;
	
	while ((rule = rule_context_next_rule (re->context, rule, re->source))) {
		if (rule == re->current)
			index = count;
		count++;
	}
	
	d(printf("index = %d count=%d\n", index, count));
	
	count--;
	
	gtk_widget_set_sensitive (GTK_WIDGET (re->priv->buttons[BUTTON_EDIT]), index != -1);
	gtk_widget_set_sensitive (GTK_WIDGET (re->priv->buttons[BUTTON_DELETE]), index != -1);
	gtk_widget_set_sensitive (GTK_WIDGET (re->priv->buttons[BUTTON_UP]), index > 0);
	gtk_widget_set_sensitive (GTK_WIDGET (re->priv->buttons[BUTTON_DOWN]), index >= 0 && index < count);
}


static void
cursor_changed (GtkTreeView *treeview, RuleEditor *re)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	selection = gtk_tree_view_get_selection (re->list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (re->model), &iter, 1, &re->current, -1);
		
		g_assert (re->current);
		
		rule_editor_set_sensitive (re);
	}
}

static void
double_click (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, RuleEditor *re)
{
	if (re->current)
		rule_edit ((GtkWidget *) treeview, re);
}

static void
set_source (RuleEditor *re, const char *source)
{
	FilterRule *rule = NULL;
	GtkTreeIter iter;
	
	gtk_list_store_clear (re->model);
	
	d(printf("Checking for rules that are of type %s\n", source ? source : "<nil>"));
	while ((rule = rule_context_next_rule (re->context, rule, source)) != NULL) {
		d(printf("Adding row '%s'\n", rule->name));
		gtk_list_store_append (re->model, &iter);
		gtk_list_store_set (re->model, &iter, 0, rule->name, 1, rule, -1);
	}
	
	g_free (re->source);
	re->source = g_strdup (source);
	re->current = NULL;
	rule_editor_set_sensitive (re);
}

void
rule_editor_add_undo (RuleEditor *re, int type, FilterRule *rule, int rank, int newrank)
{
	RuleEditorUndo *undo;
	
	if (!re->undo_active && enable_undo) {
		undo = g_malloc0 (sizeof (*undo));
		undo->rule = rule;
		undo->type = type;
		undo->rank = rank;
		undo->newrank = newrank;
		
		undo->next = re->undo_log;
		re->undo_log = undo;
	} else {
		g_object_unref (rule);
	}
}

void
rule_editor_play_undo (RuleEditor *re)
{
	RuleEditorUndo *undo, *next;
	FilterRule *rule;
	
	re->undo_active = TRUE;
	undo = re->undo_log;
	re->undo_log = NULL;
	while (undo) {
		next = undo->next;
		switch (undo->type) {
		case RULE_EDITOR_LOG_EDIT:
			d(printf ("Undoing edit on rule '%s'\n", undo->rule->name));
			rule = rule_context_find_rank_rule (re->context, undo->rank, undo->rule->source);
			if (rule) {
				d(printf (" name was '%s'\n", rule->name));
				filter_rule_copy (rule, undo->rule);
				d(printf (" name is '%s'\n", rule->name));
			} else {
				g_warning ("Could not find the right rule to undo against?");
			}
			break;
		case RULE_EDITOR_LOG_ADD:
			d(printf ("Undoing add on rule '%s'\n", undo->rule->name));
			rule = rule_context_find_rank_rule (re->context, undo->rank, undo->rule->source);
			if (rule)
				rule_context_remove_rule (re->context, rule);
			break;
		case RULE_EDITOR_LOG_REMOVE:
			d(printf ("Undoing remove on rule '%s'\n", undo->rule->name));
			g_object_ref (undo->rule);
			rule_context_add_rule (re->context, undo->rule);
			rule_context_rank_rule (re->context, undo->rule, re->source, undo->rank);
			break;
		case RULE_EDITOR_LOG_RANK:
			rule = rule_context_find_rank_rule (re->context, undo->newrank, undo->rule->source);
			if (rule)
				rule_context_rank_rule (re->context, rule, re->source, undo->rank);
			break;
		}
		
		g_object_unref (undo->rule);
		g_free (undo);
		undo = next;
	}
	re->undo_active = FALSE;
}

static void
editor_response (GtkWidget *dialog, int button, RuleEditor *re)
{
	if (button == GTK_RESPONSE_CANCEL) {
		if (enable_undo)
			rule_editor_play_undo (re);
		else {
			RuleEditorUndo *undo, *next;
			
			undo = re->undo_log;
			re->undo_log = 0;
			while (undo) {
				next = undo->next;
				g_object_unref (undo->rule);
				g_free (undo);
				undo = next;
			}
		}
	}
}

GtkWidget *rule_editor_treeview_new (char *widget_name, char *string1, char *string2,
				     int int1, int int2);

GtkWidget *
rule_editor_treeview_new (char *widget_name, char *string1, char *string2, int int1, int int2)
{
	GtkWidget *table, *scrolled;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *model;
	
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	table = gtk_tree_view_new_with_model ((GtkTreeModel *) model);
	gtk_tree_view_set_headers_visible ((GtkTreeView *) table, FALSE);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes ((GtkTreeView *) table, -1,
						     _("Rule name"), renderer,
						     "text", 0, NULL);
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) table);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	gtk_container_add (GTK_CONTAINER (scrolled), table);
	
	g_object_set_data ((GObject *) scrolled, "table", table);
	g_object_set_data ((GObject *) scrolled, "model", model);
	
	gtk_widget_show (scrolled);
	gtk_widget_show (table);
	
	return scrolled;
}

void
rule_editor_construct (RuleEditor *re, RuleContext *context, GladeXML *gui, const char *source, const char *label)
{
	GtkWidget *w;
	int i;
	char *tmp;
	
	re->context = context;
	g_object_ref (context);
	
	gtk_window_set_resizable ((GtkWindow *) re, TRUE);
	gtk_window_set_default_size ((GtkWindow *) re, 350, 400);
	gtk_widget_realize ((GtkWidget *) re);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *) re)->action_area, 12);

	w = glade_xml_get_widget(gui, "rule_editor");
	gtk_box_pack_start((GtkBox *)((GtkDialog *)re)->vbox, w, TRUE, TRUE, 3);

	for (i = 0; i < BUTTON_LAST; i++) {
		re->priv->buttons[i] = (GtkButton *) w = glade_xml_get_widget (gui, edit_buttons[i].name);
		g_signal_connect (w, "clicked", edit_buttons[i].func, re);
	}
	
	w = glade_xml_get_widget (gui, "rule_list");
	re->list = (GtkTreeView *) g_object_get_data ((GObject *) w, "table");
	re->model = (GtkListStore *) g_object_get_data ((GObject *) w, "model");
	
	g_signal_connect (re->list, "cursor-changed", G_CALLBACK (cursor_changed), re);
	g_signal_connect (re->list, "row-activated", G_CALLBACK (double_click), re);

	w = glade_xml_get_widget (gui, "rule_label");
	tmp = alloca(strlen(label)+8);
	sprintf(tmp, "<b>%s</b>", label);
	gtk_label_set_label((GtkLabel *)w, tmp);
	gtk_label_set_mnemonic_widget ((GtkLabel *) w, (GtkWidget *) re->list);

	g_signal_connect (re, "response", G_CALLBACK (editor_response), re);
	rule_editor_set_source (re, source);

	gtk_dialog_set_has_separator ((GtkDialog *) re, FALSE);
	gtk_dialog_add_buttons ((GtkDialog *) re,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
}
