/* e-editor-spell-dialog.c
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

#include "e-editor-spell-check-dialog.h"

#include <glib/gi18n-lib.h>
#include <enchant/enchant.h>

#include "e-editor-spell-checker.h"
#include "e-editor-widget.h"

G_DEFINE_TYPE (
	EEditorSpellCheckDialog,
	e_editor_spell_check_dialog,
	E_TYPE_EDITOR_DIALOG
);

enum {
	COMBO_COLUMN_DICTIONARY,	/* E_TYPE_SPELL_DICTIONARY */
	COMBO_COLUMN_TEXT		/* G_TYPE_STRING */
};

struct _EEditorSpellCheckDialogPrivate {
	GtkWidget *add_word_button;
	GtkWidget *back_button;
	GtkWidget *dictionary_combo;
	GtkWidget *ignore_button;
	GtkWidget *replace_button;
	GtkWidget *replace_all_button;
	GtkWidget *skip_button;
	GtkWidget *suggestion_label;
	GtkWidget *tree_view;

	GList *dictionaries;
	WebKitDOMDOMSelection *selection;

	gchar *word;
	EnchantDict *current_dict;
};

static void
editor_spell_check_dialog_set_word (EEditorSpellCheckDialog *dialog,
				    const gchar *word)
{
	GtkListStore *store;
	gchar *markup;
	gchar **suggestions;
	gint ii;

	if (word == NULL) {
		return;
	}

	if (dialog->priv->word != word) {
		g_free (dialog->priv->word);
		dialog->priv->word = g_strdup (word);
	}

	markup = g_strdup_printf (_("<b>Suggestions for '%s'</b>"), word);
	gtk_label_set_markup (
		GTK_LABEL (dialog->priv->suggestion_label), markup);
	g_free (markup);

	store = GTK_LIST_STORE (
			gtk_tree_view_get_model (
				GTK_TREE_VIEW (dialog->priv->tree_view)));
	gtk_list_store_clear (store);

	suggestions = enchant_dict_suggest (
			dialog->priv->current_dict, word, -1, NULL);
	for (ii = 0; suggestions && suggestions[ii]; ii++) {
		GtkTreeIter iter;
		gchar *suggestion = suggestions[ii];

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, suggestion, -1);
	}

	g_strfreev (suggestions);
}

static gboolean
select_next_word (EEditorSpellCheckDialog *dialog)
{
	WebKitDOMNode *anchor;
	gulong anchor_offset;

	anchor = webkit_dom_dom_selection_get_anchor_node (dialog->priv->selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dialog->priv->selection);

	/* Jump _behind_ next word */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "move", "forward", "word");
	/* Jump before the word */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (
		dialog->priv->selection, "extend", "forward", "word");

	/* If the selection start didn't change, then we have most probably
	 * reached the end of document - return FALSE */
	return ((anchor != webkit_dom_dom_selection_get_anchor_node (
				dialog->priv->selection)) ||
		(anchor_offset != webkit_dom_dom_selection_get_anchor_offset (
				dialog->priv->selection)));
}


static void
editor_spell_check_dialog_next (EEditorSpellCheckDialog *dialog)
{
	WebKitDOMNode *start = NULL, *end = NULL;
	gulong start_offset, end_offset;

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

		range = webkit_dom_dom_selection_get_range_at (dialog->priv->selection, 0, NULL);
		word = webkit_dom_range_get_text (range);

		checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
		webkit_spell_checker_check_spelling_of_string (
			checker, word, &loc, &len);

		/* Found misspelled word! */
		if (loc != -1) {
			editor_spell_check_dialog_set_word (dialog, word);
			g_free (word);
			return;
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
}

static gboolean
select_previous_word (EEditorSpellCheckDialog *dialog)
{
	WebKitDOMNode *anchor;
	gulong anchor_offset;

	anchor = webkit_dom_dom_selection_get_anchor_node (dialog->priv->selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dialog->priv->selection);

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
	return ((anchor != webkit_dom_dom_selection_get_anchor_node (
				dialog->priv->selection)) ||
		(anchor_offset != webkit_dom_dom_selection_get_anchor_offset (
				dialog->priv->selection)));
}

static void
editor_spell_check_dialog_prev (EEditorSpellCheckDialog *dialog)
{
	WebKitDOMNode *start = NULL, *end = NULL;
	gulong start_offset, end_offset;	

	if (dialog->priv->word == NULL) {
		webkit_dom_dom_selection_modify (
			dialog->priv->selection, "move", "right", "documentboundary");
		webkit_dom_dom_selection_modify (
			dialog->priv->selection, "extend", "backward", "word");
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

		range = webkit_dom_dom_selection_get_range_at (dialog->priv->selection, 0, NULL);
		word = webkit_dom_range_get_text (range);

		checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
		webkit_spell_checker_check_spelling_of_string (
				checker, word, &loc, &len);

		/* Found misspelled word! */
		if (loc != -1) {
			editor_spell_check_dialog_set_word (dialog, word);
			g_free (word);
			return;
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
}

static void
editor_spell_check_dialog_replace (EEditorSpellCheckDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *editor_selection;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gchar *replacement;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	editor_selection = e_editor_widget_get_selection (widget);

	selection = gtk_tree_view_get_selection (
			GTK_TREE_VIEW (dialog->priv->tree_view));
	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, 0, &replacement, -1);

	e_editor_selection_insert_html (
		editor_selection, replacement);

	g_free (replacement);
	editor_spell_check_dialog_next (dialog);
}

static void
editor_spell_check_dialog_replace_all (EEditorSpellCheckDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *editor_selection;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gchar *replacement;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	editor_selection = e_editor_widget_get_selection (widget);

	selection = gtk_tree_view_get_selection (
			GTK_TREE_VIEW (dialog->priv->tree_view));
	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, 0, &replacement, -1);

	/* Repeatedly search for 'word', then replace selection by
	 * 'replacement'. Repeat until there's at least one occurence of
	 * 'word' in the document */
	while (webkit_web_view_search_text (
			WEBKIT_WEB_VIEW (widget), dialog->priv->word,
			FALSE, TRUE, TRUE)) {

		e_editor_selection_insert_html (
			editor_selection, replacement);

	}

	g_free (replacement);
	editor_spell_check_dialog_next (dialog);
}

static void
editor_spell_check_dialog_ignore (EEditorSpellCheckDialog *dialog)
{
	if (dialog->priv->word == NULL) {
		return;
	}

	enchant_dict_add_to_session (
		dialog->priv->current_dict, dialog->priv->word, -1);

	editor_spell_check_dialog_next (dialog);
}

static void
editor_spell_check_dialog_learn (EEditorSpellCheckDialog *dialog)
{
	if (dialog->priv->word == NULL) {
		return;
	}

	enchant_dict_add_to_personal (
		dialog->priv->current_dict, dialog->priv->word, -1);

	editor_spell_check_dialog_next (dialog);
}

static void
editor_spell_check_dialog_set_dictionary (EEditorSpellCheckDialog *dialog)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	EnchantDict *dictionary;

	gtk_combo_box_get_active_iter (
			GTK_COMBO_BOX (dialog->priv->dictionary_combo), &iter);
	model = gtk_combo_box_get_model (
			GTK_COMBO_BOX (dialog->priv->dictionary_combo));

	gtk_tree_model_get (model, &iter, 1, &dictionary, -1);

	dialog->priv->current_dict = dictionary;

	/* Update suggestions */
	editor_spell_check_dialog_set_word (dialog, dialog->priv->word);
}

static void
editor_spell_check_dialog_show (GtkWidget *gtk_widget)
{
	EEditorSpellCheckDialog *dialog;
	EEditor *editor;
	EEditorWidget *widget;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;

	dialog = E_EDITOR_SPELL_CHECK_DIALOG (gtk_widget);
	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);

	g_free (dialog->priv->word);
	dialog->priv->word = NULL;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	dialog->priv->selection = webkit_dom_dom_window_get_selection (window);

	/* Select the first word */
	editor_spell_check_dialog_next (dialog);

	GTK_WIDGET_CLASS (e_editor_spell_check_dialog_parent_class)->show (gtk_widget);
}

static void
editor_spell_check_dialog_finalize (GObject *object)
{
	EEditorSpellCheckDialog *dialog;

	dialog = E_EDITOR_SPELL_CHECK_DIALOG (object);

	g_free (dialog->priv->word);
	dialog->priv->word = NULL;
}

static void
e_editor_spell_check_dialog_class_init (EEditorSpellCheckDialogClass *klass)
{
	GtkWidgetClass *widget_class;
	GObjectClass *object_class;

	e_editor_spell_check_dialog_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorSpellCheckDialogPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = editor_spell_check_dialog_finalize;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = editor_spell_check_dialog_show;
}

static void
e_editor_spell_check_dialog_init (EEditorSpellCheckDialog *dialog)
{
	GtkWidget *widget;
 	GtkGrid *main_layout;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		dialog, E_TYPE_EDITOR_SPELL_CHECK_DIALOG, EEditorSpellCheckDialogPrivate);

	main_layout = e_editor_dialog_get_container (E_EDITOR_DIALOG (dialog));

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
	widget = gtk_button_new_with_mnemonic (_("Replace"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
			GTK_STOCK_CONVERT, GTK_ICON_SIZE_BUTTON));
	gtk_grid_attach (main_layout, widget, 1, 1, 1, 1);
	dialog->priv->replace_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_spell_check_dialog_replace), dialog);

	/* Replace All */
	widget = gtk_button_new_with_mnemonic (_("Replace All"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
			GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON));
	gtk_grid_attach (main_layout, widget, 1, 2, 1, 1);
	dialog->priv->replace_all_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_spell_check_dialog_replace_all), dialog);

	/* Ignore */
	widget = gtk_button_new_with_mnemonic (_("Ignore"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
			GTK_STOCK_CLEAR, GTK_ICON_SIZE_BUTTON));
	gtk_grid_attach (main_layout, widget, 1, 3, 1, 1);
	dialog->priv->ignore_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_spell_check_dialog_ignore), dialog);

	/* Skip */
	widget = gtk_button_new_with_mnemonic (_("Skip"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
			GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON));
	gtk_grid_attach (main_layout, widget, 1, 4, 1, 1);
	dialog->priv->skip_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_spell_check_dialog_next), dialog);

	/* Back */
	widget = gtk_button_new_with_mnemonic (_("Back"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
			GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON));
	gtk_grid_attach (main_layout, widget, 1, 5, 1, 1);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_spell_check_dialog_prev), dialog);

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
		G_CALLBACK (editor_spell_check_dialog_set_dictionary), dialog);

	/* Add Word button */
	widget = gtk_button_new_with_mnemonic (_("Add Word"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
			GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
	gtk_grid_attach (main_layout, widget, 1, 7, 1, 1);
	dialog->priv->add_word_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_spell_check_dialog_learn), dialog);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_editor_spell_check_dialog_new (EEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_EDITOR_SPELL_CHECK_DIALOG,
			"editor", editor,
			"title", N_("Spell Checking"),
			NULL));
}

GList *
e_editor_spell_check_dialog_get_dictionaries (EEditorSpellCheckDialog *dialog)
{
	g_return_val_if_fail (E_IS_EDITOR_SPELL_CHECK_DIALOG (dialog), NULL);

	return g_list_copy (dialog->priv->dictionaries);
}

void
e_editor_spell_check_dialog_set_dictionaries (EEditorSpellCheckDialog *dialog,
					      GList *dictionaries)
{
	GtkComboBox *combo_box;
	GtkListStore *store;
	GList *list;

	g_return_if_fail (E_IS_EDITOR_SPELL_CHECK_DIALOG (dialog));

	combo_box = GTK_COMBO_BOX (dialog->priv->dictionary_combo);

	/* Free the old list of spell checkers. */
	g_list_free (dialog->priv->dictionaries);

	/* Copy and sort the new list of spell checkers. */
	list = g_list_sort (
		g_list_copy (dictionaries),
		(GCompareFunc) e_editor_spell_checker_dict_compare);
	dialog->priv->dictionaries = list;

	/* Populate a list store for the combo box. */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

	while (list != NULL) {
		EnchantDict *dictionary = list->data;
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
		      	0, e_editor_spell_checker_get_dict_name (dictionary),
			1, dictionary, -1);

		list = g_list_next (list);
	}

	/* FIXME: Try to restore selection */
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));
	gtk_combo_box_set_active (combo_box, 0);

	g_object_unref (store);
}
