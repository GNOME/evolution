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

#include "e-spell-dialog.h"
#include "e-spell-dictionary.h"
#include "e-editor-widget.h"

#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (
	ESpellDialog,
	e_spell_dialog,
	GTK_TYPE_DIALOG
);

enum {
	COMBO_COLUMN_DICTIONARY,	/* E_TYPE_SPELL_DICTIONARY */
	COMBO_COLUMN_TEXT		/* G_TYPE_STRING */
};

enum {
	PROP_0,
	PROP_WORD
};

enum {
	ADDED,
	IGNORED,
	NEXT_WORD,
	PREV_WORD,
	REPLACE,
	REPLACE_ALL,
	LAST_SIGNAL
};

struct _ESpellDialogPrivate {

	/* widgets */
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
	gchar *word;

	EEditorWidget *editor;
};

static guint signals[LAST_SIGNAL];

static void
spell_dialog_render_dictionary (GtkComboBox *combo_box,
				GtkCellRenderer *renderer,
				GtkTreeModel *model,
				GtkTreeIter *iter)
{
	ESpellDictionary *dictionary;
	const gchar *name;

	gtk_tree_model_get (model, iter, 0, &dictionary, -1);
	name = e_spell_dictionary_get_name (dictionary);

	g_object_set (renderer, "text", name, NULL);
}

static void
spell_dialog_update_buttons (ESpellDialog *dialog)
{
	gboolean sensitive;

	/* Update "Add Word" and "Ignore" button sensitivity. */
	sensitive = (e_spell_dialog_get_word (dialog) != NULL);
	gtk_widget_set_sensitive (dialog->priv->add_word_button, sensitive);
	gtk_widget_set_sensitive (dialog->priv->ignore_button, sensitive);
}

static void
spell_dialog_update_suggestion_label (ESpellDialog *dialog)
{
	GtkLabel *label;
	const gchar *word;
	gchar *markup;
	gchar *text;

	label = GTK_LABEL (dialog->priv->suggestion_label);
	word = e_spell_dialog_get_word (dialog);

	/* Handle the simple case and get out. */
	if (word == NULL) {
		gtk_label_set_markup (label, NULL);
		return;
	}

	text = g_strdup_printf (_("Suggestions for \"%s\""), word);
	markup = g_strdup_printf ("<b>%s</b>", text);

	gtk_label_set_markup (label, markup);

	g_free (markup);
	g_free (text);
}

static void
spell_dialog_update_tree_view (ESpellDialog *dialog)
{
	WebKitSpellChecker *checker;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkListStore *store;
	GtkTreePath *path;
	const gchar *word;
	gchar** list;
	gint i;

	tree_view = GTK_TREE_VIEW (dialog->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);
	word = e_spell_dialog_get_word (dialog);

	store = gtk_list_store_new (1, G_TYPE_STRING);

	checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
	if (checker != NULL && word != NULL) {
		gchar *suggestions;
		suggestions = webkit_spell_checker_get_autocorrect_suggestions_for_misspelled_word (
			checker, word);

		list = g_strsplit (suggestions, ",", 0);
		g_free (suggestions);
	}

	for (i = 0; list && list[i]; i++) {
		const gchar *suggestion = list[i];
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, suggestion, -1);
	}
	g_strfreev (list);

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));

	/* Select the first item. */
	path = gtk_tree_path_new_first ();
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);
}

static void
spell_dialog_add_word_cb (ESpellDialog *dialog)
{
	WebKitSpellChecker *checker;
	const gchar *word;

	checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
	word = e_spell_dialog_get_word (dialog);

	webkit_spell_checker_learn_word (checker, word);
	g_signal_emit (dialog, signals[ADDED], 0);

	e_spell_dialog_next_word (dialog);
}

static void
spell_dialog_ignore_cb (ESpellDialog *dialog)
{
	WebKitSpellChecker *checker;
	const gchar *word;

	checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
	word = e_spell_dialog_get_word (dialog);

	webkit_spell_checker_ignore_word (checker, word);
	g_signal_emit (dialog, signals[IGNORED], 0);

	e_spell_dialog_next_word (dialog);
}

static void
spell_dialog_selection_changed_cb (ESpellDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	gboolean selected;

	tree_view = GTK_TREE_VIEW (dialog->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	/* Update "Replace" and "Replace All" button sensitivity. */
	selected = gtk_tree_selection_get_selected (selection, NULL, NULL);
	gtk_widget_set_sensitive (dialog->priv->replace_button, selected);
	gtk_widget_set_sensitive (dialog->priv->replace_all_button, selected);
}

static void
spell_dialog_replace_cb (ESpellDialog *dialog)
{
	gchar *word;

	word = e_spell_dialog_get_active_suggestion (dialog);
	g_return_if_fail (word != NULL);

	g_signal_emit (dialog, signals[REPLACE], 0, word);

	g_free (word);
}

static void
spell_dialog_replace_all_cb (ESpellDialog *dialog)
{
	gchar *word;

	word = e_spell_dialog_get_active_suggestion (dialog);
	g_return_if_fail (word != NULL);

	g_signal_emit (dialog, signals[REPLACE_ALL], 0, word);

	g_free (word);
}

static void
spell_dialog_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WORD:
			e_spell_dialog_set_word (
				E_SPELL_DIALOG (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_dialog_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WORD:
			g_value_set_string (
				value, e_spell_dialog_get_word (
				E_SPELL_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_dialog_dispose (GObject *object)
{
	ESpellDialogPrivate *priv;

	priv = E_SPELL_DIALOG (object)->priv;

	g_clear_object (&priv->add_word_button);
	g_clear_object (&priv->back_button);
	g_clear_object (&priv->dictionary_combo);
	g_clear_object (&priv->ignore_button);
	g_clear_object (&priv->replace_button);
	g_clear_object (&priv->replace_all_button);
	g_clear_object (&priv->skip_button);
	g_clear_object (&priv->tree_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_spell_dialog_parent_class)->dispose (object);
}

static void
spell_dialog_finalize (GObject *object)
{
	ESpellDialogPrivate *priv;

	priv = E_SPELL_DIALOG (object)->priv;

	g_free (priv->word);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_spell_dialog_parent_class)->finalize (object);
}

static void
e_spell_dialog_class_init (ESpellDialogClass *klass)
{
	GObjectClass *object_class;

	e_spell_dialog_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (ESpellDialogPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = spell_dialog_set_property;
	object_class->get_property = spell_dialog_get_property;
	object_class->dispose = spell_dialog_dispose;
	object_class->finalize = spell_dialog_finalize;

	g_object_class_install_property (
		object_class,
		PROP_WORD,
		g_param_spec_string (
			"word",
			"Misspelled Word",
			"The current misspelled word",
			NULL,
			G_PARAM_READWRITE));

	signals[ADDED] = g_signal_new (
		"added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[IGNORED] = g_signal_new (
		"ignored",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[NEXT_WORD] = g_signal_new (
		"next-word",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[PREV_WORD] = g_signal_new (
		"prev-word",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[REPLACE] = g_signal_new (
		"replace",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	signals[REPLACE_ALL] = g_signal_new (
		"replace-all",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

static void
e_spell_dialog_init (ESpellDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *container;
	GtkWidget *content_area;
	GtkWidget *table;
	GtkWidget *widget;
	gchar *markup;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		dialog, E_TYPE_SPELL_DIALOG, ESpellDialogPrivate);

	g_signal_connect (
		dialog, "notify::word", G_CALLBACK (
		spell_dialog_update_buttons), NULL);

	g_signal_connect (
		dialog, "notify::word", G_CALLBACK (
		spell_dialog_update_suggestion_label), NULL);

	g_signal_connect (
		dialog, "notify::word", G_CALLBACK (
		spell_dialog_update_tree_view), NULL);

	/* Build the widgets. */

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	gtk_dialog_add_button (
		GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Spell Checker"));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	gtk_box_set_spacing (GTK_BOX (content_area), 2);

	/* Table */
	widget = gtk_table_new (4, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 6);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 6);
	gtk_table_set_row_spacing (GTK_TABLE (widget), 1, 12);
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);
	table = widget;

	/* Suggestion Label */
	widget = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (
		GTK_TABLE (table), widget, 0, 2, 0, 1,
		GTK_EXPAND | GTK_FILL, 0, 0, 0);
	dialog->priv->suggestion_label = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Scrolled Window */
	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget),
		GTK_SHADOW_ETCHED_IN);
	gtk_table_attach (
		GTK_TABLE (table), widget, 0, 1, 1, 2,
		GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (widget);
	container = widget;

	/* Tree View */
	widget = gtk_tree_view_new ();
	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer, "text", 0);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (dialog->priv->suggestion_label), widget);
	g_signal_connect_swapped (
		widget, "row-activated",
		G_CALLBACK (spell_dialog_replace_cb), dialog);
	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (spell_dialog_selection_changed_cb), dialog);
	gtk_container_add (GTK_CONTAINER (container), widget);
	dialog->priv->tree_view = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Vertical Button Box */
	widget = gtk_vbutton_box_new ();
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_table_attach (
		GTK_TABLE (table), widget, 1, 2, 1, 2,
		0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (widget);
	container = widget;

	/* Replace Button */
	widget = gtk_button_new_with_mnemonic (_("_Replace"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
		GTK_STOCK_CONVERT, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (spell_dialog_replace_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	dialog->priv->replace_button = g_object_ref (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_show (widget);

	/* Replace All Button */
	widget = gtk_button_new_with_mnemonic (_("R_eplace All"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
		GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (spell_dialog_replace_all_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	dialog->priv->replace_all_button = g_object_ref (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_show (widget);

	/* Ignore Button */
	widget = gtk_button_new_with_mnemonic (_("_Ignore"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
		GTK_STOCK_CLEAR, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (spell_dialog_ignore_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	dialog->priv->ignore_button = g_object_ref (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_show (widget);

	/* Skip Button */
	widget = gtk_button_new_with_mnemonic (_("_Skip"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
		GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_spell_dialog_next_word), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	dialog->priv->skip_button = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Back Button */
	widget = gtk_button_new_with_mnemonic (_("_Back"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
		GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_spell_dialog_prev_word), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	dialog->priv->back_button = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Dictionary Label */
	markup = g_markup_printf_escaped ("<b>%s</b>", _("Dictionary"));
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (
		GTK_TABLE (table), widget, 0, 2, 2, 3,
		GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);
	g_free (markup);

	/* Dictionary Combo Box */
	widget = gtk_combo_box_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (widget), renderer,
		(GtkCellLayoutDataFunc) spell_dialog_render_dictionary,
		NULL, NULL);
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (spell_dialog_update_tree_view), dialog);
	gtk_table_attach (
		GTK_TABLE (table), widget, 0, 1, 3, 4,
		GTK_EXPAND | GTK_FILL, 0, 0, 0);
	dialog->priv->dictionary_combo = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Add Word Button */
	widget = gtk_button_new_with_mnemonic (_("_Add Word"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (spell_dialog_add_word_cb), dialog);
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_stock (
		GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
	gtk_table_attach (
		GTK_TABLE (table), widget, 1, 2, 3, 4,
		GTK_FILL, 0, 0, 0);
	dialog->priv->add_word_button = g_object_ref (widget);
	gtk_widget_show (widget);
}

GtkWidget *
e_spell_dialog_new (GtkWindow *parent)
{
	return g_object_new (
		E_TYPE_SPELL_DIALOG,
		"transient-for", parent, NULL);
}

void
e_spell_dialog_close (ESpellDialog *dialog)
{
	g_return_if_fail (E_IS_SPELL_DIALOG (dialog));

	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
}

const gchar *
e_spell_dialog_get_word (ESpellDialog *dialog)
{
	g_return_val_if_fail (E_IS_SPELL_DIALOG (dialog), NULL);

	return dialog->priv->word;
}

void
e_spell_dialog_set_word (ESpellDialog *dialog,
			 const gchar *word)
{
	g_return_if_fail (E_IS_SPELL_DIALOG (dialog));

	/* Do not emit signals if the word is unchanged. */
	if (word != NULL && dialog->priv->word != NULL)
		if (g_str_equal (word, dialog->priv->word))
			return;

	g_free (dialog->priv->word);
	dialog->priv->word = g_strdup (word);

	g_object_notify (G_OBJECT (dialog), "word");
}

void
e_spell_dialog_next_word (ESpellDialog *dialog)
{
	g_signal_emit (dialog, signals[NEXT_WORD], 0);
}

void
e_spell_dialog_prev_word (ESpellDialog *dialog)
{
	g_signal_emit (dialog, signals[PREV_WORD], 0);
}

GList *
e_spell_dialog_get_dictionaries (ESpellDialog *dialog)
{
	g_return_val_if_fail (E_IS_SPELL_DIALOG (dialog), NULL);

	return g_list_copy (dialog->priv->dictionaries);
}

void
e_spell_dialog_set_dictionaries (ESpellDialog *dialog,
				   GList *dictionaries)
{
	GtkComboBox *combo_box;
	GtkListStore *store;
	GList *list;

	g_return_if_fail (E_IS_SPELL_DIALOG (dialog));

	combo_box = GTK_COMBO_BOX (dialog->priv->dictionary_combo);

	/* Free the old list of spell checkers. */
	list = dialog->priv->dictionaries;
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Copy and sort the new list of spell checkers. */
	list = g_list_sort (
		g_list_copy (dictionaries),
		(GCompareFunc) e_spell_dictionary_compare);
	g_list_foreach (list, (GFunc) g_object_ref, NULL);
	dialog->priv->dictionaries = list;

	/* Populate a list store for the combo box. */

	store = gtk_list_store_new (1, E_TYPE_SPELL_DICTIONARY);

	while (list != NULL) {
		ESpellDictionary *dictionary = list->data;
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, dictionary, -1);

		list = g_list_next (list);
	}

	/* FIXME Try to preserve the previously selected language. */
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));
	gtk_combo_box_set_active (combo_box, 0);

	g_object_unref (store);

	/* XXX notify property? */
}

ESpellDictionary *
e_spell_dialog_get_active_dictionary (ESpellDialog *dialog)
{
	ESpellDictionary *dict;
	GtkComboBox *combo_box;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_SPELL_DIALOG (dialog), NULL);

	combo_box = GTK_COMBO_BOX (dialog->priv->dictionary_combo);
	model = gtk_combo_box_get_model (combo_box);

	if (!gtk_combo_box_get_active_iter (combo_box, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COMBO_COLUMN_DICTIONARY, &dict, -1);

	return dict;
}

gchar *
e_spell_dialog_get_active_suggestion (ESpellDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *word;

	g_return_val_if_fail (E_IS_SPELL_DIALOG (dialog), NULL);

	tree_view = GTK_TREE_VIEW (dialog->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	/* If nothing is selected, return NULL. */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, 0, &word, -1);
	g_return_val_if_fail (word != NULL, NULL);

	return word;
}
