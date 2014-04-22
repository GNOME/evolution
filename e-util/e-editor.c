/*
 * e-editor.c
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

#include <config.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <enchant/enchant.h>

#include "e-editor.h"

#include "e-activity-bar.h"
#include "e-alert-bar.h"
#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-editor-private.h"
#include "e-editor-utils.h"
#include "e-editor-selection.h"

#define E_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR, EEditorPrivate))

/**
 * EEditor:
 *
 * #EEditor provides GUI for manipulating with properties of #EEditorWidget and
 * its #EEditorSelection - i.e. toolbars and actions.
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
	PROP_BUSY,
	PROP_FILENAME
};

enum {
	UPDATE_ACTIONS,
	SPELL_LANGUAGES_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Forward Declarations */
static void	e_editor_alert_sink_init
					(EAlertSinkInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EEditor,
	e_editor,
	GTK_TYPE_GRID,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK,
		e_editor_alert_sink_init))

/* Action callback for context menu spelling suggestions.
 * XXX This should really be in e-editor-actions.c */
static void
action_context_spell_suggest_cb (GtkAction *action,
                                 EEditor *editor)
{
	EEditorWidget *widget;
	EEditorSelection *selection;
	const gchar *word;

	word = g_object_get_data (G_OBJECT (action), "word");
	g_return_if_fail (word != NULL);

	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);

	e_editor_selection_replace_caret_word (selection, word);
}

static void
editor_inline_spelling_suggestions (EEditor *editor)
{
	EEditorWidget *widget;
	EEditorSelection *selection;
	WebKitSpellChecker *checker;
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

	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);
	checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());

	word = e_editor_selection_get_caret_word (selection);
	if (word == NULL || *word == '\0')
		return;

	suggestions = webkit_spell_checker_get_guesses_for_word (checker, word, NULL);

	path = "/context-menu/context-spell-suggest/";
	manager = e_editor_get_ui_manager (editor);
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

	ii = 0;
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
			action, "activate", G_CALLBACK (
			action_context_spell_suggest_cb), editor);

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
}

/* Helper for editor_update_actions() */
static void
editor_spell_checkers_foreach (EEditor *editor,
                               const gchar *language_code)
{
	EEditorWidget *editor_widget;
	EEditorSelection *selection;
	ESpellChecker *spell_checker;
	ESpellDictionary *dictionary;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GList *list, *link;
	gchar *path;
	gchar *word;
	gint ii = 0;
	guint merge_id;

	editor_widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (editor_widget);
	spell_checker = e_editor_widget_get_spell_checker (editor_widget);

	word = e_editor_selection_get_caret_word (selection);
	if (word == NULL || *word == '\0')
		return;

	dictionary = e_spell_checker_ref_dictionary (
		spell_checker, language_code);
	if (dictionary != NULL) {
		list = e_spell_dictionary_get_suggestions (
			dictionary, word, -1);
		g_object_unref (dictionary);
	} else {
		list = NULL;
	}

	manager = e_editor_get_ui_manager (editor);
	action_group = editor->priv->suggestion_actions;
	merge_id = editor->priv->spell_suggestions_merge_id;

	path = g_strdup_printf (
		"/context-menu/context-spell-suggest/"
		"context-spell-suggest-%s-menu", language_code);

	for (link = list; link != NULL; link = g_list_next (link)) {
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
			"<b>%s</b>", suggestion);

		action = gtk_action_new (
			action_name, action_label, NULL, NULL);

		g_object_set_data_full (
			G_OBJECT (action), "word",
			g_strdup (suggestion), g_free);

		g_signal_connect (
			action, "activate", G_CALLBACK (
			action_context_spell_suggest_cb), editor);

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

	g_free (path);
	g_free (word);
}

static void
editor_update_actions (EEditor *editor,
                       GdkEventButton *event)
{
	WebKitWebView *webview;
	WebKitSpellChecker *checker;
	WebKitHitTestResult *hit_test;
	WebKitHitTestResultContext context;
	WebKitDOMNode *node;
	EEditorSelection *selection;
	EEditorWidget *editor_widget;
	ESpellChecker *spell_checker;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GList *list;
	gchar **languages;
	guint ii, n_languages;
	gboolean visible;
	guint merge_id;
	gint loc, len;

	editor_widget = e_editor_get_editor_widget (editor);
	spell_checker = e_editor_widget_get_spell_checker (editor_widget);

	webview = WEBKIT_WEB_VIEW (editor_widget);
	manager = e_editor_get_ui_manager (editor);

	editor->priv->image = NULL;
	editor->priv->table_cell = NULL;

	/* Update context menu item visibility. */
	hit_test = webkit_web_view_get_hit_test_result (webview, event);
	g_object_get (
		G_OBJECT (hit_test),
		"context", &context,
		"inner-node", &node, NULL);
	g_object_unref (hit_test);

	visible = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_IMAGE), visible);
	if (visible)
		editor->priv->image = node;

	visible = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_LINK), visible);

	visible = (WEBKIT_DOM_IS_HTMLHR_ELEMENT (node));
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_RULE), visible);

	visible = (webkit_dom_node_get_node_type (node) == 3);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_TEXT), visible);

	visible =
		gtk_action_get_visible (ACTION (CONTEXT_PROPERTIES_IMAGE)) ||
		gtk_action_get_visible (ACTION (CONTEXT_PROPERTIES_LINK)) ||
		gtk_action_get_visible (ACTION (CONTEXT_PROPERTIES_TEXT));
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_PARAGRAPH), visible);

	/* Set to visible if any of these are true:
	 *   - Selection is active and contains a link.
	 *   - Cursor is on a link.
	 *   - Cursor is on an image that has a URL or target.
	 */
	visible = (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node) ||
		(e_editor_dom_node_find_parent_element (node, "A") != NULL));
	gtk_action_set_visible (ACTION (CONTEXT_REMOVE_LINK), visible);

	visible = (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node) ||
		(e_editor_dom_node_find_parent_element (node, "TD") != NULL) ||
		(e_editor_dom_node_find_parent_element (node, "TH") != NULL));
	gtk_action_set_visible (ACTION (CONTEXT_DELETE_CELL), visible);
	gtk_action_set_visible (ACTION (CONTEXT_DELETE_COLUMN), visible);
	gtk_action_set_visible (ACTION (CONTEXT_DELETE_ROW), visible);
	gtk_action_set_visible (ACTION (CONTEXT_DELETE_TABLE), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_COLUMN_AFTER), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_COLUMN_BEFORE), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_ROW_ABOVE), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_ROW_BELOW), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_TABLE), visible);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_CELL), visible);
	if (visible)
		editor->priv->table_cell = node;

	/* Note the |= (cursor must be in a table cell). */
	visible |= (WEBKIT_DOM_IS_HTML_TABLE_ELEMENT (node) ||
		(e_editor_dom_node_find_parent_element (node, "TABLE") != NULL));
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_TABLE), visible);

	/********************** Spell Check Suggestions **********************/

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

	languages = e_spell_checker_list_active_languages (
		spell_checker, &n_languages);

	/* Decide if we should show spell checking items. */
	checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
	selection = e_editor_widget_get_selection (editor_widget);
	visible = FALSE;
	if ((n_languages > 0) && e_editor_selection_has_text (selection)) {
		gchar *word = e_editor_selection_get_caret_word (selection);
		if (word && *word) {
			webkit_spell_checker_check_spelling_of_string (
				checker, word, &loc, &len);
			visible = (loc > -1);
		} else {
			visible = FALSE;
		}
		g_free (word);
	}

	action_group = editor->priv->spell_check_actions;
	gtk_action_group_set_visible (action_group, visible);

	/* Exit early if spell checking items are invisible. */
	if (!visible) {
		g_strfreev (languages);
		return;
	}

	merge_id = gtk_ui_manager_new_merge_id (manager);
	editor->priv->spell_suggestions_merge_id = merge_id;

	/* Handle a single active language as a special case. */
	if (n_languages == 1) {
		editor_inline_spelling_suggestions (editor);
		g_strfreev (languages);
		return;
	}

	/* Add actions and context menu content for active languages. */
	for (ii = 0; ii < n_languages; ii++)
		editor_spell_checkers_foreach (editor, languages[ii]);

	g_strfreev (languages);
}

static void
editor_spell_languages_changed (EEditor *editor)
{
	EEditorWidget *editor_widget;
	ESpellChecker *spell_checker;
	WebKitWebSettings *settings;
	gchar *comma_separated;
	gchar **languages;

	editor_widget = e_editor_get_editor_widget (editor);
	spell_checker = e_editor_widget_get_spell_checker (editor_widget);

	languages = e_spell_checker_list_active_languages (spell_checker, NULL);
	comma_separated = g_strjoinv (",", languages);
	g_strfreev (languages);

	/* Set the languages for webview to highlight misspelled words */
	settings = webkit_web_view_get_settings (
		WEBKIT_WEB_VIEW (editor->priv->editor_widget));

	g_object_set (
		G_OBJECT (settings),
		"spell-checking-languages", comma_separated,
		NULL);

	if (editor->priv->spell_check_dialog != NULL)
		e_editor_spell_check_dialog_update_dictionaries (
			E_EDITOR_SPELL_CHECK_DIALOG (
			editor->priv->spell_check_dialog));

	if (*comma_separated)
		e_editor_widget_force_spell_check (editor->priv->editor_widget);
	else
		e_editor_widget_turn_spell_check_off (editor->priv->editor_widget);

	g_free (comma_separated);
}

static gboolean
editor_show_popup (EEditor *editor,
                   GdkEventButton *event,
                   gpointer user_data)
{
	GtkWidget *menu;

	menu = e_editor_get_managed_widget (editor, "/context-menu");

	g_signal_emit (editor, signals[UPDATE_ACTIONS], 0, event);

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL,
			user_data, event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL,
			user_data, 0, gtk_get_current_event_time ());

	return TRUE;
}

static gchar *
editor_find_ui_file (const gchar *basename)
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
editor_parent_changed (GtkWidget *widget,
                       GtkWidget *previous_parent)
{
	GtkWidget *top_level;
	EEditor *editor = E_EDITOR (widget);

	/* If he now have a window, then install our accelators to it */
	top_level = gtk_widget_get_toplevel (widget);
	if (GTK_IS_WINDOW (top_level)) {
		gtk_window_add_accel_group (
			GTK_WINDOW (top_level),
			gtk_ui_manager_get_accel_group (editor->priv->manager));
	}
}

static void
editor_notify_activity_cb (EActivityBar *activity_bar,
                           GParamSpec *pspec,
                           EEditor *editor)
{
	EEditorWidget *editor_widget;
	WebKitWebView *web_view;
	gboolean editable;
	gboolean busy;

	busy = (e_activity_bar_get_activity (activity_bar) != NULL);

	if (busy == editor->priv->busy)
		return;

	editor->priv->busy = busy;

	editor_widget = e_editor_get_editor_widget (editor);
	web_view = WEBKIT_WEB_VIEW (editor_widget);

	if (busy) {
		editable = webkit_web_view_get_editable (web_view);
		webkit_web_view_set_editable (web_view, FALSE);
		editor->priv->saved_editable = editable;
	} else {
		editable = editor->priv->saved_editable;
		webkit_web_view_set_editable (web_view, editable);
	}

	g_object_notify (G_OBJECT (editor), "busy");
}

static void
editor_set_property (GObject *object,
                     guint property_id,
                     const GValue *value,
                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			e_editor_set_filename (
				E_EDITOR (object),
				g_value_get_string (value));
			return;

	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_get_property (GObject *object,
                     guint property_id,
                     GValue *value,
                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BUSY:
			g_value_set_boolean (
				value, e_editor_is_busy (
				E_EDITOR (object)));
			return;

		case PROP_FILENAME:
			g_value_set_string (
				value, e_editor_get_filename (
				E_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_constructed (GObject *object)
{
	EEditor *editor = E_EDITOR (object);
	EEditorPrivate *priv = editor->priv;
	GtkIMMulticontext *im_context;

	GtkWidget *widget;
	GtkToolbar *toolbar;
	GtkToolItem *tool_item;

	/* Construct the editing toolbars. */

	widget = e_editor_get_managed_widget (editor, "/edit-toolbar");
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 0, 1, 1);
	priv->edit_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_editor_get_managed_widget (editor, "/html-toolbar");
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
	/* EActivityBar controls its own visibility. */

	g_signal_connect (
		widget, "notify::activity",
		G_CALLBACK (editor_notify_activity_cb), editor);

	/* Construct the alert bar for errors. */

	widget = e_alert_bar_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 3, 1, 1);
	priv->alert_bar = g_object_ref (widget);
	/* EAlertBar controls its own visibility. */

	/* Construct the main editing area. */

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 4, 1, 1);
	priv->scrolled_window = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = GTK_WIDGET (e_editor_get_editor_widget (editor));
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window), widget);
	gtk_widget_show (widget);
	g_signal_connect_swapped (
		widget, "popup-event",
		G_CALLBACK (editor_show_popup), editor);

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
	g_object_bind_property (
		priv->color_combo_box, "current-color",
		priv->selection, "font-color",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_object_bind_property (
		priv->editor_widget, "editable",
		priv->color_combo_box, "sensitive",
		G_BINDING_SYNC_CREATE);

	tool_item = gtk_tool_item_new ();
	widget = e_action_combo_box_new_with_action (
		GTK_RADIO_ACTION (ACTION (SIZE_PLUS_ZERO)));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Font Size"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->size_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	/* Add input methods to the context menu. */
	widget = e_editor_get_managed_widget (
		editor, "/context-menu/context-input-methods-menu");
	widget = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
	g_object_get (
		G_OBJECT (priv->editor_widget), "im-context", &im_context, NULL);
	gtk_im_multicontext_append_menuitems (
		GTK_IM_MULTICONTEXT (im_context),
		GTK_MENU_SHELL (widget));
}

static void
editor_dispose (GObject *object)
{
	EEditorPrivate *priv;

	priv = E_EDITOR_GET_PRIVATE (object);

	g_clear_object (&priv->manager);
	g_clear_object (&priv->core_actions);
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
	g_clear_object (&priv->scrolled_window);

	g_clear_object (&priv->editor_widget);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_editor_parent_class)->dispose (object);
}

static void
editor_submit_alert (EAlertSink *alert_sink,
                     EAlert *alert)
{
	EEditorPrivate *priv;
	EAlertBar *alert_bar;
	GtkWidget *toplevel;
	GtkWidget *widget;
	GtkWindow *parent;

	priv = E_EDITOR_GET_PRIVATE (alert_sink);

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
e_editor_class_init (EEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = editor_set_property;
	object_class->get_property = editor_get_property;
	object_class->constructed = editor_constructed;
	object_class->dispose = editor_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->parent_set = editor_parent_changed;

	class->update_actions = editor_update_actions;
	class->spell_languages_changed = editor_spell_languages_changed;

	g_object_class_install_property (
		object_class,
		PROP_BUSY,
		g_param_spec_boolean (
			"busy",
			"Busy",
			"Whether an activity is in progress",
			FALSE,
			G_PARAM_READABLE |
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

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EEditorClass, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[SPELL_LANGUAGES_CHANGED] = g_signal_new (
		"spell-languages-changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EEditorClass, spell_languages_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_editor_alert_sink_init (EAlertSinkInterface *interface)
{
	interface->submit_alert = editor_submit_alert;
}

static void
e_editor_init (EEditor *editor)
{
	EEditorPrivate *priv;
	GtkWidget *widget;
	gchar *filename;
	GError *error = NULL;

	editor->priv = E_EDITOR_GET_PRIVATE (editor);

	priv = editor->priv;

	priv->manager = gtk_ui_manager_new ();
	priv->core_actions = gtk_action_group_new ("core");
	priv->html_actions = gtk_action_group_new ("html");
	priv->context_actions = gtk_action_group_new ("core-context");
	priv->html_context_actions = gtk_action_group_new ("html-context");
	priv->language_actions = gtk_action_group_new ("language");
	priv->spell_check_actions = gtk_action_group_new ("spell-check");
	priv->suggestion_actions = gtk_action_group_new ("suggestion");
	priv->editor_widget = g_object_ref_sink (e_editor_widget_new ());
	priv->selection = e_editor_widget_get_selection (priv->editor_widget);

	filename = editor_find_ui_file ("e-editor-manager.ui");
	if (!gtk_ui_manager_add_ui_from_file (priv->manager, filename, &error)) {
		g_critical ("Couldn't load builder file: %s\n", error->message);
		g_clear_error (&error);
	}
	g_free (filename);

	editor_actions_init (editor);
	priv->editor_layout_row = 2;

	/* Tweak the main-toolbar style. */
	widget = e_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
}

/**
 * e_editor_new:
 *
 * Constructs a new #EEditor.
 *
 * Returns: A newly created widget. [transfer-full]
 */
GtkWidget *
e_editor_new (void)
{
	return g_object_new (E_TYPE_EDITOR, NULL);
}

/**
 * e_editor_is_busy:
 * @editor: an #EEditor
 *
 * Returns %TRUE only while an #EActivity is in progress.
 *
 * Returns: whether @editor is busy
 **/
gboolean
e_editor_is_busy (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), FALSE);

	return editor->priv->busy;
}

/**
 * e_editor_get_editor_widget:
 * @editor: an #EEditor
 *
 * Returns instance of #EEditorWidget used in the @editor.
 */
EEditorWidget *
e_editor_get_editor_widget (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

	return editor->priv->editor_widget;
}

/**
 * e_editor_get_ui_manager:
 * @editor: an #EEditor
 *
 * Returns #GtkUIManager that manages all the actions in the @editor.
 */
GtkUIManager *
e_editor_get_ui_manager (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

	return editor->priv->manager;
}

/**
 * e_editor_get_action:
 * @editor: an #EEditor
 * @action_name: name of action to lookup and return
 *
 * Returns: A #GtkAction matching @action_name or @NULL if no such action exists.
 */
GtkAction *
e_editor_get_action (EEditor *editor,
                     const gchar *action_name)
{
	GtkUIManager *manager;
	GtkAction *action = NULL;
	GList *list;

	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	manager = e_editor_get_ui_manager (editor);
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
 * e_editor_get_action_group:
 * @editor: an #EEditor
 * @group_name: name of action group to lookup and return
 *
 * Returns: A #GtkActionGroup matching @group_name or @NULL if not such action
 * group exists.
 */
GtkActionGroup *
e_editor_get_action_group (EEditor *editor,
                           const gchar *group_name)
{
	GtkUIManager *manager;
	GList *list;

	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	manager = e_editor_get_ui_manager (editor);
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
e_editor_get_managed_widget (EEditor *editor,
                             const gchar *widget_path)
{
	GtkUIManager *manager;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	manager = e_editor_get_ui_manager (editor);
	widget = gtk_ui_manager_get_widget (manager, widget_path);

	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

GtkWidget *
e_editor_get_style_combo_box (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

	return editor->priv->style_combo_box;
}

/**
 * e_editor_get_filename:
 * @editor: an #EEditor
 *
 * Returns path and name of file to which content of the editor should be saved.
 */
const gchar *
e_editor_get_filename (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

	return editor->priv->filename;
}

/**
 * e_editor_set_filename:
 * @editor: an #EEditor
 * @filename: Target file
 *
 * Sets file to which content of the editor should be saved (see
 * e_editor_save()).
 */
void
e_editor_set_filename (EEditor *editor,
                       const gchar *filename)
{
	g_return_if_fail (E_IS_EDITOR (editor));

	if (g_strcmp0 (editor->priv->filename, filename) == 0)
		return;

	g_free (editor->priv->filename);
	editor->priv->filename = g_strdup (filename);

	g_object_notify (G_OBJECT (editor), "filename");
}

/**
 * e_editor_new_activity:
 * @editor: an #EEditor
 *
 * Creates and configures a new #EActivity so its progress is shown in
 * the @editor.  The #EActivity comes pre-loaded with a #CamelOperation.
 *
 * Returns: a new #EActivity for use with @editor
 **/
EActivity *
e_editor_new_activity (EEditor *editor)
{
	EActivity *activity;
	EActivityBar *activity_bar;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

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
 * e_editor_pack_above:
 * @editor: an #EEditor
 * @child: a #GtkWidget
 *
 * Inserts @child right between the toolbars and the editor widget itself.
 */
void
e_editor_pack_above (EEditor *editor,
                     GtkWidget *child)
{
	g_return_if_fail (E_IS_EDITOR (editor));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_grid_insert_row (GTK_GRID (editor), editor->priv->editor_layout_row);
	gtk_grid_attach (GTK_GRID (editor), child, 0, editor->priv->editor_layout_row, 1, 1);
	editor->priv->editor_layout_row++;
}

/**
 * e_editor_save:
 * @editor: an #EEditor
 * @filename: file into which to save the content
 * @as_html: whether the content should be saved as HTML or plain text
 * @error:[out] a #GError
 *
 * Saves current content of the #EEditorWidget into given file. When @as_html
 * is @FALSE, the content is first converted into plain text.
 *
 * Returns: @TRUE when content is succesfully saved, @FALSE otherwise.
 */
gboolean
e_editor_save (EEditor *editor,
               const gchar *filename,
               gboolean as_html,
               GError **error)
{
	GFile *file;
	GFileOutputStream *stream;
	gchar *content;
	gsize written;

	file = g_file_new_for_path (filename);
	stream = g_file_replace (
		file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, error);
	if ((error && *error) || !stream)
		return FALSE;

	if (as_html)
		content = e_editor_widget_get_text_html (
			E_EDITOR_WIDGET (editor));
	else
		content = e_editor_widget_get_text_plain (
			E_EDITOR_WIDGET (editor));

	if (!content || !*content) {
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

