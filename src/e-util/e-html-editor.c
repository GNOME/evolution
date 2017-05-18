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
#include <enchant/enchant.h>
#include <libedataserver/libedataserver.h>

#include "e-html-editor.h"

#include "e-activity-bar.h"
#include "e-alert-bar.h"
#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-html-editor-private.h"
#include "e-content-editor.h"
#include "e-misc-utils.h"
#include "e-simple-async-result.h"

#define E_HTML_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR, EHTMLEditorPrivate))

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
	PROP_FILENAME
};

enum {
	UPDATE_ACTIONS,
	SPELL_LANGUAGES_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Forward Declarations */
static void	e_html_editor_alert_sink_init
					(EAlertSinkInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EHTMLEditor,
	e_html_editor,
	GTK_TYPE_GRID,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK,
		e_html_editor_alert_sink_init))

/* Action callback for context menu spelling suggestions.
 * XXX This should really be in e-html-editor-actions.c */
static void
action_context_spell_suggest_cb (GtkAction *action,
                                 EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	const gchar *word;

	word = g_object_get_data (G_OBJECT (action), "word");
	g_return_if_fail (word != NULL);

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_replace_caret_word (cnt_editor, word);
}

static void
html_editor_inline_spelling_suggestions (EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	gchar **suggestions;
	const gchar *path;
	gchar *word;
	guint count = 0;
	guint length;
	guint merge_id;
	guint threshold;
	gint ii;

	cnt_editor = e_html_editor_get_content_editor (editor);
	word = e_content_editor_get_caret_word (cnt_editor);
	if (word == NULL || *word == '\0')
		return;

	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	suggestions = e_spell_checker_get_guesses_for_word (spell_checker, word);

	path = "/context-menu/context-spell-suggest/";
	manager = e_html_editor_get_ui_manager (editor);
	action_group = editor->priv->suggestion_actions;
	merge_id = editor->priv->spell_suggestions_merge_id;

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
		gchar *action_name;
		gchar *action_label;
		GtkAction *action;
		GtkWidget *child;
		GSList *proxies;

		/* Once we reach the threshold, put all subsequent
		 * spelling suggestions in a secondary menu. */
		if (count == threshold)
			path = "/context-menu/context-more-suggestions-menu/";

		/* Action name just needs to be unique. */
		action_name = g_strdup_printf ("suggest-%d", count++);

		action_label = g_markup_printf_escaped (
			"<b>%s</b>", suggestion);

		action = gtk_action_new (
			action_name, action_label, NULL, NULL);

		g_object_set_data_full (
			G_OBJECT (action), "word",
			g_strdup (suggestion), g_free);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_context_spell_suggest_cb), editor);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			manager, merge_id, path,
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		/* XXX GtkAction offers no support for Pango markup,
		 *     so we have to manually set "use-markup" on the
		 *     child of the proxy widget. */
		gtk_ui_manager_ensure_update (manager);
		proxies = gtk_action_get_proxies (action);
		child = gtk_bin_get_child (proxies->data);
		g_object_set (child, "use-markup", TRUE, NULL);

		g_free (action_name);
		g_free (action_label);
	}

	g_free (word);
	g_strfreev (suggestions);
	g_clear_object (&spell_checker);
}

/* Helper for html_editor_update_actions() */
static void
html_editor_spell_checkers_foreach (EHTMLEditor *editor,
                                    const gchar *language_code)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	ESpellDictionary *dictionary = NULL;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GList *list, *link;
	gchar *path;
	gchar *word;
	gint ii = 0;
	guint merge_id;

	cnt_editor = e_html_editor_get_content_editor (editor);
	word = e_content_editor_get_caret_word (cnt_editor);
	if (word == NULL || *word == '\0')
		return;

	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);

	dictionary = e_spell_checker_ref_dictionary (
		spell_checker, language_code);
	if (dictionary != NULL) {
		list = e_spell_dictionary_get_suggestions (
			dictionary, word, -1);
		g_object_unref (dictionary);
	} else {
		list = NULL;
	}

	manager = e_html_editor_get_ui_manager (editor);
	action_group = editor->priv->suggestion_actions;
	merge_id = editor->priv->spell_suggestions_merge_id;

	path = g_strdup_printf (
		"/context-menu/context-spell-suggest/"
		"context-spell-suggest-%s-menu", language_code);

	for (link = list; link != NULL; link = g_list_next (link), ii++) {
		gchar *suggestion = link->data;
		gchar *action_name;
		gchar *action_label;
		GtkAction *action;
		GtkWidget *child;
		GSList *proxies;

		/* Action name just needs to be unique. */
		action_name = g_strdup_printf (
			"suggest-%s-%d", language_code, ii);

		action_label = g_markup_printf_escaped (
			"%s", suggestion);

		action = gtk_action_new (
			action_name, action_label, NULL, NULL);

		g_object_set_data_full (
			G_OBJECT (action), "word",
			g_strdup (suggestion), g_free);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_context_spell_suggest_cb), editor);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			manager, merge_id, path,
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		/* XXX GtkAction offers no supports for Pango markup,
		 *     so we have to manually set "use-markup" on the
		 *     child of the proxy widget. */
		gtk_ui_manager_ensure_update (manager);
		proxies = gtk_action_get_proxies (action);
		if (proxies && proxies->data) {
			child = gtk_bin_get_child (proxies->data);
			g_object_set (child, "use-markup", TRUE, NULL);
		}

		g_free (action_name);
		g_free (action_label);
	}

	g_list_free_full (list, (GDestroyNotify) g_free);
	g_clear_object (&spell_checker);
	g_free (path);
	g_free (word);
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

	gtk_action_set_visible (ACTION (CONTEXT_SPELL_ADD), count == 1);
	gtk_action_set_visible (ACTION (CONTEXT_SPELL_ADD_MENU), count > 1);
	gtk_action_set_visible (ACTION (CONTEXT_SPELL_IGNORE), count > 0);

	gtk_action_set_sensitive (ACTION (SPELL_CHECK), count > 0);
	gtk_action_set_sensitive (ACTION (LANGUAGE_MENU), e_spell_checker_count_available_dicts (spell_checker) > 0);

	g_clear_object (&spell_checker);
}

static void
action_set_visible_and_sensitive (GtkAction *action,
                                  gboolean value)
{
	gtk_action_set_visible (action, value);
	gtk_action_set_sensitive (action, value);
}

static void
html_editor_update_actions (EHTMLEditor *editor,
                            EContentEditorNodeFlags flags)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GList *list;
	gchar **languages = NULL;
	guint ii, n_languages;
	gboolean visible;
	guint merge_id;

	cnt_editor = e_html_editor_get_content_editor (editor);

	if (camel_debug ("webkit:editor"))
		printf ("%s: flags:%d(%x)\n", G_STRFUNC, flags, flags);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_IMAGE);
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_IMAGE), visible);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_ANCHOR);
	if (visible)
		action_set_visible_and_sensitive (ACTION (CONTEXT_INSERT_LINK), visible);
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_LINK), visible);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_H_RULE);
	action_set_visible_and_sensitive (ACTION (CONTEXT_PROPERTIES_RULE), visible);

	visible = (flags & E_CONTENT_EDITOR_NODE_IS_TEXT);
	/* Only display the text properties dialog when some text is selected. */
	action_set_visible_and_sensitive (
		ACTION (CONTEXT_PROPERTIES_TEXT),
		visible && !(flags & E_CONTENT_EDITOR_NODE_IS_TEXT_COLLAPSED));

	visible =
		gtk_action_get_visible (ACTION (CONTEXT_PROPERTIES_IMAGE)) ||
		gtk_action_get_visible (ACTION (CONTEXT_PROPERTIES_LINK)) ||
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

	manager = e_html_editor_get_ui_manager (editor);
	action_group = editor->priv->suggestion_actions;

	/* Remove the old content from the context menu. */
	merge_id = editor->priv->spell_suggestions_merge_id;
	if (merge_id > 0) {
		gtk_ui_manager_remove_ui (manager, merge_id);
		editor->priv->spell_suggestions_merge_id = 0;
	}

	/* Clear the action group for spelling suggestions. */
	list = gtk_action_group_list_actions (action_group);
	while (list != NULL) {
		GtkAction *action = list->data;

		gtk_action_group_remove_action (action_group, action);
		list = g_list_delete_link (list, list);
	}

	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	languages = e_spell_checker_list_active_languages (
		spell_checker, &n_languages);

	/* Decide if we should show spell checking items. */
	visible = FALSE;
	if (n_languages > 0) {
		gchar *word = e_content_editor_get_caret_word (cnt_editor);
		if (word && *word) {
			visible = !e_spell_checker_check_word (spell_checker, word, -1);
		} else {
			visible = FALSE;
		}
		g_free (word);
	}

	action_group = editor->priv->spell_check_actions;
	gtk_action_group_set_visible (action_group, visible);

	g_clear_object (&spell_checker);

	/* Exit early if spell checking items are invisible. */
	if (!visible) {
		g_strfreev (languages);
		return;
	}

	merge_id = gtk_ui_manager_new_merge_id (manager);
	editor->priv->spell_suggestions_merge_id = merge_id;

	/* Handle a single active language as a special case. */
	if (n_languages == 1) {
		html_editor_inline_spelling_suggestions (editor);
		g_strfreev (languages);

		e_html_editor_update_spell_actions (editor);
		return;
	}

	/* Add actions and context menu content for active languages. */
	for (ii = 0; ii < n_languages; ii++)
		html_editor_spell_checkers_foreach (editor, languages[ii]);

	g_strfreev (languages);

	e_html_editor_update_spell_actions (editor);
}

static void
html_editor_spell_languages_changed (EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	gchar **languages;

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);

	languages = e_spell_checker_list_active_languages (spell_checker, NULL);

	/* Set the languages for webview to highlight misspelled words */
	e_content_editor_set_spell_checking_languages (cnt_editor, (const gchar **) languages);

	if (editor->priv->spell_check_dialog != NULL)
		e_html_editor_spell_check_dialog_update_dictionaries (
			E_HTML_EDITOR_SPELL_CHECK_DIALOG (
			editor->priv->spell_check_dialog));

	e_content_editor_set_spell_check_enabled (cnt_editor, languages && *languages);

	g_clear_object (&spell_checker);
	g_strfreev (languages);
}

static gboolean
html_editor_context_menu_requested_cb (EContentEditor *cnt_editor,
                                       EContentEditorNodeFlags flags,
                                       GdkEvent *event,
                                       EHTMLEditor *editor)
{
	GtkWidget *menu;

	/* COUNT FLAGS */
	menu = e_html_editor_get_managed_widget (editor, "/context-menu");

	g_signal_emit (editor, signals[UPDATE_ACTIONS], 0, flags);

	if (!gtk_menu_get_attach_widget (GTK_MENU (menu)))
		gtk_menu_attach_to_widget (GTK_MENU (menu),
					   GTK_WIDGET (editor),
					   NULL);

	if (event)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL,
			GTK_WIDGET (cnt_editor),
			((GdkEventButton*) event)->button,
			((GdkEventButton*) event)->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL,
			GTK_WIDGET (cnt_editor),
			0,
			gtk_get_current_event_time ());

	return TRUE;
}

static gchar *
html_editor_find_ui_file (const gchar *basename)
{
	gchar *filename;

	g_return_val_if_fail (basename != NULL, NULL);

	/* Support running directly from the source tree. */
	filename = g_build_filename (".", basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	/* XXX This is kinda broken. */
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

	/* If he now have a window, then install our accelators to it */
	top_level = gtk_widget_get_toplevel (widget);
	if (GTK_IS_WINDOW (top_level)) {
		gtk_window_add_accel_group (
			GTK_WINDOW (top_level),
			gtk_ui_manager_get_accel_group (editor->priv->manager));
	}
}

static void
html_editor_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			e_html_editor_set_filename (
				E_HTML_EDITOR (object),
				g_value_get_string (value));
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
		case PROP_FILENAME:
			g_value_set_string (
				value, e_html_editor_get_filename (
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
	GtkWidget *widget;
	GtkToolbar *toolbar;
	GtkToolItem *tool_item;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_html_editor_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	editor_actions_init (editor);
	priv->editor_layout_row = 2;

	/* Tweak the main-toolbar style. */
	widget = e_html_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	/* Construct the editing toolbars. */

	widget = e_html_editor_get_managed_widget (editor, "/edit-toolbar");
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 0, 1, 1);
	priv->edit_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_html_editor_get_managed_widget (editor, "/html-toolbar");
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 1, 1, 1);
	priv->html_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

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

	/* Construct the main editing area. */
	widget = GTK_WIDGET (e_html_editor_get_content_editor (editor));

	/* Pack editors which implement GtkScrollable in a scrolled window */
	if (GTK_IS_SCROLLABLE (widget)) {
		GtkWidget *scrolled_window;

		scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_widget_show (scrolled_window);

		gtk_grid_attach (GTK_GRID (editor), scrolled_window, 0, 4, 1, 1);

		gtk_container_add (GTK_CONTAINER (scrolled_window), widget);
	} else {
		gtk_grid_attach (GTK_GRID (editor), widget, 0, 4, 1, 1);
	}

	gtk_widget_show (widget);

	g_signal_connect (
		widget, "context-menu-requested",
		G_CALLBACK (html_editor_context_menu_requested_cb), editor);

	/* Add some combo boxes to the "edit" toolbar. */

	toolbar = GTK_TOOLBAR (priv->edit_toolbar);

	tool_item = gtk_tool_item_new ();
	widget = e_action_combo_box_new_with_action (
		GTK_RADIO_ACTION (ACTION (STYLE_NORMAL)));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Paragraph Style"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->style_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	tool_item = gtk_separator_tool_item_new ();
	gtk_toolbar_insert (toolbar, tool_item, 0);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	tool_item = gtk_tool_item_new ();
	widget = e_action_combo_box_new_with_action (
		GTK_RADIO_ACTION (ACTION (MODE_HTML)));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Editing Mode"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->mode_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	/* Add some combo boxes to the "html" toolbar. */

	toolbar = GTK_TOOLBAR (priv->html_toolbar);

	tool_item = gtk_tool_item_new ();
	widget = e_color_combo_new ();
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Font Color"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->color_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	tool_item = gtk_tool_item_new ();
	widget = e_action_combo_box_new_with_action (
		GTK_RADIO_ACTION (ACTION (SIZE_PLUS_ZERO)));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Font Size"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->size_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));
}

static void
html_editor_dispose (GObject *object)
{
	EHTMLEditorPrivate *priv;

	priv = E_HTML_EDITOR_GET_PRIVATE (object);

	g_clear_object (&priv->manager);
	g_clear_object (&priv->core_actions);
	g_clear_object (&priv->core_editor_actions);
	g_clear_object (&priv->html_actions);
	g_clear_object (&priv->context_actions);
	g_clear_object (&priv->html_context_actions);
	g_clear_object (&priv->language_actions);
	g_clear_object (&priv->spell_check_actions);
	g_clear_object (&priv->suggestion_actions);

	g_clear_object (&priv->main_menu);
	g_clear_object (&priv->main_toolbar);
	g_clear_object (&priv->edit_toolbar);
	g_clear_object (&priv->html_toolbar);
	g_clear_object (&priv->activity_bar);
	g_clear_object (&priv->alert_bar);
	g_clear_object (&priv->edit_area);

	g_clear_object (&priv->color_combo_box);
	g_clear_object (&priv->mode_combo_box);
	g_clear_object (&priv->size_combo_box);
	g_clear_object (&priv->style_combo_box);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_parent_class)->dispose (object);
}

static void
html_editor_finalize (GObject *object)
{
	EHTMLEditor *editor = E_HTML_EDITOR (object);

	g_hash_table_destroy (editor->priv->content_editors);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_html_editor_parent_class)->finalize (object);
}

static void
html_editor_submit_alert (EAlertSink *alert_sink,
                          EAlert *alert)
{
	EHTMLEditorPrivate *priv;
	EAlertBar *alert_bar;
	GtkWidget *toplevel;
	GtkWidget *widget;
	GtkWindow *parent;

	priv = E_HTML_EDITOR_GET_PRIVATE (alert_sink);

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
		case GTK_MESSAGE_WARNING:
		case GTK_MESSAGE_ERROR:
			alert_bar = E_ALERT_BAR (priv->alert_bar);
			e_alert_bar_add_alert (alert_bar, alert);
			break;

		default:
			widget = GTK_WIDGET (alert_sink);
			toplevel = gtk_widget_get_toplevel (widget);
			if (GTK_IS_WINDOW (toplevel))
				parent = GTK_WINDOW (toplevel);
			else
				parent = NULL;
			widget = e_alert_dialog_new (parent, alert);
			gtk_dialog_run (GTK_DIALOG (widget));
			gtk_widget_destroy (widget);
	}
}

static void
e_html_editor_class_init (EHTMLEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorPrivate));

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
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EHTMLEditorClass, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__UINT,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);

	signals[SPELL_LANGUAGES_CHANGED] = g_signal_new (
		"spell-languages-changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EHTMLEditorClass, spell_languages_changed),
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
e_html_editor_init (EHTMLEditor *editor)
{
	EHTMLEditorPrivate *priv;
	gchar *filename;
	GError *error = NULL;

	editor->priv = E_HTML_EDITOR_GET_PRIVATE (editor);

	priv = editor->priv;

	priv->manager = gtk_ui_manager_new ();
	priv->core_actions = gtk_action_group_new ("core");
	priv->core_editor_actions = gtk_action_group_new ("core-editor");
	priv->html_actions = gtk_action_group_new ("html");
	priv->context_actions = gtk_action_group_new ("core-context");
	priv->html_context_actions = gtk_action_group_new ("html-context");
	priv->language_actions = gtk_action_group_new ("language");
	priv->spell_check_actions = gtk_action_group_new ("spell-check");
	priv->suggestion_actions = gtk_action_group_new ("suggestion");
	priv->content_editors = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);

	filename = html_editor_find_ui_file ("e-html-editor-manager.ui");
	if (!gtk_ui_manager_add_ui_from_file (priv->manager, filename, &error)) {
		g_critical ("Couldn't load builder file: %s\n", error->message);
		g_clear_error (&error);
	}
	g_free (filename);
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

	e_binding_bind_property (
		html_editor->priv->color_combo_box, "current-color",
		content_editor, "font-color",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	e_binding_bind_property (
		content_editor, "editable",
		html_editor->priv->color_combo_box, "sensitive",
		G_BINDING_SYNC_CREATE);
	editor_actions_bind (html_editor);

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
		GSettings *settings;
		gchar *name;

		if (!g_hash_table_size (editor->priv->content_editors))
			return NULL;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		name = g_settings_get_string (settings, "composer-editor");
		g_clear_object (&settings);

		if (name)
			editor->priv->use_content_editor = g_hash_table_lookup (editor->priv->content_editors, name);

		g_free (name);

		if (!editor->priv->use_content_editor)
			editor->priv->use_content_editor = g_hash_table_lookup (editor->priv->content_editors, DEFAULT_CONTENT_EDITOR_NAME);

		if (!editor->priv->use_content_editor) {
			GHashTableIter iter;
			gpointer key, value;

			g_hash_table_iter_init (&iter, editor->priv->content_editors);
			if (g_hash_table_iter_next (&iter, &key, &value)) {
				editor->priv->use_content_editor = value;
			}
		}

		if (editor->priv->use_content_editor)
			e_content_editor_setup_editor (editor->priv->use_content_editor, editor);
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
	if (g_hash_table_iter_next (&iter, &key, &value)) {
		if (value == cnt_editor)
			return key;
	}

	return NULL;
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

/**
 * e_html_editor_get_ui_manager:
 * @editor: an #EHTMLEditor
 *
 * Returns #GtkUIManager that manages all the actions in the @editor.
 */
GtkUIManager *
e_html_editor_get_ui_manager (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	return editor->priv->manager;
}

/**
 * e_html_editor_get_action:
 * @editor: an #EHTMLEditor
 * @action_name: name of action to lookup and return
 *
 * Returns: A #GtkAction matching @action_name or @NULL if no such action exists.
 */
GtkAction *
e_html_editor_get_action (EHTMLEditor *editor,
                          const gchar *action_name)
{
	GtkUIManager *manager;
	GtkAction *action = NULL;
	GList *list;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	manager = e_html_editor_get_ui_manager (editor);
	list = gtk_ui_manager_get_action_groups (manager);

	while (list != NULL && action == NULL) {
		GtkActionGroup *action_group = list->data;

		action = gtk_action_group_get_action (
			action_group, action_name);

		list = g_list_next (list);
	}

	g_return_val_if_fail (action != NULL, NULL);

	return action;
}

/**
 * e_html_editor_get_action_group:
 * @editor: an #EHTMLEditor
 * @group_name: name of action group to lookup and return
 *
 * Returns: A #GtkActionGroup matching @group_name or @NULL if not such action
 * group exists.
 */
GtkActionGroup *
e_html_editor_get_action_group (EHTMLEditor *editor,
                                const gchar *group_name)
{
	GtkUIManager *manager;
	GList *list;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	manager = e_html_editor_get_ui_manager (editor);
	list = gtk_ui_manager_get_action_groups (manager);

	while (list != NULL) {
		GtkActionGroup *action_group = list->data;
		const gchar *name;

		name = gtk_action_group_get_name (action_group);
		if (strcmp (name, group_name) == 0)
			return action_group;

		list = g_list_next (list);
	}

	return NULL;
}

GtkWidget *
e_html_editor_get_managed_widget (EHTMLEditor *editor,
                                  const gchar *widget_path)
{
	GtkUIManager *manager;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	manager = e_html_editor_get_ui_manager (editor);
	widget = gtk_ui_manager_get_widget (manager, widget_path);

	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

GtkWidget *
e_html_editor_get_style_combo_box (EHTMLEditor *editor)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), NULL);

	return editor->priv->style_combo_box;
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

/**
 * e_html_editor_save:
 * @editor: an #EHTMLEditor
 * @filename: file into which to save the content
 * @as_html: whether the content should be saved as HTML or plain text
 * @error:[out] a #GError
 *
 * Saves current content of the #EContentEditor into given file. When @as_html
 * is @FALSE, the content is first converted into plain text.
 *
 * Returns: @TRUE when content is succesfully saved, @FALSE otherwise.
 */
gboolean
e_html_editor_save (EHTMLEditor *editor,
                    const gchar *filename,
                    gboolean as_html,
                    GError **error)
{
	EContentEditor *cnt_editor;
	GFile *file;
	GFileOutputStream *stream;
	gchar *content;
	gsize written;

	file = g_file_new_for_path (filename);
	stream = g_file_replace (
		file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, error);
	if ((error && *error) || !stream)
		return FALSE;

	cnt_editor = e_html_editor_get_content_editor (editor);

	if (as_html)
		content = e_content_editor_get_content (
			cnt_editor,
			E_CONTENT_EDITOR_GET_TEXT_HTML |
			E_CONTENT_EDITOR_GET_PROCESSED,
			NULL, NULL);
	else
		content = e_content_editor_get_content (
			cnt_editor,
			E_CONTENT_EDITOR_GET_TEXT_PLAIN |
			E_CONTENT_EDITOR_GET_PROCESSED,
			NULL, NULL);

	if (!content || !*content) {
		g_free (content);
		g_set_error (
			error, G_IO_ERROR, G_IO_ERROR_FAILED,
			"Failed to obtain content of editor");
		return FALSE;
	}

	g_output_stream_write_all (
		G_OUTPUT_STREAM (stream), content, strlen (content),
		&written, NULL, error);

	g_free (content);
	g_object_unref (stream);
	g_object_unref (file);

	return TRUE;
}
