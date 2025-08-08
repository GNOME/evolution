/*
 * e-html-editor.c
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

#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <enchant.h>
#include <libedataserver/libedataserver.h>

#include "e-html-editor.h"

#include "e-activity-bar.h"
#include "e-alert-bar.h"
#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-html-editor-private.h"
#include "e-content-editor.h"
#include "e-markdown-editor.h"
#include "e-misc-utils.h"
#include "e-simple-async-result.h"
#include "e-util-enumtypes.h"
#include "e-ui-customize-dialog.h"

#define MARKDOWN_EDITOR_NAME "markdown"

/**
 * EHTMLEditor:
 *
 * #EHTMLEditor provides GUI for manipulating with properties of
 * #EContentEditor i.e. toolbars and actions.
 */

/* This controls how spelling suggestions are divided between the primary
 * context menu and a secondary menu.  The idea is to prevent the primary
 * menu from growing too long.
 *
 * The constants below are used as follows:
 *
 * if TOTAL_SUGGESTIONS <= MAX_LEVEL1_SUGGETIONS:
 *     LEVEL1_SUGGESTIONS = TOTAL_SUGGESTIONS
 * elif TOTAL_SUGGESTIONS - MAX_LEVEL1_SUGGESTIONS < MIN_LEVEL2_SUGGESTIONS:
 *     LEVEL1_SUGGESTIONS = TOTAL_SUGGESTIONS
 * else
 *     LEVEL1_SUGGESTIONS = MAX_LEVEL1_SUGGETIONS
 *
 * LEVEL2_SUGGESTIONS = TOTAL_SUGGESTIONS - LEVEL1_SUGGESTIONS
 *
 * Note that MAX_LEVEL1_SUGGETIONS is not a hard maximum.
 */
#define MAX_LEVEL1_SUGGESTIONS	4
#define MIN_LEVEL2_SUGGESTIONS	3

enum {
	PROP_0,
	PROP_MODE,
	PROP_FILENAME,
	PROP_PASTE_PLAIN_PREFER_PRE
};

enum {
	UPDATE_ACTIONS,
	SPELL_LANGUAGES_CHANGED,
	AFTER_MODE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Forward Declarations */
static void	e_html_editor_alert_sink_init
					(EAlertSinkInterface *interface);

G_DEFINE_TYPE_WITH_CODE (EHTMLEditor, e_html_editor, GTK_TYPE_GRID,
	G_ADD_PRIVATE (EHTMLEditor)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_html_editor_alert_sink_init))

/* See: https://www.w3schools.com/cssref/css_websafe_fonts.asp */
static struct _SupportedFonts {
	const gchar *display_name;
	const gchar *css_value;
} supported_fonts[] = {
	{ "Arial", "Arial, Helvetica, sans-serif" },
	{ "Arial Black", "\"Arial Black\", Gadget, sans-serif" },
	{ "Comic Sans MS", "\"Comic Sans MS\", cursive, sans-serif" },
	{ "Courier New", "\"Courier New\", Courier, monospace" },
	{ "Georgia", "Georgia, serif" },
	{ "Impact", "Impact, Charcoal, sans-serif" },
	{ "Lucida Console", "\"Lucida Console\", Monaco, monospace" },
	{ "Lucida Sans", "\"Lucida Sans Unicode\", \"Lucida Grande\", sans-serif" },
	{ "Monospace", "monospace" },
	{ "Palatino", "\"Palatino Linotype\", \"Book Antiqua\", Palatino, serif" },
	{ "Tahoma", "Tahoma, Geneva, sans-serif" },
	{ "Times New Roman", "\"Times New Roman\", Times, serif" },
	{ "Trebuchet MS", "\"Trebuchet MS\", Helvetica, sans-serif" },
	{ "Verdana", "Verdana, Geneva, sans-serif" }
};

GtkWidget *
e_html_editor_util_create_font_name_combo (void)
{
	GtkComboBoxText *combo_box;
	gint ii;

	combo_box = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());

	gtk_combo_box_text_append (combo_box, "", _("Default"));

	for (ii = 0; ii < G_N_ELEMENTS (supported_fonts); ii++) {
		gtk_combo_box_text_append (combo_box, supported_fonts[ii].css_value, supported_fonts[ii].display_name);
	}

	return GTK_WIDGET (combo_box);
}

gchar *
e_html_editor_util_dup_font_id (GtkComboBox *combo_box,
				const gchar *font_name)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *free_str = NULL;
	gchar *id = NULL, **variants;
	gint id_column, ii;

	g_return_val_if_fail (GTK_IS_COMBO_BOX_TEXT (combo_box), NULL);

	if (!font_name || !*font_name)
		return NULL;

	for (ii = 0; ii < G_N_ELEMENTS (supported_fonts); ii++) {
		if (camel_strcase_equal (supported_fonts[ii].css_value, font_name))
			return g_strdup (font_name);
	}

	id_column = gtk_combo_box_get_id_column (combo_box);
	model = gtk_combo_box_get_model (combo_box);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *stored_id = NULL;

			gtk_tree_model_get (model, &iter, id_column, &stored_id, -1);

			if (stored_id && *stored_id) {
				if (camel_strcase_equal (stored_id, font_name)) {
					id = stored_id;
					break;
				}

				free_str = g_slist_prepend (free_str, stored_id);
			} else {
				g_free (stored_id);
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	if (!id) {
		GHashTable *known_fonts;
		GSList *free_strv = NULL, *link;

		known_fonts = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);

		for (link = free_str; link; link = g_slist_next (link)) {
			gchar *stored_id = link->data;

			variants = g_strsplit (stored_id, ",", -1);
			for (ii = 0; variants[ii]; ii++) {
				if (variants[ii][0] &&
				    !g_hash_table_contains (known_fonts, variants[ii])) {
					g_hash_table_insert (known_fonts, variants[ii], stored_id);
				}
			}

			free_strv = g_slist_prepend (free_strv, variants);
		}

		variants = g_strsplit (font_name, ",", -1);
		for (ii = 0; variants[ii]; ii++) {
			if (variants[ii][0]) {
				const gchar *stored_id;

				stored_id = g_hash_table_lookup (known_fonts, variants[ii]);
				if (stored_id) {
					id = g_strdup (stored_id);
					break;
				}
			}
		}

		if (!id) {
			gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_box), font_name, variants[0]);
			id = g_strdup (font_name);
		}

		g_hash_table_destroy (known_fonts);
		g_slist_free_full (free_strv, (GDestroyNotify) g_strfreev);
		g_strfreev (variants);
	}

	g_slist_free_full (free_str, g_free);

	return id;
}

EHTMLEditorActionMenuPair *
e_html_editor_action_menu_pair_new (EUIAction *action, /* (transfer full) */
				    GMenu *menu) /* (transfer full) */
{
	EHTMLEditorActionMenuPair *data;

	data = g_new0 (EHTMLEditorActionMenuPair, 1);
	data->parent_menu_action = action;
	data->submenu_items = menu;

	return data;
}

void
e_html_editor_action_menu_pair_free (gpointer ptr)
{
	EHTMLEditorActionMenuPair *data = ptr;

	if (data) {
		g_clear_object (&data->parent_menu_action);
		g_clear_object (&data->submenu_items);
		g_free (data);
	}
}

void
e_html_editor_emit_after_mode_changed (EHTMLEditor *self)
{
	g_return_if_fail (E_IS_HTML_EDITOR (self));

	g_signal_emit (self, signals[AFTER_MODE_CHANGED], 0, NULL);
}

/* Action callback for context menu spelling suggestions.
 * XXX This should really be in e-html-editor-actions.c */
static void
action_context_spell_suggest_cb (EUIAction *action,
				 GVariant *parameter,
                                 gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));
	g_return_if_fail (state != NULL);

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_replace_caret_word (cnt_editor, g_variant_get_string (state, NULL));

	g_clear_pointer (&state, g_variant_unref);
}

static void
html_editor_inline_spelling_suggestions (EHTMLEditor *editor,
					 const gchar *caret_word)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	EUIActionGroup *action_group;
	GPtrArray *fill_actions;
	gchar **suggestions;
	const gchar *map_name;
	guint count = 0;
	guint length;
	guint threshold;
	gint ii;
	gboolean had_any_before;

	had_any_before = editor->priv->spell_suggest_actions->len > 0 || editor->priv->spell_suggest_more_actions->len > 0;

	g_ptr_array_set_size (editor->priv->spell_suggest_actions, 0);
	g_ptr_array_set_size (editor->priv->spell_suggest_more_actions, 0);

	if (!caret_word || !*caret_word) {
		if (had_any_before)
			e_ui_menu_rebuild (editor->priv->main_menu);
		return;
	}

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	suggestions = e_spell_checker_get_guesses_for_word (spell_checker, caret_word);
	action_group = editor->priv->suggestion_actions;
	map_name = e_ui_action_group_get_name (action_group);

	fill_actions = editor->priv->spell_suggest_actions;

	length = (suggestions != NULL) ? g_strv_length (suggestions) : 0;

	/* Calculate how many suggestions to put directly in the
	 * context menu.  The rest will go in a secondary menu. */
	if (length <= MAX_LEVEL1_SUGGESTIONS) {
		threshold = length;
	} else if (length - MAX_LEVEL1_SUGGESTIONS < MIN_LEVEL2_SUGGESTIONS) {
		threshold = length;
	} else {
		threshold = MAX_LEVEL1_SUGGESTIONS;
	}

	for (ii = 0; suggestions && suggestions[ii]; ii++) {
		gchar *suggestion = suggestions[ii];
		gchar action_name[128];
		EUIAction *action;

		/* Once we reach the threshold, put all subsequent
		 * spelling suggestions in a secondary menu. */
		if (count == threshold)
			fill_actions = editor->priv->spell_suggest_more_actions;

		/* Action name just needs to be unique. */
		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "suggest-%d", count++) < sizeof (action_name));

		action = e_ui_action_new_stateful (map_name, action_name, NULL, g_variant_new_string (suggestion));
		e_ui_action_set_label (action, suggestion);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_context_spell_suggest_cb), editor);

		e_ui_action_group_add (action_group, action);
		/* the array assumes ownership of the action */
		g_ptr_array_add (fill_actions, action);
	}

	g_strfreev (suggestions);
	g_clear_object (&spell_checker);

	if (had_any_before || count > 0)
		e_ui_menu_rebuild (editor->priv->main_menu);
}

/* Helper for html_editor_update_actions() */
static void
html_editor_spell_checkers_foreach (EHTMLEditor *editor,
				    const gchar *language_code,
				    const gchar *caret_word)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	ESpellDictionary *dictionary = NULL;
	EUIActionGroup *action_group;
	EHTMLEditorActionMenuPair *action_menu_pair;
	GList *list, *link;
	const gchar *map_name;
	gint ii = 0;

	action_menu_pair = g_hash_table_lookup (editor->priv->spell_suggest_menus_by_code, language_code);
	g_return_if_fail (action_menu_pair != NULL);

	g_menu_remove_all (action_menu_pair->submenu_items);

	if (!caret_word || !*caret_word) {
		e_ui_action_set_visible (action_menu_pair->parent_menu_action, FALSE);
		return;
	}

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);

	dictionary = e_spell_checker_ref_dictionary (spell_checker, language_code);
	if (dictionary != NULL) {
		list = e_spell_dictionary_get_suggestions (dictionary, caret_word, -1);
		g_object_unref (dictionary);
	} else {
		list = NULL;
	}

	action_group = editor->priv->suggestion_actions;
	map_name = e_ui_action_group_get_name (action_group);
	e_ui_action_set_visible (action_menu_pair->parent_menu_action, list != NULL);

	for (link = list; link != NULL; link = g_list_next (link), ii++) {
		gchar *suggestion = link->data;
		gchar action_name[128];
		EUIAction *action;
		GMenuItem *menu_item;

		/* Action name just needs to be unique. */
		g_snprintf (action_name, sizeof (action_name), "suggest-%s-%d", language_code, ii);

		action = e_ui_action_new_stateful (map_name, action_name, NULL, g_variant_new_string (suggestion));
		e_ui_action_set_label (action, suggestion);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_context_spell_suggest_cb), editor);

		e_ui_action_group_add (action_group, action);

		menu_item = g_menu_item_new (NULL, NULL);
		e_ui_manager_update_item_from_action (editor->priv->ui_manager, menu_item, action);
		/* to not look like a radio item */
		g_menu_item_set_attribute (menu_item, G_MENU_ATTRIBUTE_TARGET, NULL);

		g_menu_append_item (action_menu_pair->submenu_items, menu_item);

		g_clear_object (&menu_item);

		if (link == list)
			g_ptr_array_add (editor->priv->spell_suggest_actions, g_object_ref (action_menu_pair->parent_menu_action));
	}

	g_list_free_full (list, (GDestroyNotify) g_free);
	g_clear_object (&spell_checker);
}

void
e_html_editor_update_spell_actions (EHTMLEditor *editor)
{
	ESpellChecker *spell_checker;
	EContentEditor *cnt_editor;
	guint count;

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);

	count = e_spell_checker_count_active_languages (spell_checker);

	e_ui_action_set_visible (ACTION (CONTEXT_SPELL_ADD), count == 1);
	e_ui_action_set_visible (ACTION (CONTEXT_SPELL_ADD_MENU), count > 1);
	e_ui_action_set_visible (ACTION (CONTEXT_SPELL_IGNORE), count > 0);

	e_ui_action_set_sensitive (ACTION (SPELL_CHECK), count > 0);
	e_ui_action_set_sensitive (ACTION (LANGUAGE_MENU), e_spell_checker_count_available_dicts (spell_checker) > 0);

	g_clear_object (&spell_checker);
}

static void
action_set_visible_and_sensitive (EUIAction *action,
                                  gboolean value)
{
	e_ui_action_set_visible (action, value);
	e_ui_action_set_sensitive (action, value);
}

static void
html_editor_update_actions (EHTMLEditor *editor,
			    EContentEditorNodeFlags flags,
			    const gchar *caret_word,
			    const gchar *hover_uri)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	GHashTableIter iter;
	gpointer value = NULL;
	gchar **languages = NULL;
	guint ii, n_languages;
	gboolean visible;

	cnt_editor = e_html_editor_get_content_editor (editor);

	if (camel_debug ("webkit:editor"))
		printf ("%s: flags:%d(%x) caret-word:'%s' hover_uri:'%s'\n", G_STRFUNC, flags, flags, caret_word, hover_uri);

	g_clear_pointer (&editor->priv->context_hover_uri, g_free);
	editor->priv->context_hover_uri = hover_uri && *hover_uri ? g_strdup (hover_uri) : NULL;

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_IMAGE);
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_IMAGE), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_DELETE_IMAGE), visible);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_ANCHOR);
	action_set_visible_and_sensitive (ACTION (CONTEXT_INSERT_LINK), !visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_LINK), visible);

	visible = hover_uri && *hover_uri;
	action_set_visible_and_sensitive (ACTION (CONTEXT_COPY_LINK), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_OPEN_LINK), visible);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_H_RULE);
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_RULE), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_DELETE_HRULE), visible);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_TEXT);
	/* Only display the text properties dialog when some text is selected. */
	action_set_visible_and_sensitive (
		ACTION (CONTEXT_PROPERTIES_TEXT),
		visible && !(flags & E_CONTENT_EDITOR_NODE_IS_TEXT_COLLAPSED));

	visible =
		e_ui_action_get_visible (ACTION (CONTEXT_PROPERTIES_IMAGE)) ||
		e_ui_action_get_visible (ACTION (CONTEXT_PROPERTIES_LINK)) ||
		visible; /* text node under caret */
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_PARAGRAPH), visible);

	/* Set to visible if any of these are true:
	 *   - Selection is active and contains a link.
	 *   - Cursor is on a link.
	 *   - Cursor is on an image that has a URL or target.
	 */
	visible = (flags & E_CONTENT_EDITOR_NODE_IS_ANCHOR);
	action_set_visible_and_sensitive (ACTION (CONTEXT_REMOVE_LINK), visible);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_TABLE_CELL);
	action_set_visible_and_sensitive (ACTION (CONTEXT_DELETE_CELL), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_DELETE_COLUMN), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_DELETE_ROW), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_DELETE_TABLE), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_INSERT_COLUMN_AFTER), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_INSERT_COLUMN_BEFORE), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_INSERT_ROW_ABOVE), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_INSERT_ROW_BELOW), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_CELL), visible);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_TABLE);
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_TABLE), visible);

	/********************** Spell Check Suggestions **********************/

	/* Remove the old content from the context menu. */
	g_hash_table_iter_init (&iter, editor->priv->spell_suggest_menus_by_code);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		EHTMLEditorActionMenuPair *action_menu_pair = value;
		e_ui_action_set_visible (action_menu_pair->parent_menu_action, FALSE);
		g_menu_remove_all (action_menu_pair->submenu_items);
	}

	/* Clear the action group for spelling suggestions. */
	e_ui_action_group_remove_all (editor->priv->suggestion_actions);

	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	languages = e_spell_checker_list_active_languages (
		spell_checker, &n_languages);

	/* Decide if we should show spell checking items. */
	visible = FALSE;
	if (n_languages > 0) {
		if (caret_word && *caret_word) {
			visible = !e_spell_checker_check_word (spell_checker, caret_word, -1);
		} else {
			visible = FALSE;
		}
	}

	e_ui_action_group_set_visible (editor->priv->spell_check_actions, visible);

	g_clear_object (&spell_checker);

	/* Exit early if spell checking items are invisible. */
	if (!visible) {
		g_strfreev (languages);
		return;
	}

	/* Handle a single active language as a special case. */
	if (n_languages == 1) {
		html_editor_inline_spelling_suggestions (editor, caret_word);
		g_strfreev (languages);

		e_html_editor_update_spell_actions (editor);
		return;
	}

	g_ptr_array_set_size (editor->priv->spell_suggest_actions, 0);
	g_ptr_array_set_size (editor->priv->spell_suggest_more_actions, 0);

	/* Add actions and context menu content for active languages. */
	for (ii = 0; ii < n_languages; ii++)
		html_editor_spell_checkers_foreach (editor, languages[ii], caret_word);

	g_strfreev (languages);

	e_html_editor_update_spell_actions (editor);
}

static void
html_editor_update_spell_languages (EHTMLEditor *editor,
				    gboolean autoenable_spelling)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	gchar **languages;

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);

	languages = e_spell_checker_list_active_languages (spell_checker, NULL);

	if (autoenable_spelling)
		e_content_editor_set_spell_check_enabled (cnt_editor, languages && *languages);

	/* Set the languages for webview to highlight misspelled words */
	e_content_editor_set_spell_checking_languages (cnt_editor, (const gchar **) languages);

	if (editor->priv->spell_check_dialog != NULL)
		e_html_editor_spell_check_dialog_update_dictionaries (
			E_HTML_EDITOR_SPELL_CHECK_DIALOG (
			editor->priv->spell_check_dialog));

	e_html_editor_actions_update_spellcheck_languages_menu (editor, (const gchar * const *) languages);
	g_clear_object (&spell_checker);
	g_strfreev (languages);
}

static void
html_editor_spell_languages_changed (EHTMLEditor *editor)
{
	html_editor_update_spell_languages (editor, TRUE);
}

typedef struct _ContextMenuData {
	GWeakRef *editor_weakref; /* EHTMLEditor * */
	EContentEditorNodeFlags flags;
	gchar *caret_word;
	gchar *hover_uri;
	GdkEvent *event;
} ContextMenuData;

static void
context_menu_data_free (gpointer ptr)
{
	ContextMenuData *cmd = ptr;

	if (cmd) {
		g_clear_pointer (&cmd->event, gdk_event_free);
		e_weak_ref_free (cmd->editor_weakref);
		g_free (cmd->caret_word);
		g_free (cmd->hover_uri);
		g_slice_free (ContextMenuData, cmd);
	}
}

static gboolean
html_editor_show_context_menu_idle_cb (gpointer user_data)
{
	ContextMenuData *cmd = user_data;
	EHTMLEditor *editor;

	g_return_val_if_fail (cmd != NULL, FALSE);

	editor = g_weak_ref_get (cmd->editor_weakref);
	if (editor) {
		GtkMenu *menu;
		GObject *ui_item;

		e_ui_menu_freeze (editor->priv->main_menu);
		g_signal_emit (editor, signals[UPDATE_ACTIONS], 0, cmd->flags, cmd->caret_word, cmd->hover_uri);
		e_ui_menu_thaw (editor->priv->main_menu);

		ui_item = e_ui_manager_create_item (editor->priv->ui_manager, "context-menu");
		menu = GTK_MENU (gtk_menu_new_from_model (G_MENU_MODEL (ui_item)));
		g_clear_object (&ui_item);

		gtk_menu_attach_to_widget (menu, GTK_WIDGET (editor), NULL);
		e_util_connect_menu_detach_after_deactivate (menu);

		gtk_menu_popup_at_pointer (menu, cmd->event);

		g_object_unref (editor);
	}

	return FALSE;
}

static void
html_editor_context_menu_requested_cb (EContentEditor *cnt_editor,
				       EContentEditorNodeFlags flags,
				       const gchar *caret_word,
				       GdkEvent *event,
				       EHTMLEditor *editor)
{
	ContextMenuData *cmd;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	cmd = g_slice_new0 (ContextMenuData);
	cmd->editor_weakref = e_weak_ref_new (editor);
	cmd->flags = flags;
	cmd->caret_word = g_strdup (caret_word);
	cmd->hover_uri = g_strdup (e_content_editor_get_hover_uri (cnt_editor));
	cmd->event = gdk_event_copy (event);

	g_idle_add_full (G_PRIORITY_LOW, html_editor_show_context_menu_idle_cb,
		cmd, context_menu_data_free);
}

static gchar *
html_editor_find_ui_file (const gchar *basename)
{
	gchar *filename;

	g_return_val_if_fail (basename != NULL, NULL);

	/* Support running directly from the source tree. */
	filename = g_build_filename (".", "data", "ui", basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	filename = g_build_filename ("..", "..", "..", "data", "ui", basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	filename = g_build_filename (EVOLUTION_UIDIR, basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	g_critical ("Could not locate '%s'", basename);

	return NULL;
}

static void
html_editor_parent_changed (GtkWidget *widget,
                            GtkWidget *previous_parent)
{
	GtkWidget *top_level;
	EHTMLEditor *editor = E_HTML_EDITOR (widget);

	/* If we now have a window, then install our accelerators to it */
	top_level = gtk_widget_get_toplevel (widget);
	if (GTK_IS_WINDOW (top_level)) {
		gtk_window_add_accel_group (
			GTK_WINDOW (top_level),
			e_ui_manager_get_accel_group (editor->priv->ui_manager));
		e_ui_manager_set_action_groups_widget (editor->priv->ui_manager, top_level);
	}
}

static void
html_editor_realize (GtkWidget *widget)
{
	html_editor_update_spell_languages (E_HTML_EDITOR (widget), FALSE);
}

static void
html_editor_set_paste_plain_prefer_pre (EHTMLEditor *editor,
					gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	if ((editor->priv->paste_plain_prefer_pre ? 1 : 0) != (value ? 1 : 0)) {
		editor->priv->paste_plain_prefer_pre = value;

		g_object_notify (G_OBJECT (editor), "paste-plain-prefer-pre");
	}
}

static gboolean
html_editor_get_paste_plain_prefer_pre (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), FALSE);

	return editor->priv->paste_plain_prefer_pre;
}

static gboolean
e_html_editor_mode_to_bool_hide_in_markdown_cb (GBinding *binding,
						const GValue *from_value,
						GValue *to_value,
						gpointer user_data)
{
	EContentEditorMode mode;

	mode = g_value_get_enum (from_value);

	g_value_set_boolean (to_value,
		mode != E_CONTENT_EDITOR_MODE_MARKDOWN &&
		mode != E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT &&
		mode != E_CONTENT_EDITOR_MODE_MARKDOWN_HTML);

	return TRUE;
}

static gboolean
e_html_editor_edit_html_toolbar_visible_cb (GBinding *binding,
					    const GValue *from_value,
					    GValue *to_value,
					    gpointer user_data)
{
	gboolean visible;
	EHTMLEditor *editor;

	editor = user_data;
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), TRUE);

	visible = g_value_get_boolean (from_value);

	g_value_set_boolean (to_value, visible && editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML);

	return TRUE;
}

static gboolean
e_html_editor_content_editor_font_name_to_combo_box (GBinding *binding,
						     const GValue *from_value,
						     GValue *to_value,
						     gpointer user_data)
{
	gchar *id = NULL;
	GObject *target = g_binding_dup_target (binding);

	id = e_html_editor_util_dup_font_id (GTK_COMBO_BOX (target), g_value_get_string (from_value));
	g_value_take_string (to_value, id ? id : g_strdup (""));
	g_clear_object (&target);

	return TRUE;
}

static void
e_html_editor_unbind_and_unref (gpointer ptr)
{
	GBinding *binding = ptr;

	if (binding) {
		g_binding_unbind (binding);
		g_object_unref (binding);
	}
}

static void
e_html_editor_bind_font_name_after_mode_changed_cb (EHTMLEditor *self,
						    gpointer user_data)
{
	GtkWidget *widget = user_data;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_HTML_EDITOR (self));
	g_return_if_fail (GTK_IS_COMBO_BOX (widget));

	cnt_editor = e_html_editor_get_content_editor (self);
	if (cnt_editor) {
		GBinding *binding;

		binding = e_binding_bind_property_full (
			cnt_editor, "font-name",
			widget, "active-id",
			G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
			e_html_editor_content_editor_font_name_to_combo_box,
			NULL,
			NULL, NULL);
		g_object_set_data_full (G_OBJECT (widget), "EHTMLEditor::binding", g_object_ref (binding),
			e_html_editor_unbind_and_unref);

		binding = e_binding_bind_property (
			cnt_editor, "editable",
			widget, "sensitive",
			G_BINDING_SYNC_CREATE);
		g_object_set_data_full (G_OBJECT (widget), "EHTMLEditor::binding-sensitive", g_object_ref (binding),
			e_html_editor_unbind_and_unref);

		gtk_widget_set_sensitive (widget, e_content_editor_is_editable (cnt_editor) && e_html_editor_get_mode (self) == E_CONTENT_EDITOR_MODE_HTML);
	} else {
		g_object_set_data (G_OBJECT (widget), "EHTMLEditor::binding", NULL);
		g_object_set_data (G_OBJECT (widget), "EHTMLEditor::binding-sensitive", NULL);
	}
}

static gboolean
e_html_editor_current_color_to_cnt_editor_cb (GBinding *binding,
					      const GValue *from_value,
					      GValue *to_value,
					      gpointer user_data)
{
	GdkRGBA *from_rgba = g_value_get_boxed (from_value);

	if (from_rgba) {
		EColorCombo *color_combo = E_COLOR_COMBO (g_binding_dup_source (binding));
		GdkRGBA def_rgba = { 0, };

		e_color_combo_get_default_color (color_combo, &def_rgba);

		/* the default color means no special color to set */
		if (gdk_rgba_equal (&def_rgba, from_rgba))
			from_rgba = NULL;

		g_clear_object (&color_combo);
	}

	g_value_set_boxed (to_value, from_rgba);

	return TRUE;
}

static void
e_html_editor_bind_color_combox_box (EHTMLEditor *self,
				     GtkWidget *widget,
				     const gchar *property_name)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (self);
	if (cnt_editor) {
		GBinding *binding;

		binding = e_binding_bind_property_full (
			widget, "current-color",
			cnt_editor, property_name,
			G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
			e_html_editor_current_color_to_cnt_editor_cb,
			NULL,
			NULL, NULL);
		g_object_set_data_full (G_OBJECT (widget), "EHTMLEditor::binding", g_object_ref (binding),
			e_html_editor_unbind_and_unref);

		binding = e_binding_bind_property (
			cnt_editor, "editable",
			widget, "sensitive",
			G_BINDING_SYNC_CREATE);
		g_object_set_data_full (G_OBJECT (widget), "EHTMLEditor::binding-sensitive", g_object_ref (binding),
			e_html_editor_unbind_and_unref);

		gtk_widget_set_sensitive (widget, e_content_editor_is_editable (cnt_editor) && e_html_editor_get_mode (self) == E_CONTENT_EDITOR_MODE_HTML);
	} else {
		g_object_set_data (G_OBJECT (widget), "EHTMLEditor::binding", NULL);
		g_object_set_data (G_OBJECT (widget), "EHTMLEditor::binding-sensitive", NULL);
		gtk_widget_set_sensitive (widget, FALSE);
	}
}

static void
e_html_editor_bind_font_color_after_mode_changed_cb (EHTMLEditor *self,
						     gpointer user_data)
{
	GtkWidget *widget = user_data;

	g_return_if_fail (E_IS_HTML_EDITOR (self));
	g_return_if_fail (E_IS_COLOR_COMBO (widget));

	e_html_editor_bind_color_combox_box (self, widget, "font-color");
}

static void
e_html_editor_bind_background_color_after_mode_changed_cb (EHTMLEditor *self,
							   gpointer user_data)
{
	GtkWidget *widget = user_data;

	g_return_if_fail (E_IS_HTML_EDITOR (self));
	g_return_if_fail (E_IS_COLOR_COMBO (widget));

	e_html_editor_bind_color_combox_box (self, widget, "background-color");
}

static GObject *
e_html_editor_create_submenu_from_actions (EHTMLEditor *self,
					   const gchar *label,
					   GPtrArray *actions) /* EUIAction * */
{
	GMenuItem *submenu;
	GMenu *menu;
	guint ii, n_added = 0;

	menu = g_menu_new ();

	for (ii = 0; ii < actions->len; ii++) {
		EUIAction *subaction = g_ptr_array_index (actions, ii);
		GMenuItem *item;

		if (!e_ui_action_is_visible (subaction))
			continue;

		item = g_menu_item_new (NULL, NULL);
		e_ui_manager_update_item_from_action (self->priv->ui_manager, item, subaction);
		/* to not look like a radio item */
		g_menu_item_set_attribute (item, G_MENU_ATTRIBUTE_TARGET, NULL);
		g_menu_append_item (menu, item);
		g_clear_object (&item);

		n_added++;
	}

	if (n_added) {
		if (label)
			submenu = g_menu_item_new_submenu (label, G_MENU_MODEL (menu));
		else
			submenu = g_menu_item_new_section (NULL, G_MENU_MODEL (menu));
	} else {
		submenu = NULL;
	}

	g_clear_object (&menu);

	return submenu ? G_OBJECT (submenu) : NULL;
}

static GObject *
e_html_editor_create_spell_add_menu (EHTMLEditor *self,
				     EUIAction *action)
{
	return e_html_editor_create_submenu_from_actions (self, e_ui_action_get_label (action), self->priv->spell_add_actions);
}

static gboolean
e_html_editor_spell_is_single_lang (EHTMLEditor *self)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	gboolean single_lang;

	cnt_editor = e_html_editor_get_content_editor (self);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	single_lang = spell_checker && e_spell_checker_count_active_languages (spell_checker) == 1;
	g_clear_object (&spell_checker);

	return single_lang;
}

static GObject *
e_html_editor_create_spell_suggest (EHTMLEditor *self,
				    EUIAction *action)
{
	GMenu *menu;
	GMenuItem *item;
	GHashTableIter iter;
	GHashTable *submenu_by_action; /* EUIAction ~> GMenu */
	gpointer value;
	guint ii, n_added = 0;

	if (e_html_editor_spell_is_single_lang (self))
		return e_html_editor_create_submenu_from_actions (self, NULL, self->priv->spell_suggest_actions);

	submenu_by_action = g_hash_table_new (g_direct_hash, g_direct_equal);

	g_hash_table_iter_init (&iter, self->priv->spell_suggest_menus_by_code);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		EHTMLEditorActionMenuPair *pair = value;

		if (e_ui_action_is_visible (pair->parent_menu_action))
			g_hash_table_insert (submenu_by_action, pair->parent_menu_action, pair->submenu_items);
	}

	menu = g_menu_new ();

	for (ii = 0; ii < self->priv->spell_suggest_actions->len; ii++) {
		EUIAction *subaction = g_ptr_array_index (self->priv->spell_suggest_actions, ii);
		GMenuModel *submenu;

		if (!e_ui_action_is_visible (subaction))
			continue;

		submenu = g_hash_table_lookup (submenu_by_action, subaction);
		if (!submenu) {
			g_warn_if_reached ();
			continue;
		}

		item = g_menu_item_new_submenu (e_ui_action_get_label (subaction), submenu);
		g_menu_append_item (menu, item);
		g_clear_object (&item);

		n_added++;
	}

	g_hash_table_destroy (submenu_by_action);

	if (n_added)
		item = g_menu_item_new_section (NULL, G_MENU_MODEL (menu));
	else
		item = NULL;

	g_clear_object (&menu);

	return item ? G_OBJECT (item) : NULL;
}

static GObject *
e_html_editor_create_spell_suggest_more_menu (EHTMLEditor *self,
					      EUIAction *action)
{
	if (!e_html_editor_spell_is_single_lang (self))
		return NULL;

	return e_html_editor_create_submenu_from_actions (self, e_ui_action_get_label (action), self->priv->spell_suggest_more_actions);
}

static gboolean
e_html_editor_ui_manager_create_item_cb (EUIManager *ui_manager,
					 EUIElement *elem,
					 EUIAction *action,
					 EUIElementKind for_kind,
					 GObject **out_item,
					 gpointer user_data)
{
	EHTMLEditor *self = user_data;
	const gchar *name;

	g_return_val_if_fail (E_IS_HTML_EDITOR (self), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EHTMLEditor::"))
		return FALSE;

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		if (is_action ("EHTMLEditor::recent-languages"))
			*out_item = G_OBJECT (g_menu_item_new_section (NULL, G_MENU_MODEL (self->priv->recent_languages_menu)));
		else if (is_action ("EHTMLEditor::all-languages"))
			*out_item = G_OBJECT (g_menu_item_new_section (NULL, G_MENU_MODEL (self->priv->all_languages_menu)));
		else if (is_action ("EHTMLEditor::context-spell-suggest"))
			*out_item = e_html_editor_create_spell_suggest (self, action);
		else if (is_action ("EHTMLEditor::context-spell-suggest-more-menu"))
			*out_item = e_html_editor_create_spell_suggest_more_menu (self, action);
		else if (is_action ("EHTMLEditor::context-spell-add-menu"))
			*out_item = e_html_editor_create_spell_add_menu (self, action);
		else if (is_action ("EHTMLEditor::insert-emoticon"))
			*out_item = G_OBJECT (g_menu_item_new_submenu (e_ui_action_get_label (action), G_MENU_MODEL (self->priv->emoticon_menu)));
		else
			g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		GtkWidget *widget = NULL;
		gboolean claimed = FALSE;

		if (is_action ("EHTMLEditor::editing-mode")) {
			widget = e_action_combo_box_new_with_action (E_HTML_EDITOR_ACTION_MODE_HTML (self));
			gtk_widget_set_focus_on_click (widget, FALSE);
			gtk_widget_set_tooltip_text (widget, _("Editing Mode"));
		} else if (is_action ("EHTMLEditor::paragraph-style")) {
			widget = e_action_combo_box_new_with_action (E_HTML_EDITOR_ACTION_STYLE_NORMAL (self));
			gtk_widget_set_focus_on_click (widget, FALSE);
			gtk_widget_set_tooltip_text (widget, _("Paragraph Style"));

			/* Hide the items from the action combo box as well */
			g_signal_connect_object (self, "after-mode-changed",
				G_CALLBACK (e_action_combo_box_update_model), widget, G_CONNECT_SWAPPED);
		} else if (is_action ("EHTMLEditor::font-name")) {
			widget = e_html_editor_util_create_font_name_combo ();
			gtk_widget_set_focus_on_click (widget, FALSE);
			gtk_widget_set_tooltip_text (widget, _("Font Name"));

			g_signal_connect_object (self, "after-mode-changed",
				G_CALLBACK (e_html_editor_bind_font_name_after_mode_changed_cb), widget, 0);
			e_html_editor_bind_font_name_after_mode_changed_cb (self, widget);
		} else if (is_action ("EHTMLEditor::font-size")) {
			widget = e_action_combo_box_new_with_action (E_HTML_EDITOR_ACTION_SIZE_PLUS_ZERO (self));
			gtk_widget_set_focus_on_click (widget, FALSE);
			gtk_widget_set_tooltip_text (widget, _("Font Size"));
		} else if (is_action ("EHTMLEditor::font-color")) {
			widget = e_color_combo_new ();
			gtk_widget_set_tooltip_text (widget, _("Font Color"));

			g_signal_connect_object (self, "after-mode-changed",
				G_CALLBACK (e_html_editor_bind_font_color_after_mode_changed_cb), widget, 0);
			e_html_editor_bind_font_color_after_mode_changed_cb (self, widget);
		} else if (is_action ("EHTMLEditor::background-color")) {
			GdkRGBA transparent = { 0, 0, 0, 0 };

			widget = e_color_combo_new ();
			e_color_combo_set_default_color (E_COLOR_COMBO (widget), &transparent);
			e_color_combo_set_current_color (E_COLOR_COMBO (widget), &transparent);
			e_color_combo_set_default_transparent (E_COLOR_COMBO (widget), TRUE);
			gtk_widget_set_tooltip_text (widget, _("Background Color"));

			g_signal_connect_object (self, "after-mode-changed",
				G_CALLBACK (e_html_editor_bind_background_color_after_mode_changed_cb), widget, 0);
			e_html_editor_bind_background_color_after_mode_changed_cb (self, widget);
		} else {
			g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
			claimed = TRUE;
		}

		if (widget) {
			GtkToolItem *tool_item;

			tool_item = gtk_tool_item_new ();
			gtk_container_add (GTK_CONTAINER (tool_item), widget);
			gtk_widget_show_all (GTK_WIDGET (tool_item));

			*out_item = G_OBJECT (tool_item);
		} else if (!claimed) {
			g_warning ("%s: Did not get toolbar widget for '%s'", G_STRFUNC, name);
		}
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

static void
e_html_editor_customize_toolbar_activate_cb (GtkWidget *toolbar,
					     const gchar *id,
					     gpointer user_data)
{
	EHTMLEditor *self = user_data;
	EUICustomizeDialog *dialog;
	GtkWidget *toplevel;

	g_return_if_fail (E_IS_HTML_EDITOR (self));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

	dialog = e_ui_customize_dialog_new (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL);

	e_ui_customize_dialog_add_customizer (dialog, e_ui_manager_get_customizer (self->priv->ui_manager));
	e_ui_customize_dialog_run (dialog, id);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
html_editor_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODE:
			e_html_editor_set_mode (
				E_HTML_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_FILENAME:
			e_html_editor_set_filename (
				E_HTML_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_PASTE_PLAIN_PREFER_PRE:
			html_editor_set_paste_plain_prefer_pre (
				E_HTML_EDITOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODE:
			g_value_set_enum (
				value, e_html_editor_get_mode (
				E_HTML_EDITOR (object)));
			return;

		case PROP_FILENAME:
			g_value_set_string (
				value, e_html_editor_get_filename (
				E_HTML_EDITOR (object)));
			return;

		case PROP_PASTE_PLAIN_PREFER_PRE:
			g_value_set_boolean (
				value, html_editor_get_paste_plain_prefer_pre (
				E_HTML_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_constructed (GObject *object)
{
	EHTMLEditor *editor = E_HTML_EDITOR (object);
	EHTMLEditorPrivate *priv = editor->priv;
	EUICustomizer *customizer;
	GSettings *settings;
	GtkWidget *widget;
	gchar *filename;
	const gchar *toolbar_id;
	GError *local_error = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_html_editor_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Register the markdown editor */
	priv->markdown_editor = g_object_ref_sink (e_markdown_editor_new ());
	e_html_editor_register_content_editor (editor, MARKDOWN_EDITOR_NAME, E_CONTENT_EDITOR (priv->markdown_editor));

	/* Construct the main editing area. */
	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"visible", TRUE,
		NULL);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 4, 1, 1);
	priv->content_editors_box = widget;

	e_ui_manager_freeze (priv->ui_manager);

	e_html_editor_actions_add_actions (editor);
	priv->editor_layout_row = 2;

	customizer = e_ui_manager_get_customizer (priv->ui_manager);
	filename = html_editor_find_ui_file ("e-html-editor.eui");

	if (!e_ui_parser_merge_file (e_ui_manager_get_parser (priv->ui_manager), filename, &local_error))
		g_critical ("Couldn't load .eui file: %s", local_error ? local_error->message : "Unknown error");

	g_free (filename);
	g_clear_error (&local_error);

	e_ui_manager_thaw (priv->ui_manager);

	priv->main_menu = E_UI_MENU (e_ui_manager_create_item (editor->priv->ui_manager, "main-menu"));

	e_ui_customizer_register (customizer, "main-menu", NULL);
	e_ui_customizer_register (customizer, "context-menu", _("Context Menu"));

	if (e_util_get_use_header_bar ())
		toolbar_id = "main-toolbar-with-headerbar";
	else
		toolbar_id = "main-toolbar-without-headerbar";
	widget = GTK_WIDGET (e_ui_manager_create_item (editor->priv->ui_manager, toolbar_id));
	editor->priv->main_toolbar = g_object_ref_sink (widget);
	e_ui_customizer_register (customizer, toolbar_id, _("Main Toolbar"));
	e_ui_customizer_util_attach_toolbar_context_menu (widget, toolbar_id,
		e_html_editor_customize_toolbar_activate_cb, editor);

	/* Construct the editing toolbars. */

	widget = GTK_WIDGET (e_ui_manager_create_item (editor->priv->ui_manager, "edit-toolbar"));
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 0, 1, 1);
	priv->edit_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);
	e_ui_customizer_register (customizer, "edit-toolbar", _("Edit Toolbar"));
	e_ui_customizer_util_attach_toolbar_context_menu (widget, "edit-toolbar",
		e_html_editor_customize_toolbar_activate_cb, editor);

	widget = GTK_WIDGET (e_ui_manager_create_item (editor->priv->ui_manager, "html-toolbar"));
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 1, 1, 1);
	priv->html_toolbar = g_object_ref (widget);
	e_ui_customizer_register (customizer, "html-toolbar", _("HTML Toolbar"));
	e_ui_customizer_util_attach_toolbar_context_menu (widget, "html-toolbar",
		e_html_editor_customize_toolbar_activate_cb, editor);

	/* Construct the activity bar. */

	widget = e_activity_bar_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 2, 1, 1);
	priv->activity_bar = g_object_ref (widget);

	/* Construct the alert bar for errors. */

	widget = e_alert_bar_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 3, 1, 1);
	priv->alert_bar = g_object_ref (widget);
	/* EAlertBar controls its own visibility. */

	e_ui_manager_freeze (priv->ui_manager);
	/* do this only when the EUIMenu and others are available */
	e_html_editor_actions_setup_actions (editor);
	e_ui_manager_thaw (priv->ui_manager);

	/* Have the default editor added (it's done inside the function) */
	widget = GTK_WIDGET (e_html_editor_get_content_editor (editor));
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		editor, "mode",
		E_HTML_EDITOR_ACTION (editor, "paragraph-style-menu"), "visible",
		G_BINDING_SYNC_CREATE,
		e_html_editor_mode_to_bool_hide_in_markdown_cb,
		NULL, NULL, NULL);

	e_binding_bind_property_full (
		editor, "mode",
		E_HTML_EDITOR_ACTION (editor, "justify-menu"), "visible",
		G_BINDING_SYNC_CREATE,
		e_html_editor_mode_to_bool_hide_in_markdown_cb,
		NULL, NULL, NULL);

	e_binding_bind_property_full (
		editor, "mode",
		E_HTML_EDITOR_ACTION_WRAP_LINES (editor), "visible",
		G_BINDING_SYNC_CREATE,
		e_html_editor_mode_to_bool_hide_in_markdown_cb,
		NULL, NULL, NULL);

	e_binding_bind_property_full (
		editor, "mode",
		E_HTML_EDITOR_ACTION_INDENT (editor), "visible",
		G_BINDING_SYNC_CREATE,
		e_html_editor_mode_to_bool_hide_in_markdown_cb,
		NULL, NULL, NULL);

	e_binding_bind_property_full (
		editor, "mode",
		E_HTML_EDITOR_ACTION_UNINDENT (editor), "visible",
		G_BINDING_SYNC_CREATE,
		e_html_editor_mode_to_bool_hide_in_markdown_cb,
		NULL, NULL, NULL);

	e_binding_bind_property_full (
		priv->edit_toolbar, "visible",
		priv->html_toolbar, "visible",
		G_BINDING_SYNC_CREATE,
		e_html_editor_edit_html_toolbar_visible_cb,
		NULL, editor, NULL);


	g_signal_connect_after (object, "realize", G_CALLBACK (html_editor_realize), NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "composer-paste-plain-prefer-pre",
		editor, "paste-plain-prefer-pre",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-show-edit-toolbar",
		priv->edit_toolbar, "visible",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);
}

static void
html_editor_dispose (GObject *object)
{
	EHTMLEditor *self = E_HTML_EDITOR (object);

	if (self->priv->mode_change_content_cancellable)
		g_cancellable_cancel (self->priv->mode_change_content_cancellable);

	g_clear_object (&self->priv->ui_manager);

	self->priv->core_actions = NULL;
	self->priv->core_editor_actions = NULL;
	self->priv->html_actions = NULL;
	self->priv->context_actions = NULL;
	self->priv->html_context_actions = NULL;
	self->priv->language_actions = NULL;
	self->priv->spell_check_actions = NULL;
	self->priv->suggestion_actions = NULL;

	g_clear_object (&self->priv->emoticon_menu);
	g_clear_object (&self->priv->recent_languages_menu);
	g_clear_object (&self->priv->all_languages_menu);
	g_clear_pointer (&self->priv->spell_suggest_actions, g_ptr_array_unref);
	g_clear_pointer (&self->priv->spell_suggest_more_actions, g_ptr_array_unref);
	g_clear_pointer (&self->priv->spell_add_actions, g_ptr_array_unref);
	g_clear_pointer (&self->priv->spell_suggest_menus_by_code, g_hash_table_unref);

	g_clear_object (&self->priv->main_menu);
	g_clear_object (&self->priv->main_toolbar);
	g_clear_object (&self->priv->edit_toolbar);
	g_clear_object (&self->priv->html_toolbar);
	g_clear_object (&self->priv->activity_bar);
	g_clear_object (&self->priv->alert_bar);
	g_clear_object (&self->priv->edit_area);
	g_clear_object (&self->priv->markdown_editor);

	g_clear_object (&self->priv->mode_change_content_cancellable);

	g_clear_pointer (&self->priv->filename, g_free);
	g_clear_pointer (&self->priv->context_hover_uri, g_free);

	/* Do not unbind/disconnect signal handlers here, just free/unset them */
	g_slist_free_full (self->priv->content_editor_bindings, g_object_unref);
	self->priv->content_editor_bindings = NULL;
	self->priv->subscript_notify_id = 0;
	self->priv->superscript_notify_id = 0;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_parent_class)->dispose (object);
}

static void
html_editor_finalize (GObject *object)
{
	EHTMLEditor *editor = E_HTML_EDITOR (object);

	g_hash_table_destroy (editor->priv->cid_parts);
	g_hash_table_destroy (editor->priv->content_editors);
	g_hash_table_destroy (editor->priv->content_editors_for_mode);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_html_editor_parent_class)->finalize (object);
}

static void
html_editor_submit_alert (EAlertSink *alert_sink,
                          EAlert *alert)
{
	EHTMLEditor *self = E_HTML_EDITOR (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (self->priv->alert_bar), alert);
}

static void
e_html_editor_class_init (EHTMLEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = html_editor_set_property;
	object_class->get_property = html_editor_get_property;
	object_class->constructed = html_editor_constructed;
	object_class->dispose = html_editor_dispose;
	object_class->finalize = html_editor_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->parent_set = html_editor_parent_changed;

	class->update_actions = html_editor_update_actions;
	class->spell_languages_changed = html_editor_spell_languages_changed;

	g_object_class_install_property (
		object_class,
		PROP_MODE,
		g_param_spec_enum (
			"mode",
			NULL,
			NULL,
			E_TYPE_CONTENT_EDITOR_MODE,
			E_CONTENT_EDITOR_MODE_HTML,
			G_PARAM_READWRITE |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_PASTE_PLAIN_PREFER_PRE,
		g_param_spec_boolean (
			"paste-plain-prefer-pre",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EHTMLEditorClass, update_actions),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 3,
		G_TYPE_UINT,
		G_TYPE_STRING,
		G_TYPE_STRING);

	signals[SPELL_LANGUAGES_CHANGED] = g_signal_new (
		"spell-languages-changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EHTMLEditorClass, spell_languages_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[AFTER_MODE_CHANGED] = g_signal_new (
		"after-mode-changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_html_editor_alert_sink_init (EAlertSinkInterface *interface)
{
	interface->submit_alert = html_editor_submit_alert;
}

static void
e_html_editor_init (EHTMLEditor *self)
{
	self->priv = e_html_editor_get_instance_private (self);
	self->priv->mode = E_CONTENT_EDITOR_MODE_HTML;
	self->priv->ui_manager = e_ui_manager_new (e_ui_customizer_util_dup_filename_for_component ("html-editor"));
	self->priv->core_actions = e_ui_manager_get_action_group (self->priv->ui_manager, "core");
	self->priv->core_editor_actions = e_ui_manager_get_action_group (self->priv->ui_manager, "core-editor");
	self->priv->html_actions = e_ui_manager_get_action_group (self->priv->ui_manager, "html");
	self->priv->context_actions = e_ui_manager_get_action_group (self->priv->ui_manager, "core-context");
	self->priv->html_context_actions = e_ui_manager_get_action_group (self->priv->ui_manager, "html-context");
	self->priv->language_actions = e_ui_manager_get_action_group (self->priv->ui_manager, "language");
	self->priv->spell_check_actions = e_ui_manager_get_action_group (self->priv->ui_manager, "spell-check");
	self->priv->suggestion_actions = e_ui_manager_get_action_group (self->priv->ui_manager, "suggestion");
	self->priv->cid_parts = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, g_object_unref);
	self->priv->content_editors = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);
	self->priv->content_editors_for_mode = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

	self->priv->emoticon_menu = g_menu_new ();
	self->priv->recent_languages_menu = g_menu_new ();
	self->priv->all_languages_menu = g_menu_new ();
	self->priv->spell_suggest_actions = g_ptr_array_new_with_free_func (g_object_unref);
	self->priv->spell_suggest_more_actions = g_ptr_array_new_with_free_func (g_object_unref);
	self->priv->spell_add_actions = g_ptr_array_new_with_free_func (g_object_unref);
	self->priv->spell_suggest_menus_by_code = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, e_html_editor_action_menu_pair_free);

	g_signal_connect_object (self->priv->ui_manager, "create-item",
		G_CALLBACK (e_html_editor_ui_manager_create_item_cb), self, 0);
}

static void
e_html_editor_content_editor_initialized (EContentEditor *content_editor,
					  gpointer user_data)
{
	ESimpleAsyncResult *async_result = user_data;
	EHTMLEditor *html_editor;

	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (async_result));

	html_editor = e_simple_async_result_get_user_data (async_result);
	g_return_if_fail (E_IS_HTML_EDITOR (html_editor));
	g_return_if_fail (content_editor == e_html_editor_get_content_editor (html_editor));

	/* Synchronize widget mode with the buttons */
	e_html_editor_set_mode (html_editor, E_CONTENT_EDITOR_MODE_HTML);

	/* Make sure the actions did bind, even when the content editor did not change */
	e_html_editor_actions_unbind (html_editor);
	e_html_editor_actions_bind (html_editor);

	g_object_set (G_OBJECT (content_editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"changed", FALSE,
		NULL);

	e_simple_async_result_complete (async_result);

	g_object_unref (async_result);
}

/**
 * e_html_editor_new:
 * @callback: a callback to be called when the editor is ready
 * @user_data: a used data passed into the @callback
 *
 * Constructs a new #EHTMLEditor asynchronously. The result is returned
 * by e_html_editor_new_finish(), which should be called inside @callback.
 *
 * Since: 3.22
 **/
void
e_html_editor_new (GAsyncReadyCallback callback,
		   gpointer user_data)
{
	EHTMLEditor *html_editor;
	EContentEditor *content_editor;
	ESimpleAsyncResult *async_result;

	g_return_if_fail (callback != NULL);

	html_editor = g_object_new (E_TYPE_HTML_EDITOR, NULL);
	async_result = e_simple_async_result_new (NULL, callback, user_data, e_html_editor_new);
	e_simple_async_result_set_user_data (async_result, html_editor, g_object_unref);

	content_editor = e_html_editor_get_content_editor (html_editor);
	e_content_editor_initialize (content_editor, e_html_editor_content_editor_initialized, async_result);
}

/**
 * e_html_editor_new_finish:
 * @result: a #GAsyncResult passed to callback from e_html_editor_new()
 * @error: an optional #GError
 *
 * Finishes the call of e_html_editor_new().
 *
 * Returns: (transfer-full): A newly created #EHTMLEditor.
 *
 * Since: 3.22
 **/
GtkWidget *
e_html_editor_new_finish (GAsyncResult *result,
			  GError **error)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_html_editor_new), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return e_simple_async_result_steal_user_data (eresult);
}

/**
 * e_html_editor_connect_focus_tracker:
 * @editor: an #EHTMLEditor
 * @focus_tracker: an #EFocusTracker
 *
 * Connects @editor actions and widgets to the @focus_tracker.
 *
 * Since: 3.44
 **/
void
e_html_editor_connect_focus_tracker (EHTMLEditor *editor,
				     EFocusTracker *focus_tracker)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	e_focus_tracker_set_cut_clipboard_action (focus_tracker,
		e_html_editor_get_action (editor, "cut"));

	e_focus_tracker_set_copy_clipboard_action (focus_tracker,
		e_html_editor_get_action (editor, "copy"));

	e_focus_tracker_set_paste_clipboard_action (focus_tracker,
		e_html_editor_get_action (editor, "paste"));

	e_focus_tracker_set_select_all_action (focus_tracker,
		e_html_editor_get_action (editor, "select-all"));

	e_focus_tracker_set_undo_action (focus_tracker,
		e_html_editor_get_action (editor, "undo"));

	e_focus_tracker_set_redo_action (focus_tracker,
		e_html_editor_get_action (editor, "redo"));

	e_markdown_editor_connect_focus_tracker (E_MARKDOWN_EDITOR (editor->priv->markdown_editor), focus_tracker);
}

/**
 * e_html_editor_get_content_box:
 * @editor: an #EHTMLEditor
 *
 * Returns: (transfer none): the content box, the content editors are
 *    packed into.
 *
 * Since: 3.44
 **/
GtkWidget *
e_html_editor_get_content_box (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	return editor->priv->content_editors_box;
}

static void
e_html_editor_content_editor_notify_mode_cb (GObject *object,
					     GParamSpec *param,
					     gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (E_IS_CONTENT_EDITOR (object));

	if (E_CONTENT_EDITOR (object) == e_html_editor_get_content_editor (editor)) {
		EContentEditorMode mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

		g_object_get (object, "mode", &mode, NULL);

		e_html_editor_set_mode (editor, mode);
	}
}

static gboolean
html_editor_scroll_event_cb (GtkWidget *widget,
			     GdkEventScroll *event,
			     gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if ((event->state & GDK_CONTROL_MASK) != 0) {
		GdkScrollDirection direction = event->direction;

		if (direction == GDK_SCROLL_SMOOTH) {
			static gdouble total_delta_y = 0.0;

			total_delta_y += event->delta_y;

			if (total_delta_y >= 1.0) {
				total_delta_y = 0.0;
				direction = GDK_SCROLL_DOWN;
			} else if (total_delta_y <= -1.0) {
				total_delta_y = 0.0;
				direction = GDK_SCROLL_UP;
			} else if (event->delta_y >= 1e-9 || event->delta_y <= -1e-9) {
				return TRUE;
			} else {
				return FALSE;
			}
		}

		switch (direction) {
			case GDK_SCROLL_UP:
				e_html_editor_zoom_in (editor, E_CONTENT_EDITOR (widget));
				return TRUE;
			case GDK_SCROLL_DOWN:
				e_html_editor_zoom_out (editor, E_CONTENT_EDITOR (widget));
				return TRUE;
			default:
				break;
		}
	}

	return FALSE;
}

static EContentEditor *
e_html_editor_get_content_editor_for_mode (EHTMLEditor *editor,
					   EContentEditorMode mode)
{
	EContentEditor *cnt_editor;
	GSettings *settings;
	const gchar *mode_name = NULL;
	gchar *name;

	if (!g_hash_table_size (editor->priv->content_editors))
		return NULL;

	cnt_editor = g_hash_table_lookup (editor->priv->content_editors_for_mode, GINT_TO_POINTER (mode));

	if (cnt_editor)
		return cnt_editor;

	switch (mode) {
	case E_CONTENT_EDITOR_MODE_UNKNOWN:
		g_warn_if_reached ();
		break;
	case E_CONTENT_EDITOR_MODE_PLAIN_TEXT:
		mode_name = "plain";
		break;
	case E_CONTENT_EDITOR_MODE_HTML:
		mode_name = "html";
		break;
	case E_CONTENT_EDITOR_MODE_MARKDOWN:
		mode_name = "markdown";
		break;
	case E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT:
		mode_name = "markdown-plain";
		break;
	case E_CONTENT_EDITOR_MODE_MARKDOWN_HTML:
		mode_name = "markdown-html";
		break;
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	name = g_settings_get_string (settings, "composer-editor");
	g_clear_object (&settings);

	if (name && *name && mode_name) {
		gchar **split_names;
		gint ii, mode_name_len = strlen (mode_name);

		split_names = g_strsplit (name, ",", -1);

		/* first round with the mode-specific overrides */
		for (ii = 0; split_names && split_names[ii] && !cnt_editor; ii++) {
			const gchar *check_name = split_names[ii];

			if (g_ascii_strncasecmp (check_name, mode_name, mode_name_len) == 0 &&
			    check_name[mode_name_len] == ':') {
				cnt_editor = g_hash_table_lookup (editor->priv->content_editors, check_name + mode_name_len + 1);

				if (cnt_editor && !e_content_editor_supports_mode (cnt_editor, mode))
					cnt_editor = NULL;
			}
		}

		/* second round without the mode-specific overrides */
		for (ii = 0; split_names && split_names[ii] && !cnt_editor; ii++) {
			const gchar *check_name = split_names[ii];

			cnt_editor = g_hash_table_lookup (editor->priv->content_editors, check_name);

			if (cnt_editor && !e_content_editor_supports_mode (cnt_editor, mode))
				cnt_editor = NULL;
		}

		g_strfreev (split_names);
	}

	g_free (name);

	if (!cnt_editor) {
		if (mode == E_CONTENT_EDITOR_MODE_MARKDOWN ||
		    mode == E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT ||
		    mode == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML)
			cnt_editor = g_hash_table_lookup (editor->priv->content_editors, MARKDOWN_EDITOR_NAME);
		else
			cnt_editor = g_hash_table_lookup (editor->priv->content_editors, DEFAULT_CONTENT_EDITOR_NAME);

		if (cnt_editor && !e_content_editor_supports_mode (cnt_editor, mode))
			cnt_editor = NULL;
	}

	if (!cnt_editor) {
		GHashTableIter iter;
		gpointer value;

		g_hash_table_iter_init (&iter, editor->priv->content_editors);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			if (e_content_editor_supports_mode (value, mode)) {
				cnt_editor = value;
				break;
			}
		}
	}

	if (cnt_editor) {
		GHashTableIter iter;
		gpointer value;

		g_hash_table_iter_init (&iter, editor->priv->content_editors_for_mode);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			/* The editor can be used for multiple modes and it is already packed in the content box. */
			if (value == cnt_editor) {
				g_hash_table_insert (editor->priv->content_editors_for_mode, GINT_TO_POINTER (mode), cnt_editor);
				return cnt_editor;
			}
		}
	}

	if (cnt_editor) {
		e_content_editor_setup_editor (cnt_editor, editor);

		g_signal_connect_swapped (cnt_editor, "ref-mime-part",
			G_CALLBACK (e_html_editor_ref_cid_part), editor);

		e_signal_connect_notify (cnt_editor, "notify::mode",
			G_CALLBACK (e_html_editor_content_editor_notify_mode_cb), editor);

		/* Pack editors which implement GtkScrollable in a scrolled window */
		if (GTK_IS_SCROLLABLE (cnt_editor)) {
			GtkWidget *scrolled_window;

			scrolled_window = gtk_scrolled_window_new (NULL, NULL);
			gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

			gtk_box_pack_start (GTK_BOX (editor->priv->content_editors_box), scrolled_window, TRUE, TRUE, 0);

			gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (cnt_editor));

			e_binding_bind_property (cnt_editor, "visible",
				scrolled_window, "visible",
				G_BINDING_SYNC_CREATE);
		} else {
			gtk_box_pack_start (GTK_BOX (editor->priv->content_editors_box), GTK_WIDGET (cnt_editor), TRUE, TRUE, 0);
		}

		g_signal_connect (
			cnt_editor, "context-menu-requested",
			G_CALLBACK (html_editor_context_menu_requested_cb), editor);

		g_signal_connect (
			cnt_editor, "scroll-event",
			G_CALLBACK (html_editor_scroll_event_cb), editor);

		g_hash_table_insert (editor->priv->content_editors_for_mode, GINT_TO_POINTER (mode), cnt_editor);
	}

	return cnt_editor;
}

gboolean
e_html_editor_has_editor_for_mode (EHTMLEditor *editor,
				   EContentEditorMode mode)
{
	GHashTableIter iter;
	gpointer value;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), FALSE);

	g_hash_table_iter_init (&iter, editor->priv->content_editors);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		EContentEditor *cnt_editor = value;

		if (e_content_editor_supports_mode (cnt_editor, mode))
			return TRUE;
	}

	return FALSE;
}

/**
 * e_html_editor_get_content_editor:
 * @editor: an #EHTMLEditor
 *
 * Returns instance of #EContentEditor used in the @editor.
 */
EContentEditor *
e_html_editor_get_content_editor (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	if (!editor->priv->use_content_editor) {
		editor->priv->use_content_editor =
			e_html_editor_get_content_editor_for_mode (editor, editor->priv->mode);
	}

	return editor->priv->use_content_editor;
}

/* Private function */
const gchar *
e_html_editor_get_content_editor_name (EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	GHashTableIter iter;
	gpointer key, value;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (!cnt_editor)
		return NULL;

	g_hash_table_iter_init (&iter, editor->priv->content_editors);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (value == cnt_editor)
			return key;
	}

	return NULL;
}

static void
e_html_editor_content_changed_cb (EContentEditor *cnt_editor,
				  gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	if (editor->priv->mode_change_content_cancellable) {
		if (cnt_editor == editor->priv->use_content_editor) {
			g_cancellable_cancel (editor->priv->mode_change_content_cancellable);
			g_clear_object (&editor->priv->mode_change_content_cancellable);
		}
	}

	g_signal_handlers_disconnect_by_func (cnt_editor,
		G_CALLBACK (e_html_editor_content_changed_cb), editor);
}

void
e_html_editor_register_content_editor (EHTMLEditor *editor,
				       const gchar *name,
                                       EContentEditor *cnt_editor)
{
	EContentEditor *already_taken;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (name != NULL);
	g_return_if_fail (E_IS_CONTENT_EDITOR (cnt_editor));

	already_taken = g_hash_table_lookup (editor->priv->content_editors, name);

	if (already_taken) {
		g_warning ("%s: Cannot register %s with name '%s', because it's already taken by %s",
			G_STRFUNC, G_OBJECT_TYPE_NAME (cnt_editor), name, G_OBJECT_TYPE_NAME (already_taken));
	} else {
		g_hash_table_insert (editor->priv->content_editors, g_strdup (name), cnt_editor);
	}
}

typedef struct _ModeChangeData {
	GWeakRef *editor_weak_ref;
	EContentEditorMode source_mode;
} ModeChangeData;

static void
e_html_editor_update_content_on_mode_change_cb (GObject *source_object,
						GAsyncResult *result,
						gpointer user_data)
{
	ModeChangeData *mcd = user_data;
	EContentEditorContentHash *content_hash;
	EContentEditorMode source_mode;
	EContentEditor *cnt_editor;
	EHTMLEditor *editor;

	g_return_if_fail (E_IS_CONTENT_EDITOR (source_object));
	g_return_if_fail (mcd != NULL);

	editor = g_weak_ref_get (mcd->editor_weak_ref);
	source_mode = mcd->source_mode;

	e_weak_ref_free (mcd->editor_weak_ref);
	g_slice_free (ModeChangeData, mcd);
	mcd = NULL;

	if (!editor)
		return;

	g_clear_object (&editor->priv->mode_change_content_cancellable);

	cnt_editor = E_CONTENT_EDITOR (source_object);
	content_hash = e_content_editor_get_content_finish (cnt_editor, result, NULL);

	if (content_hash) {
		gpointer text;

		text = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_HTML);

		if (editor->priv->mode != E_CONTENT_EDITOR_MODE_PLAIN_TEXT && text) {
			e_content_editor_insert_content (editor->priv->use_content_editor, text,
				E_CONTENT_EDITOR_INSERT_CONVERT |
				E_CONTENT_EDITOR_INSERT_TEXT_HTML |
				E_CONTENT_EDITOR_INSERT_REPLACE_ALL |
				((source_mode == E_CONTENT_EDITOR_MODE_PLAIN_TEXT) ? E_CONTENT_EDITOR_INSERT_FROM_PLAIN_TEXT : 0));
		} else {
			text = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_PLAIN);

			if (text) {
				e_content_editor_insert_content (editor->priv->use_content_editor, text,
					E_CONTENT_EDITOR_INSERT_CONVERT |
					E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
					E_CONTENT_EDITOR_INSERT_REPLACE_ALL);
			}
		}

		e_content_editor_clear_undo_redo_history (editor->priv->use_content_editor);

		e_content_editor_util_free_content_hash (content_hash);
	}

	g_object_unref (editor);
}

/**
 * e_html_editor_get_mode:
 * @editor: an #EHTMLEditor
 *
 * Returns: Current editor mode, as an #EContentEditorMode
 *
 * Since: 3.44
 **/
EContentEditorMode
e_html_editor_get_mode (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), E_CONTENT_EDITOR_MODE_PLAIN_TEXT);

	return editor->priv->mode;
}

/**
 * e_html_editor_set_mode:
 * @editor: an #EHTMLEditor
 * @mode: an #EContentEditorMode
 *
 * Sets the editor mode.
 *
 * Since: 3.44
 **/
void
e_html_editor_set_mode (EHTMLEditor *editor,
			EContentEditorMode mode)
{
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	if (mode == E_CONTENT_EDITOR_MODE_UNKNOWN)
		mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

	/* In case the same mode is set, but no editor is assigned, then assign it */
	if (editor->priv->mode == mode && editor->priv->use_content_editor != NULL)
		return;

	if (editor->priv->mode_change_content_cancellable) {
		g_cancellable_cancel (editor->priv->mode_change_content_cancellable);
		g_clear_object (&editor->priv->mode_change_content_cancellable);
	}

	cnt_editor = e_html_editor_get_content_editor_for_mode (editor, mode);

	if (cnt_editor) {
		gboolean editor_changed = cnt_editor != editor->priv->use_content_editor;

		e_ui_manager_freeze (editor->priv->ui_manager);

		if (editor_changed) {
			EContentEditorInterface *iface;
			gboolean is_focused = FALSE;
			gboolean zoom_supported;

			if (editor->priv->use_content_editor) {
				ModeChangeData *mcd;

				e_html_editor_actions_unbind (editor);

				is_focused = e_content_editor_is_focus (editor->priv->use_content_editor);

				editor->priv->mode_change_content_cancellable = g_cancellable_new ();

				g_signal_connect_object (cnt_editor, "content-changed",
					G_CALLBACK (e_html_editor_content_changed_cb), editor, 0);

				mcd = g_slice_new (ModeChangeData);
				mcd->editor_weak_ref = e_weak_ref_new (editor);
				mcd->source_mode = editor->priv->mode;

				/* Transfer also the content between editors */
				e_content_editor_get_content (editor->priv->use_content_editor,
					E_CONTENT_EDITOR_GET_TO_SEND_HTML | E_CONTENT_EDITOR_GET_TO_SEND_PLAIN,
					"localhost", editor->priv->mode_change_content_cancellable,
					e_html_editor_update_content_on_mode_change_cb,
					mcd);

				gtk_widget_hide (GTK_WIDGET (editor->priv->use_content_editor));

				if (E_IS_MARKDOWN_EDITOR (editor->priv->use_content_editor)) {
					EMarkdownEditor *markdown_editor;
					GSettings *settings;

					markdown_editor = E_MARKDOWN_EDITOR (editor->priv->use_content_editor);

					e_markdown_editor_set_preview_mode (markdown_editor, FALSE);

					settings = e_util_ref_settings ("org.gnome.evolution.mail");
					if (g_settings_get_boolean (settings, "composer-show-edit-toolbar"))
						gtk_widget_show (GTK_WIDGET (editor->priv->edit_toolbar));
					g_object_unref (settings);
				}
			}

			gtk_widget_show (GTK_WIDGET (cnt_editor));

			if (E_IS_MARKDOWN_EDITOR (cnt_editor)) {
				if (!g_object_get_data (G_OBJECT (cnt_editor), "EHTMLEditor::has-editing-mode")) {
					GtkToolbar *toolbar;
					GtkToolItem *tool_item;
					GtkWidget *widget;

					g_object_set_data (G_OBJECT (cnt_editor), "EHTMLEditor::has-editing-mode", GINT_TO_POINTER (1));

					widget = e_action_combo_box_new_with_action (ACTION (MODE_HTML));
					gtk_widget_set_focus_on_click (widget, FALSE);
					gtk_widget_set_tooltip_text (widget, _("Editing Mode"));

					tool_item = gtk_tool_item_new ();
					gtk_container_add (GTK_CONTAINER (tool_item), widget);
					gtk_widget_show_all (GTK_WIDGET (tool_item));

					toolbar = e_markdown_editor_get_action_toolbar (E_MARKDOWN_EDITOR (cnt_editor));
					gtk_toolbar_insert (toolbar, tool_item, 0);
				}

				gtk_widget_hide (GTK_WIDGET (editor->priv->edit_toolbar));
			}

			if (is_focused)
				e_content_editor_grab_focus (cnt_editor);

			/* Disable spell-check dialog when the content editor doesn't
			   support moving between misspelled words. */
			iface = E_CONTENT_EDITOR_GET_IFACE (cnt_editor);

			e_ui_action_set_visible (ACTION (SPELL_CHECK),
				iface && iface->spell_check_next_word && iface->spell_check_prev_word);

			e_content_editor_clear_undo_redo_history (cnt_editor);

			if (editor->priv->use_content_editor) {
				/* Inherit whether the inline spelling is enabled, because when there are
				   any selected languages, then it auto-enables inline spelling. */
				e_content_editor_set_spell_check_enabled (cnt_editor,
					e_content_editor_get_spell_check_enabled (editor->priv->use_content_editor));
			}

			zoom_supported = iface && iface->get_zoom_level && iface->set_zoom_level;
			action_set_visible_and_sensitive (ACTION (ZOOM_MENU), zoom_supported);
			action_set_visible_and_sensitive (ACTION (ZOOM_100), zoom_supported);
			action_set_visible_and_sensitive (ACTION (ZOOM_IN), zoom_supported);
			action_set_visible_and_sensitive (ACTION (ZOOM_OUT), zoom_supported);
		}

		editor->priv->mode = mode;
		editor->priv->use_content_editor = cnt_editor;

		if (editor_changed)
			e_html_editor_actions_bind (editor);

		g_object_set (G_OBJECT (cnt_editor), "mode", mode, NULL);
		g_object_notify (G_OBJECT (editor), "mode");

		e_ui_manager_thaw (editor->priv->ui_manager);
	}
}

/**
 * e_html_editor_cancel_mode_change_content_update:
 * @editor: an #EHTMLEditor
 *
 * Cancels any ongoing content update after the mode change. This is useful
 * when setting content before the read of the current editor's content
 * is finished, which can happen due to reading the editor's content
 * asynchronously.
 *
 * Since: 3.50
 **/
void
e_html_editor_cancel_mode_change_content_update (EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	if (editor->priv->mode_change_content_cancellable) {
		g_cancellable_cancel (editor->priv->mode_change_content_cancellable);
		g_clear_object (&editor->priv->mode_change_content_cancellable);
	}
}

/**
 * e_html_editor_get_ui_manager:
 * @editor: an #EHTMLEditor
 *
 * Returns an #EUIManager that manages all the actions in the @editor.
 *
 * Returns: (transfer none): an internal #EUIManager
 */
EUIManager *
e_html_editor_get_ui_manager (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	return editor->priv->ui_manager;
}

/**
 * e_html_editor_get_action:
 * @editor: an #EHTMLEditor
 * @action_name: name of action to lookup and return
 *
 * Gets action named @action_name from the internal #EUIManager.
 *
 * Returns: (transfer none) (nullable): an #EUIAction matching @action_name or
 *    %NULL, if no such action exists
 */
EUIAction *
e_html_editor_get_action (EHTMLEditor *editor,
                          const gchar *action_name)
{
	EUIManager *ui_manager;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_html_editor_get_ui_manager (editor);

	return e_ui_manager_get_action (ui_manager, action_name);
}

/**
 * e_html_editor_get_ui_object:
 * @editor: an #EHTMLEditor
 * @object_name: an object name to get
 *
 * Gets a UI object (usually a GtkWidget descendant) by its name.
 * There are only few objects recognized, namely:
 * E_HTML_EDITOR_UI_OBJECT_MAIN_MENU: the main menu as an #EUIMenu;
 * E_HTML_EDITOR_UI_OBJECT_MAIN_TOOLBAR: the main toolbar as a #GtkToolbar.
 *
 * Returns: (transfer none) (nullable): the UI object named @object_name, or %NULL,
 *    when the name is not recognized.
 *
 * Since: 3.56
 **/
gpointer
e_html_editor_get_ui_object (EHTMLEditor *editor,
			     const gchar *object_name)
{
	gpointer object = NULL;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);
	g_return_val_if_fail (object_name != NULL, NULL);

	if (g_strcmp0 (object_name, E_HTML_EDITOR_UI_OBJECT_MAIN_MENU) == 0)
		object = editor->priv->main_menu;
	else if (g_strcmp0 (object_name, E_HTML_EDITOR_UI_OBJECT_MAIN_TOOLBAR) == 0)
		object = editor->priv->main_toolbar;

	return object;
}

/**
 * e_html_editor_get_filename:
 * @editor: an #EHTMLEditor
 *
 * Returns path and name of file to which content of the editor should be saved.
 */
const gchar *
e_html_editor_get_filename (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	return editor->priv->filename;
}

/**
 * e_html_editor_set_filename:
 * @editor: an #EHTMLEditor
 * @filename: Target file
 *
 * Sets file to which content of the editor should be saved (see
 * e_html_editor_save()).
 */
void
e_html_editor_set_filename (EHTMLEditor *editor,
                            const gchar *filename)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	if (g_strcmp0 (editor->priv->filename, filename) == 0)
		return;

	g_free (editor->priv->filename);
	editor->priv->filename = g_strdup (filename);

	g_object_notify (G_OBJECT (editor), "filename");
}

EActivityBar *
e_html_editor_get_activity_bar (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	return E_ACTIVITY_BAR (editor->priv->activity_bar);
}

/**
 * e_html_editor_new_activity:
 * @editor: an #EHTMLEditor
 *
 * Creates and configures a new #EActivity so its progress is shown in
 * the @editor.  The #EActivity comes pre-loaded with a #CamelOperation.
 *
 * Returns: a new #EActivity for use with @editor
 **/
EActivity *
e_html_editor_new_activity (EHTMLEditor *editor)
{
	EActivity *activity;
	EActivityBar *activity_bar;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	activity = e_activity_new ();
	e_activity_set_alert_sink (activity, E_ALERT_SINK (editor));

	cancellable = camel_operation_new ();
	e_activity_set_cancellable (activity, cancellable);
	g_object_unref (cancellable);

	activity_bar = E_ACTIVITY_BAR (editor->priv->activity_bar);
	e_activity_bar_set_activity (activity_bar, activity);

	return activity;
}

/**
 * e_html_editor_pack_above:
 * @editor: an #EHTMLEditor
 * @child: a #GtkWidget
 *
 * Inserts @child right between the toolbars and the editor widget itself.
 */
void
e_html_editor_pack_above (EHTMLEditor *editor,
                          GtkWidget *child)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_grid_insert_row (GTK_GRID (editor), editor->priv->editor_layout_row);
	gtk_grid_attach (GTK_GRID (editor), child, 0, editor->priv->editor_layout_row, 1, 1);
	editor->priv->editor_layout_row++;
}

typedef struct _SaveContentData {
	GOutputStream *stream;
	GCancellable *cancellable;
} SaveContentData;

static SaveContentData *
save_content_data_new (GOutputStream *stream,
		       GCancellable *cancellable)
{
	SaveContentData *scd;

	scd = g_slice_new (SaveContentData);
	scd->stream = stream;
	scd->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

	return scd;
}

static void
save_content_data_free (gpointer ptr)
{
	SaveContentData *scd = ptr;

	if (scd) {
		g_clear_object (&scd->stream);
		g_clear_object (&scd->cancellable);
		g_slice_free (SaveContentData, scd);
	}
}

static void
e_html_editor_save_content_ready_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	ESimpleAsyncResult *simple = user_data;
	EContentEditorContentHash *content_hash;
	GError *error = NULL;

	g_return_if_fail (E_IS_CONTENT_EDITOR (source_object));
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (simple));

	content_hash = e_content_editor_get_content_finish (E_CONTENT_EDITOR (source_object), result, &error);

	if (content_hash) {
		const gchar *content;

		content = e_content_editor_util_get_content_data (content_hash, GPOINTER_TO_UINT (e_simple_async_result_get_op_pointer (simple)));

		if (!content) {
			g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to obtain content of editor"));
		} else {
			SaveContentData *scd;
			gsize written;

			scd = e_simple_async_result_get_user_data (simple);

			g_output_stream_write_all (scd->stream, content, strlen (content), &written, scd->cancellable, &error);
		}

		e_content_editor_util_free_content_hash (content_hash);

		if (error)
			e_simple_async_result_take_error (simple, error);
	} else {
		e_simple_async_result_take_error (simple, error);
	}

	e_simple_async_result_complete (simple);
	g_object_unref (simple);
}

/**
 * e_html_editor_save:
 * @editor: an #EHTMLEditor
 * @filename: file into which to save the content
 * @as_html: whether the content should be saved as HTML or plain text
 * @cancellable: an optional #GCancellable, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback to call when the save is finished
 * @user_data: (closure callback): user data passed to @callback
 *
 * Starts an asynchronous save of the current content of the #EContentEditor
 * into given file. When @as_html is @FALSE, the content is first converted
 * into plain text.
 *
 * Finish the call with e_html_editor_save_finish() from the @callback.
 *
 * Since: 3.38
 **/
void
e_html_editor_save (EHTMLEditor *editor,
		    const gchar *filename,
		    gboolean as_html,
		    GCancellable *cancellable,
		    GAsyncReadyCallback callback,
		    gpointer user_data)
{
	EContentEditor *cnt_editor;
	ESimpleAsyncResult *simple;
	EContentEditorGetContentFlags flag;
	SaveContentData *scd;
	GFile *file;
	GFileOutputStream *stream;
	GError *local_error = NULL;

	simple = e_simple_async_result_new (G_OBJECT (editor), callback, user_data, e_html_editor_save);

	file = g_file_new_for_path (filename);
	stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &local_error);
	if (local_error || !stream) {
		e_simple_async_result_take_error (simple, local_error);
		e_simple_async_result_complete_idle (simple);

		g_object_unref (simple);
		g_object_unref (file);

		return;
	}

	flag = as_html ? E_CONTENT_EDITOR_GET_TO_SEND_HTML : E_CONTENT_EDITOR_GET_TO_SEND_PLAIN;

	scd = save_content_data_new (G_OUTPUT_STREAM (stream), cancellable);

	e_simple_async_result_set_user_data (simple, scd, save_content_data_free);
	e_simple_async_result_set_op_pointer (simple, GUINT_TO_POINTER (flag), NULL);

	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_get_content (cnt_editor, flag, NULL, cancellable, e_html_editor_save_content_ready_cb, simple);

	g_object_unref (file);
}

/**
 * e_html_editor_save_finish:
 * @editor: an #EHTMLEditor
 * @result: a #GAsyncResult of the operation
 * @error: return location for a #GError, or %NULL
 *
 * Finished the previous call of e_html_editor_save().
 *
 * Returns: whether the save succeeded.
 *
 * Since: 3.38
 **/
gboolean
e_html_editor_save_finish (EHTMLEditor *editor,
			   GAsyncResult *result,
			   GError **error)
{
	g_return_val_if_fail (e_simple_async_result_is_valid (result, G_OBJECT (editor), e_html_editor_save), FALSE);

	return !e_simple_async_result_propagate_error (E_SIMPLE_ASYNC_RESULT (result), error);
}

/**
 * e_html_editor_add_cid_part:
 * @editor: an #EHTMLEditor
 * @mime_part: a #CamelMimePart
 *
 * Add the @mime_part with its Content-ID (if not set, one is assigned),
 * which can be later obtained with e_html_editor_ref_cid_part();
 *
 * Since: 3.38
 **/
void
e_html_editor_add_cid_part (EHTMLEditor *editor,
			    CamelMimePart *mime_part)
{
	const gchar *cid;
	gchar *cid_uri;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));

	cid = camel_mime_part_get_content_id (mime_part);

	if (!cid) {
		camel_mime_part_set_content_id (mime_part, NULL);
		cid = camel_mime_part_get_content_id (mime_part);
	}

	cid_uri = g_strconcat ("cid:", cid, NULL);

	g_hash_table_insert (editor->priv->cid_parts, cid_uri, g_object_ref (mime_part));
}

/**
 * e_html_editor_remove_cid_part:
 * @editor: an #EHTMLEditor
 * @cid_uri: a Content ID URI (starts with "cid:") to remove
 *
 * Removes CID part with given @cid_uri, previously added with
 * e_html_editor_add_cid_part(). The function does nothing if no
 * such part is stored.
 *
 * Since: 3.38
 **/
void
e_html_editor_remove_cid_part (EHTMLEditor *editor,
			       const gchar *cid_uri)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (cid_uri != NULL);

	g_hash_table_remove (editor->priv->cid_parts, cid_uri);
}

/**
 * e_html_editor_remove_all_cid_parts:
 * @editor: an #EHTMLEditor
 *
 * Removes all CID parts previously added with
 * e_html_editor_add_cid_part().
 *
 * Since: 3.38
 **/
void
e_html_editor_remove_all_cid_parts (EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	g_hash_table_remove_all (editor->priv->cid_parts);
}

typedef struct _RemoveUnusedCidPartsData {
	GHashTable *used_hash; /* CamelMimePart * ~> NULL */
	GSList **out_removed_mime_parts;
} RemoveUnusedCidPartsData;

static gboolean
remove_unused_cid_parts_cb (gpointer key,
			    gpointer value,
			    gpointer user_data)
{
	RemoveUnusedCidPartsData *data = user_data;
	CamelMimePart *mime_part = value;
	gboolean remove;

	remove = !data->used_hash || !g_hash_table_contains (data->used_hash, mime_part);

	if (remove && data->out_removed_mime_parts)
		*data->out_removed_mime_parts = g_slist_prepend (*data->out_removed_mime_parts, g_object_ref (mime_part));

	return remove;
}

/**
 * e_html_editor_remove_unused_cid_parts:
 * @editor: an #EHTMLEditor
 * @used_mime_parts: (nullable) (element-type CamelMimePart): list of used CamelMimePart-s to keep
 * @out_removed_mime_parts: (out) (optional) (element-type CamelMimePart) (transfer full): list of removed parts
 *
 * Traverses the list of "cid:" parts and removes all which are not part
 * of the @used_mime_parts.
 *
 * The optional @out_removed_mime_parts is filled with the removed parts.
 * Free it with g_slist_free_full (list, g_object_unref);, when no longer needed.
 *
 * Since: 3.44
 **/
void
e_html_editor_remove_unused_cid_parts (EHTMLEditor *editor,
				       GSList *used_mime_parts,
				       GSList **out_removed_mime_parts)
{
	RemoveUnusedCidPartsData data;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	if (out_removed_mime_parts)
		*out_removed_mime_parts = NULL;

	data.used_hash = NULL;
	data.out_removed_mime_parts = out_removed_mime_parts;

	if (used_mime_parts) {
		GSList *link;

		data.used_hash = g_hash_table_new (g_direct_hash, g_direct_equal);

		for (link = used_mime_parts; link; link = g_slist_next (link)) {
			g_hash_table_insert (data.used_hash, link->data, NULL);
		}
	}

	g_hash_table_foreach_remove (editor->priv->cid_parts, remove_unused_cid_parts_cb, &data);

	if (data.used_hash)
		g_hash_table_destroy (data.used_hash);

	if (out_removed_mime_parts)
		*out_removed_mime_parts = g_slist_reverse (*out_removed_mime_parts);
}

/**
 * e_html_editor_ref_cid_part:
 * @editor: an #EHTMLEditor
 * @cid_uri: a Content ID URI (starts with "cid:") to obtain the part for
 *
 * References a #CamelMimePart with given @cid_uri as Content ID, if
 * previously added with e_html_editor_add_cid_part(). The @cid_uri
 * should start with "cid:".
 *
 * The returned non-%NULL object is references, thus it should be freed
 * with g_object_unref(), when no longer needed.
 *
 * Returns: (transfer full) (nullable): a #CamelMimePart with given Content ID,
 *    or %NULL, if not found.
 *
 * Since: 3.38
 **/
CamelMimePart *
e_html_editor_ref_cid_part (EHTMLEditor *editor,
			   const gchar *cid_uri)
{
	CamelMimePart *mime_part;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);
	g_return_val_if_fail (cid_uri != NULL, NULL);

	mime_part = g_hash_table_lookup (editor->priv->cid_parts, cid_uri);

	if (mime_part)
		g_object_ref (mime_part);

	return mime_part;
}

/**
 * e_html_editor_clear_alerts:
 * @editor: an #EHTMLEditor
 *
 * Clears all shown alerts in the @editor.
 *
 * Since: 3.50
 **/
void
e_html_editor_clear_alerts (EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	if (editor->priv->alert_bar)
		e_alert_bar_clear (E_ALERT_BAR (editor->priv->alert_bar));
}

/**
 * e_html_editor_get_alert_bar:
 * @editor: an #EHTMLEditor
 *
 * Returns an #EAlertBar used by the @editor.
 *
 * Returns: (transfer none) (nullable): an #EAlertBar used by the @editor
 *
 * Since: 3.50
 **/
EAlertBar *
e_html_editor_get_alert_bar (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	return editor->priv->alert_bar ? E_ALERT_BAR (editor->priv->alert_bar) : NULL;
}
