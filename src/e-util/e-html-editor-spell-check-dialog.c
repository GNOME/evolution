/*
 * e-html-editor-spell-dialog.c
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "evolution-config.h"

#include "e-html-editor-spell-check-dialog.h"

#include <glib/gi18n-lib.h>
#include <enchant.h>

#include "e-spell-checker.h"
#include "e-spell-dictionary.h"

#include "e-dialog-widgets.h"

struct _EHTMLEditorSpellCheckDialogPrivate {
	GtkWidget *add_word_button;
	GtkWidget *back_button;
	GtkWidget *dictionary_combo;
	GtkWidget *ignore_button;
	GtkWidget *replace_button;
	GtkWidget *replace_all_button;
	GtkWidget *skip_button;
	GtkWidget *suggestion_label;
	GtkWidget *tree_view;

	gchar *word;
	ESpellDictionary *current_dict;
};

enum {
	COLUMN_NAME,
	COLUMN_DICTIONARY,
	NUM_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (EHTMLEditorSpellCheckDialog, e_html_editor_spell_check_dialog, E_TYPE_HTML_EDITOR_DIALOG)

static void
html_editor_spell_check_dialog_set_word (EHTMLEditorSpellCheckDialog *dialog,
                                         const gchar *word)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GtkTreeView *tree_view;
	GtkListStore *store;
	gchar *markup;
	GList *list, *link;
	gboolean empty;

	if (word == NULL)
		return;

	if (dialog->priv->word != word) {
		g_free (dialog->priv->word);
		dialog->priv->word = g_strdup (word);
	}

	markup = g_strdup_printf (_("<b>Suggestions for “%s”</b>"), word);
	gtk_label_set_markup (
		GTK_LABEL (dialog->priv->suggestion_label), markup);
	g_free (markup);

	tree_view = GTK_TREE_VIEW (dialog->priv->tree_view);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (tree_view));
	gtk_list_store_clear (store);

	list = e_spell_dictionary_get_suggestions (
		dialog->priv->current_dict, word, -1);

	empty = list == NULL;

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreeIter iter;
		gchar *suggestion = link->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, suggestion, -1);
	}

	gtk_widget_set_sensitive (dialog->priv->replace_button, !empty);
	gtk_widget_set_sensitive (dialog->priv->replace_all_button, !empty);

	if (!empty) {
		GtkTreeSelection *tree_selection;

		/* Select the first suggestion */
		tree_selection = gtk_tree_view_get_selection (
			GTK_TREE_VIEW (dialog->priv->tree_view));
		gtk_tree_selection_select_path (tree_selection, gtk_tree_path_new_first ());
	}

	g_list_free_full (list, (GDestroyNotify) g_free);

	/* We give focus to WebKit so that the currently selected word
	 * is highlited. Without focus selection is not visible (at
	 * least with my default color scheme). The focus in fact is not
	 * given to WebKit, because this dialog is modal, but it satisfies
	 * it in a way that it paints the selection :) */
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);
	gtk_widget_grab_focus (GTK_WIDGET (cnt_editor));
}

static gboolean
html_editor_spell_check_dialog_next (EHTMLEditorSpellCheckDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *next_word;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	next_word = e_content_editor_spell_check_next_word (cnt_editor, dialog->priv->word);
	if (next_word && *next_word) {
		html_editor_spell_check_dialog_set_word (dialog, next_word);
		g_free (next_word);
		return TRUE;
	}
	g_free (next_word);

	/* Close the dialog */
	gtk_widget_hide (GTK_WIDGET (dialog));
	return FALSE;
}

static gboolean
html_editor_spell_check_dialog_prev (EHTMLEditorSpellCheckDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *prev_word;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	prev_word = e_content_editor_spell_check_prev_word (cnt_editor, dialog->priv->word);
	if (prev_word && *prev_word) {
		html_editor_spell_check_dialog_set_word (dialog, prev_word);
		g_free (prev_word);
		return TRUE;
	}
	g_free (prev_word);

	/* Close the dialog */
	gtk_widget_hide (GTK_WIDGET (dialog));
	return FALSE;
}

static gboolean
html_editor_spell_check_dialog_next_idle_cb (gpointer user_data)
{
	EHTMLEditorSpellCheckDialog *dialog = user_data;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SPELL_CHECK_DIALOG (dialog), FALSE);

	html_editor_spell_check_dialog_next (dialog);
	g_object_unref (dialog);

	return FALSE;
}

static void
html_editor_spell_check_dialog_replace (EHTMLEditorSpellCheckDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gchar *replacement;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (dialog->priv->tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	gtk_tree_model_get (model, &iter, 0, &replacement, -1);

	e_content_editor_replace (cnt_editor, replacement);

	g_free (replacement);

	g_idle_add (html_editor_spell_check_dialog_next_idle_cb, g_object_ref (dialog));
}

static void
html_editor_spell_check_dialog_replace_all (EHTMLEditorSpellCheckDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gchar *replacement;

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (dialog->priv->tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	gtk_tree_model_get (model, &iter, 0, &replacement, -1);

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_replace_all (
		cnt_editor,
		E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE |
		E_CONTENT_EDITOR_FIND_WRAP_AROUND,
		dialog->priv->word,
		replacement);

	g_free (replacement);

	g_idle_add (html_editor_spell_check_dialog_next_idle_cb, g_object_ref (dialog));
}

static void
html_editor_spell_check_dialog_ignore (EHTMLEditorSpellCheckDialog *dialog)
{
	if (dialog->priv->word == NULL)
		return;
	e_spell_dictionary_ignore_word (
		dialog->priv->current_dict, dialog->priv->word, -1);

	html_editor_spell_check_dialog_next (dialog);
}

static void
html_editor_spell_check_dialog_learn (EHTMLEditorSpellCheckDialog *dialog)
{
	if (dialog->priv->word == NULL)
		return;

	e_spell_dictionary_learn_word (dialog->priv->current_dict, dialog->priv->word, -1);

	html_editor_spell_check_dialog_next (dialog);
}

static void
html_editor_spell_check_dialog_set_dictionary (EHTMLEditorSpellCheckDialog *dialog)
{
	GtkComboBox *combo_box;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESpellDictionary *dictionary;

	combo_box = GTK_COMBO_BOX (dialog->priv->dictionary_combo);
	if (gtk_combo_box_get_active_iter (combo_box, &iter)) {
		model = gtk_combo_box_get_model (combo_box);

		gtk_tree_model_get (model, &iter, 1, &dictionary, -1);

		dialog->priv->current_dict = dictionary;

		/* Update suggestions */
		html_editor_spell_check_dialog_set_word (dialog, dialog->priv->word);
	}
}

static void
html_editor_spell_check_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EHTMLEditorSpellCheckDialog *dialog;

	dialog = E_HTML_EDITOR_SPELL_CHECK_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	g_free (dialog->priv->word);
	dialog->priv->word = NULL;

	e_content_editor_on_dialog_open (cnt_editor, E_CONTENT_EDITOR_DIALOG_SPELLCHECK);

	GTK_WIDGET_CLASS (e_html_editor_spell_check_dialog_parent_class)->show (widget);

	/* Select the first word or quit */
	if (!html_editor_spell_check_dialog_next (dialog))
		e_content_editor_on_dialog_close (cnt_editor, E_CONTENT_EDITOR_DIALOG_SPELLCHECK);
}

static void
html_editor_spell_check_dialog_hide (GtkWidget *widget)
{
	EContentEditor *cnt_editor;
	EHTMLEditor *editor;
	EHTMLEditorSpellCheckDialog *dialog = E_HTML_EDITOR_SPELL_CHECK_DIALOG (widget);

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_dialog_close (cnt_editor, E_CONTENT_EDITOR_DIALOG_SPELLCHECK);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_spell_check_dialog_parent_class)->hide (widget);
}

static void
html_editor_spell_check_dialog_finalize (GObject *object)
{
	EHTMLEditorSpellCheckDialog *self = E_HTML_EDITOR_SPELL_CHECK_DIALOG (object);

	g_free (self->priv->word);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_html_editor_spell_check_dialog_parent_class)->finalize (object);
}

static void
html_editor_spell_check_dialog_constructed (GObject *object)
{
	EHTMLEditorSpellCheckDialog *dialog;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_html_editor_spell_check_dialog_parent_class)->constructed (object);

	dialog = E_HTML_EDITOR_SPELL_CHECK_DIALOG (object);

	e_html_editor_spell_check_dialog_update_dictionaries (dialog);
}

static void
e_html_editor_spell_check_dialog_class_init (EHTMLEditorSpellCheckDialogClass *class)
{
	GtkWidgetClass *widget_class;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = html_editor_spell_check_dialog_finalize;
	object_class->constructed = html_editor_spell_check_dialog_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_spell_check_dialog_show;
	widget_class->hide = html_editor_spell_check_dialog_hide;
}

static void
e_html_editor_spell_check_dialog_init (EHTMLEditorSpellCheckDialog *dialog)
{
	GtkWidget *widget;
	GtkGrid *main_layout;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	dialog->priv = e_html_editor_spell_check_dialog_get_instance_private (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == Suggestions == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Suggestions</b>"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 0, 2, 1);
	dialog->priv->suggestion_label = widget;

	/* Tree view */
	widget = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_widget_set_hexpand (widget, TRUE);
	dialog->priv->tree_view = widget;

	/* Column */
	column = gtk_tree_view_column_new_with_attributes (
		"", gtk_cell_renderer_text_new (), "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	/* Store */
	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (
		GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));

	/* Scrolled Window */
	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_size_request (widget, 150, -1);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget),
		GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (widget), dialog->priv->tree_view);
	gtk_grid_attach (main_layout, widget, 0, 1, 1, 5);

	/* Replace */
	widget = e_dialog_button_new_with_icon ("edit-find-replace", _("Replace"));
	gtk_grid_attach (main_layout, widget, 1, 1, 1, 1);
	dialog->priv->replace_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_spell_check_dialog_replace), dialog);

	/* Replace All */
	widget = gtk_button_new_with_mnemonic (_("Replace All"));
	gtk_grid_attach (main_layout, widget, 1, 2, 1, 1);
	dialog->priv->replace_all_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_spell_check_dialog_replace_all), dialog);

	/* Ignore */
	widget = e_dialog_button_new_with_icon ("edit-clear", _("Ignore"));
	gtk_grid_attach (main_layout, widget, 1, 3, 1, 1);
	dialog->priv->ignore_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_spell_check_dialog_ignore), dialog);

	/* Skip */
	widget = e_dialog_button_new_with_icon ("go-next", _("Skip"));
	gtk_grid_attach (main_layout, widget, 1, 4, 1, 1);
	dialog->priv->skip_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_spell_check_dialog_next), dialog);

	/* Back */
	widget = e_dialog_button_new_with_icon ("go-previous", _("Back"));
	gtk_grid_attach (main_layout, widget, 1, 5, 1, 1);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_spell_check_dialog_prev), dialog);

	/* Dictionary label */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Dictionary</b>"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_yalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 6, 2, 1);

	/* Dictionaries combo */
	widget = gtk_combo_box_new ();
	gtk_grid_attach (main_layout, widget, 0, 7, 1, 1);
	dialog->priv->dictionary_combo = widget;

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (widget), renderer, "text", 0);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_spell_check_dialog_set_dictionary), dialog);

	/* Add Word button */
	widget = e_dialog_button_new_with_icon ("list-add", _("Add word"));
	gtk_grid_attach (main_layout, widget, 1, 7, 1, 1);
	dialog->priv->add_word_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_spell_check_dialog_learn), dialog);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_spell_check_dialog_new (EHTMLEditor *editor)
{
	return g_object_new (
		E_TYPE_HTML_EDITOR_SPELL_CHECK_DIALOG,
		"editor", editor,
		"title", _("Spell Checking"),
		NULL);
}

void
e_html_editor_spell_check_dialog_update_dictionaries (EHTMLEditorSpellCheckDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	GtkComboBox *combo_box;
	GtkListStore *store = NULL;
	GQueue queue = G_QUEUE_INIT;
	gchar **languages;
	guint n_languages = 0;
	guint ii;

	g_return_if_fail (E_IS_HTML_EDITOR_SPELL_CHECK_DIALOG (dialog));

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);

	languages = e_spell_checker_list_active_languages (
		spell_checker, &n_languages);
	for (ii = 0; ii < n_languages; ii++) {
		ESpellDictionary *dictionary;

		dictionary = e_spell_checker_ref_dictionary (
			spell_checker, languages[ii]);
		if (dictionary != NULL)
			g_queue_push_tail (&queue, dictionary);
		else
			g_warning (
				"%s: No '%s' dictionary found",
				G_STRFUNC, languages[ii]);
	}
	g_strfreev (languages);

	/* Populate a list store for the combo box. */
	store = gtk_list_store_new (
		NUM_COLUMNS,
		G_TYPE_STRING,			/* COLUMN_NAME */
		E_TYPE_SPELL_DICTIONARY);	/* COLUMN_DICTIONARY */

	while (!g_queue_is_empty (&queue)) {
		ESpellDictionary *dictionary;
		GtkTreeIter iter;
		const gchar *name = NULL;

		dictionary = g_queue_pop_head (&queue);
		name = e_spell_dictionary_get_name (dictionary);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			COLUMN_NAME, name,
			COLUMN_DICTIONARY, dictionary,
			-1);

		g_object_unref (dictionary);
	}

	/* FIXME Try to restore selection. */
	combo_box = GTK_COMBO_BOX (dialog->priv->dictionary_combo);
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));
	gtk_combo_box_set_active (combo_box, 0);

	g_object_unref (store);
	g_clear_object (&spell_checker);
}

