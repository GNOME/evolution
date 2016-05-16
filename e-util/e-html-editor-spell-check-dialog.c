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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-html-editor-spell-check-dialog.h"

#include <glib/gi18n-lib.h>
#include <enchant/enchant.h>

#include "e-html-editor-view.h"
#include "e-spell-checker.h"
#include "e-spell-dictionary.h"

#include "e-dialog-widgets.h"

#define E_HTML_EDITOR_SPELL_CHECK_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_SPELL_CHECK_DIALOG, EHTMLEditorSpellCheckDialogPrivate))

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

	WebKitDOMDOMSelection *selection;

	gchar *word;
	ESpellDictionary *current_dict;
};

enum {
	COLUMN_NAME,
	COLUMN_DICTIONARY,
	NUM_COLUMNS
};

G_DEFINE_TYPE (
	EHTMLEditorSpellCheckDialog,
	e_html_editor_spell_check_dialog,
	E_TYPE_HTML_EDITOR_DIALOG)

static void
html_editor_spell_check_dialog_set_word (EHTMLEditorSpellCheckDialog *dialog,
                                         const gchar *word)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
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

	markup = g_strdup_printf (_("<b>Suggestions for '%s'</b>"), word);
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
	view = e_html_editor_get_view (editor);
	gtk_widget_grab_focus (GTK_WIDGET (view));
}

static gboolean
select_next_word (EHTMLEditorSpellCheckDialog *dialog)
{
	WebKitDOMNode *anchor, *focus;
	gulong anchor_offset, focus_offset;

	anchor = webkit_dom_dom_selection_get_anchor_node (dialog->priv->selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dialog->priv->selection);

	focus = webkit_dom_dom_selection_get_focus_node (dialog->priv->selection);
	focus_offset = webkit_dom_dom_selection_get_focus_offset (dialog->priv->selection);

	/* Jump _behind_ next word */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "move", "forward", "word");
	/* Jump before the word */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "extend", "forward", "word");

	/* If the selection didn't change, then we have most probably
	 * reached the end of document - return FALSE */
	return !((anchor == webkit_dom_dom_selection_get_anchor_node (
				dialog->priv->selection)) &&
		 (anchor_offset == webkit_dom_dom_selection_get_anchor_offset (
				dialog->priv->selection)) &&
		 (focus == webkit_dom_dom_selection_get_focus_node (
				dialog->priv->selection)) &&
		 (focus_offset == webkit_dom_dom_selection_get_focus_offset (
				dialog->priv->selection)));
}

static gboolean
html_editor_spell_check_dialog_next (EHTMLEditorSpellCheckDialog *dialog)
{
	WebKitDOMNode *start = NULL, *end = NULL;
	gulong start_offset = 0, end_offset = 0;

	if (dialog->priv->word == NULL) {
		webkit_dom_dom_selection_modify (
			dialog->priv->selection, "move", "left", "documentboundary");
	} else {
		/* Remember last selected word */
		start = webkit_dom_dom_selection_get_anchor_node (
			dialog->priv->selection);
		end = webkit_dom_dom_selection_get_focus_node (
			dialog->priv->selection);
		start_offset = webkit_dom_dom_selection_get_anchor_offset (
			dialog->priv->selection);
		end_offset = webkit_dom_dom_selection_get_focus_offset (
			dialog->priv->selection);
	}

	while (select_next_word (dialog)) {
		WebKitDOMRange *range;
		WebKitSpellChecker *checker;
		gint loc, len;
		gchar *word;

		range = webkit_dom_dom_selection_get_range_at (
			dialog->priv->selection, 0, NULL);
		word = webkit_dom_range_get_text (range);
		g_object_unref (range);

		checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
		webkit_spell_checker_check_spelling_of_string (
			checker, word, &loc, &len);

		/* Found misspelled word! */
		if (loc != -1) {
			html_editor_spell_check_dialog_set_word (dialog, word);
			g_free (word);
			return TRUE;
		}

		g_free (word);
	}

	/* Restore the selection to contain the last misspelled word. This is
	 * reached only when we reach the end of the document */
	if (start && end) {
		webkit_dom_dom_selection_set_base_and_extent (
			dialog->priv->selection, start, start_offset,
			end, end_offset, NULL);
	}

	/* Close the dialog */
	gtk_widget_hide (GTK_WIDGET (dialog));
	return FALSE;
}

static gboolean
select_previous_word (EHTMLEditorSpellCheckDialog *dialog)
{
	WebKitDOMNode *old_anchor_node;
	WebKitDOMNode *new_anchor_node;
	gulong old_anchor_offset;
	gulong new_anchor_offset;

	old_anchor_node = webkit_dom_dom_selection_get_anchor_node (
		dialog->priv->selection);
	old_anchor_offset = webkit_dom_dom_selection_get_anchor_offset (
		dialog->priv->selection);

	/* Jump on the beginning of current word */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "move", "backward", "word");
	/* Jump before previous word */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "extend", "forward", "word");

	/* If the selection start didn't change, then we have most probably
	 * reached the beginnig of document. Return FALSE */

	new_anchor_node = webkit_dom_dom_selection_get_anchor_node (
		dialog->priv->selection);
	new_anchor_offset = webkit_dom_dom_selection_get_anchor_offset (
		dialog->priv->selection);

	return (new_anchor_node != old_anchor_node) ||
		(new_anchor_offset != old_anchor_offset);
}

static gboolean
html_editor_spell_check_dialog_prev (EHTMLEditorSpellCheckDialog *dialog)
{
	WebKitDOMNode *start = NULL, *end = NULL;
	gulong start_offset = 0, end_offset = 0;

	if (dialog->priv->word == NULL) {
		webkit_dom_dom_selection_modify (
			dialog->priv->selection,
			"move", "right", "documentboundary");
		webkit_dom_dom_selection_modify (
			dialog->priv->selection,
			"extend", "backward", "word");
	} else {
		/* Remember last selected word */
		start = webkit_dom_dom_selection_get_anchor_node (
			dialog->priv->selection);
		end = webkit_dom_dom_selection_get_focus_node (
			dialog->priv->selection);
		start_offset = webkit_dom_dom_selection_get_anchor_offset (
			dialog->priv->selection);
		end_offset = webkit_dom_dom_selection_get_focus_offset (
			dialog->priv->selection);
	}

	while (select_previous_word (dialog)) {
		WebKitDOMRange *range;
		WebKitSpellChecker *checker;
		gint loc, len;
		gchar *word;

		range = webkit_dom_dom_selection_get_range_at (
			dialog->priv->selection, 0, NULL);
		word = webkit_dom_range_get_text (range);
		g_object_unref (range);

		checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
		webkit_spell_checker_check_spelling_of_string (
			checker, word, &loc, &len);

		/* Found misspelled word! */
		if (loc != -1) {
			html_editor_spell_check_dialog_set_word (dialog, word);
			g_free (word);
			return TRUE;
		}

		g_free (word);
	}

	/* Restore the selection to contain the last misspelled word. This is
	 * reached only when we reach the beginning of the document */
	if (start && end) {
		webkit_dom_dom_selection_set_base_and_extent (
			dialog->priv->selection, start, start_offset,
			end, end_offset, NULL);
	}

	/* Close the dialog */
	gtk_widget_hide (GTK_WIDGET (dialog));
	return FALSE;
}

static void
html_editor_spell_check_dialog_replace (EHTMLEditorSpellCheckDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	EHTMLEditorSelection *editor_selection;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gchar *replacement;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	editor_selection = e_html_editor_view_get_selection (view);

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (dialog->priv->tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	gtk_tree_model_get (model, &iter, 0, &replacement, -1);

	e_html_editor_selection_insert_html (
		editor_selection, replacement);

	g_free (replacement);
	html_editor_spell_check_dialog_next (dialog);
}

static void
html_editor_spell_check_dialog_replace_all (EHTMLEditorSpellCheckDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	EHTMLEditorSelection *editor_selection;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gchar *replacement;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	editor_selection = e_html_editor_view_get_selection (view);

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (dialog->priv->tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	gtk_tree_model_get (model, &iter, 0, &replacement, -1);

	/* Repeatedly search for 'word', then replace selection by
	 * 'replacement'. Repeat until there's at least one occurrence of
	 * 'word' in the document */
	while (webkit_web_view_search_text (
			WEBKIT_WEB_VIEW (view), dialog->priv->word,
			FALSE, TRUE, TRUE)) {

		e_html_editor_selection_insert_html (
			editor_selection, replacement);
	}

	g_free (replacement);
	html_editor_spell_check_dialog_next (dialog);
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

	e_spell_dictionary_learn_word (
		dialog->priv->current_dict, dialog->priv->word, -1);

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
	EHTMLEditorView *view;
	EHTMLEditorSpellCheckDialog *dialog;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;

	dialog = E_HTML_EDITOR_SPELL_CHECK_DIALOG (widget);

	g_free (dialog->priv->word);
	dialog->priv->word = NULL;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dialog->priv->selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	/* Select the first word or quit */
	if (html_editor_spell_check_dialog_next (dialog)) {
		GTK_WIDGET_CLASS (e_html_editor_spell_check_dialog_parent_class)->
			show (widget);
	}
}

static void
html_editor_spell_check_dialog_finalize (GObject *object)
{
	EHTMLEditorSpellCheckDialogPrivate *priv;

	priv = E_HTML_EDITOR_SPELL_CHECK_DIALOG_GET_PRIVATE (object);

	g_clear_object (&priv->selection);
	g_free (priv->word);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_html_editor_spell_check_dialog_parent_class)->
		finalize (object);
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

	g_type_class_add_private (
		class, sizeof (EHTMLEditorSpellCheckDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = html_editor_spell_check_dialog_finalize;
	object_class->constructed = html_editor_spell_check_dialog_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_spell_check_dialog_show;
}

static void
e_html_editor_spell_check_dialog_init (EHTMLEditorSpellCheckDialog *dialog)
{
	GtkWidget *widget;
	GtkGrid *main_layout;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	dialog->priv = E_HTML_EDITOR_SPELL_CHECK_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == Suggestions == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Suggestions</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
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
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0);
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
	EHTMLEditorView *view;
	ESpellChecker *spell_checker;
	GtkComboBox *combo_box;
	GtkListStore *store;
	GQueue queue = G_QUEUE_INIT;
	gchar **languages;
	guint n_languages = 0;
	guint ii;

	g_return_if_fail (E_IS_HTML_EDITOR_SPELL_CHECK_DIALOG (dialog));

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	spell_checker = e_html_editor_view_get_spell_checker (view);

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
		const gchar *name;

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
}

