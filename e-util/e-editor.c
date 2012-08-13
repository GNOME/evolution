/* e-editor.c
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

#include "e-editor.h"
#include "e-editor-private.h"
#include "e-editor-utils.h"

#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (
	EEditor,
	e_editor,
	GTK_TYPE_BOX);

enum {
	PROP_0,
	PROP_FILENAME
};

enum {
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
editor_update_actions (EEditor *editor,
		       GdkEventButton *event)
{
	WebKitWebView *webview;
	WebKitHitTestResult *hit_test;
	WebKitHitTestResultContext context;
	WebKitDOMNode *node;
	EEditorWidget *widget;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GList *list;
	gboolean visible;
	guint merge_id;

	widget = e_editor_get_editor_widget (editor);
	webview = WEBKIT_WEB_VIEW (widget);
	manager = e_editor_get_ui_manager (editor);

	editor->priv->image = NULL;

	/* Update context menu item visibility. */
	hit_test = webkit_web_view_get_hit_test_result (webview, event);
	g_object_get (
		G_OBJECT (hit_test),
		"context", &context,
	        "inner-node", &node, NULL);

	visible = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_IMAGE), visible);
	if (visible) {
		editor->priv->image = node;
	}

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
	visible = (e_editor_dom_node_get_parent_element (
			node, WEBKIT_TYPE_DOM_HTML_ANCHOR_ELEMENT) != NULL);
	gtk_action_set_visible (ACTION (CONTEXT_REMOVE_LINK), visible);


	visible = (e_editor_dom_node_get_parent_element (
			node, WEBKIT_TYPE_DOM_HTML_TABLE_CELL_ELEMENT) != NULL);
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

	/* Note the |= (cursor must be in a table cell). */
	visible |= (e_editor_dom_node_get_parent_element (
			node, WEBKIT_TYPE_DOM_HTML_TABLE_ELEMENT) != NULL);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_TABLE), visible);

	/********************** Spell Check Suggestions **********************/

	/* FIXME WEBKIT No spellcheching for now
	object = html->engine->cursor->object;
	action_group = editor->priv->suggestion_actions;

	// Remove the old content from the context menu.
	merge_id = editor->priv->spell_suggestions_merge_id;
	if (merge_id > 0) {
		gtk_ui_manager_remove_ui (manager, merge_id);
		editor->priv->spell_suggestions_merge_id = 0;
	}

	// Clear the action group for spelling suggestions.
	list = gtk_action_group_list_actions (action_group);
	while (list != NULL) {
		GtkAction *action = list->data;

		gtk_action_group_remove_action (action_group, action);
		list = g_list_delete_link (list, list);
	}

	// Decide if we should show spell checking items.
	visible =
		!html_engine_is_selection_active (html->engine) &&
		object != NULL && html_object_is_text (object) &&
		!html_engine_spell_word_is_valid (html->engine);
	action_group = editor->priv->spell_check_actions;
	gtk_action_group_set_visible (action_group, visible);

	// Exit early if spell checking items are invisible.
	if (!visible)
		return;

	list = editor->priv->active_spell_checkers;
	merge_id = gtk_ui_manager_new_merge_id (manager);
	editor->priv->spell_suggestions_merge_id = merge_id;

	// Handle a single active language as a special case.
	if (g_list_length (list) == 1) {
		editor_inline_spelling_suggestions (editor, list->data);
		return;
	}

	// Add actions and context menu content for active languages
	g_list_foreach (list, (GFunc) editor_spell_checkers_foreach, editor);
	*/
}

static gboolean
editor_show_popup (EEditor *editor,
		   GdkEventButton *event,
		   gpointer user_data)
{
	GtkWidget *menu;

	menu = e_editor_get_managed_widget (editor, "/context-menu");

	g_signal_emit(editor, signals[UPDATE_ACTIONS], 0, event);

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
editor_set_property (GObject *object,
		     guint property_id,
		     const GValue *value,
		     GParamSpec *pspec)
{
	switch (property_id) {

		case PROP_FILENAME:
			e_editor_set_filename (E_EDITOR (object),
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

		case PROP_FILENAME:
			g_value_set_string (
				value, e_editor_get_filename(
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

	GtkWidget *widget;
	GtkToolbar *toolbar;
	GtkToolItem *tool_item;

	/* Construct main window widgets. */

	widget = e_editor_get_managed_widget (editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (editor), widget, FALSE, FALSE, 0);
	priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (editor), widget, FALSE, FALSE, 0);
	priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	widget = e_editor_get_managed_widget (editor, "/edit-toolbar");
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_box_pack_start (GTK_BOX (editor), widget, FALSE, FALSE, 0);
	priv->edit_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_editor_get_managed_widget (editor, "/html-toolbar");
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_box_pack_start (GTK_BOX (editor), widget, FALSE, FALSE, 0);
	priv->html_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (editor), widget, TRUE, TRUE, 0);
	priv->scrolled_window = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = GTK_WIDGET (e_editor_get_editor_widget (editor));
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window), widget);
	gtk_widget_show (widget);
	g_signal_connect_swapped (widget, "popup-event",
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

	tool_item = gtk_tool_item_new ();
	widget = e_action_combo_box_new_with_action (
		GTK_RADIO_ACTION (ACTION (SIZE_PLUS_ZERO)));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Font Size"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->size_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	/* Initialize painters (requires "edit_area"). */

	/* FIXME WEBKIT
	html = e_editor_widget_get_html (E_EDITOR_WIDGET (editor));
	gtk_widget_ensure_style (GTK_WIDGET (html));
	priv->html_painter = g_object_ref (html->engine->painter);
	priv->plain_painter = html_plain_painter_new (priv->edit_area, TRUE);
	*/

	/* Add input methods to the context menu. */

	/* FIXME WEBKIT
	widget = e_editor_get_managed_widget (
		editor, "/context-menu/context-input-methods-menu");
	widget = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
	gtk_im_multicontext_append_menuitems (
		GTK_IM_MULTICONTEXT (html->priv->im_context),
		GTK_MENU_SHELL (widget));
	*/

	/* Configure color stuff. */

	/* FIXME WEBKIT
	priv->palette = gtkhtml_color_palette_new ();
	priv->text_color = gtkhtml_color_state_new ();

	gtkhtml_color_state_set_default_label (
		priv->text_color, _("Automatic"));
	gtkhtml_color_state_set_palette (
		priv->text_color, priv->palette);
	*/

	/* Text color widgets share state. */

	/* FIXME WEBKIT
	widget = priv->color_combo_box;
	gtkhtml_color_combo_set_state (
		GTKHTML_COLOR_COMBO (widget), priv->text_color);

	widget = WIDGET (TEXT_PROPERTIES_COLOR_COMBO);
	gtkhtml_color_combo_set_state (
		GTKHTML_COLOR_COMBO (widget), priv->text_color);
	*/

	/* These color widgets share a custom color palette. */

	/* FIXME WEBKIT
	widget = WIDGET (CELL_PROPERTIES_COLOR_COMBO);
	gtkhtml_color_combo_set_palette (
		GTKHTML_COLOR_COMBO (widget), priv->palette);

	widget = WIDGET (PAGE_PROPERTIES_BACKGROUND_COLOR_COMBO);
	gtkhtml_color_combo_set_palette (
		GTKHTML_COLOR_COMBO (widget), priv->palette);

	widget = WIDGET (PAGE_PROPERTIES_LINK_COLOR_COMBO);
	gtkhtml_color_combo_set_palette (
		GTKHTML_COLOR_COMBO (widget), priv->palette);

	widget = WIDGET (TABLE_PROPERTIES_COLOR_COMBO);
	gtkhtml_color_combo_set_palette (
		GTKHTML_COLOR_COMBO (widget), priv->palette);
		*/
}

static void
editor_dispose (GObject *object)
{
	EEditor *editor = E_EDITOR (object);
	EEditorPrivate *priv = editor->priv;

	/* Disconnect signal handlers from the color
	 * state object since it may live on. */
	/* FIXME WEBKIT
	if (priv->text_color != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->text_color, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, editor);
	}
	*/

	g_clear_object (&priv->manager);
	g_clear_object (&priv->manager);
	g_clear_object (&priv->core_actions);
	g_clear_object (&priv->html_actions);
	g_clear_object (&priv->context_actions);
	g_clear_object (&priv->html_context_actions);
	g_clear_object (&priv->language_actions);
	g_clear_object (&priv->spell_check_actions);
	g_clear_object (&priv->suggestion_actions);
	g_clear_object (&priv->builder);

	/* FIXME WEBKIT
	g_hash_table_remove_all (priv->available_spell_checkers);

	g_list_foreach (
		priv->active_spell_checkers,
		(GFunc) g_object_unref, NULL);
	g_list_free (priv->active_spell_checkers);
	priv->active_spell_checkers = NULL;
	*/

	g_clear_object (&priv->main_menu);
	g_clear_object (&priv->main_toolbar);
	g_clear_object (&priv->edit_toolbar);
	g_clear_object (&priv->html_toolbar);
	g_clear_object (&priv->edit_area);

	g_clear_object (&priv->color_combo_box);
	g_clear_object (&priv->mode_combo_box);
	g_clear_object (&priv->size_combo_box);
	g_clear_object (&priv->style_combo_box);
	g_clear_object (&priv->scrolled_window);

	/* FIXME WEBKIT
	DISPOSE (priv->palette);
	DISPOSE (priv->text_color);
	*/
}

static void
e_editor_class_init (EEditorClass *klass)
{
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (EEditorPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = editor_set_property;
	object_class->get_property = editor_get_property;
	object_class->constructed = editor_constructed;
	object_class->dispose = editor_dispose;

	klass->update_actions = editor_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
		        NULL,
		        NULL,
		        NULL,
		        G_PARAM_READWRITE));

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EEditorClass, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
e_editor_init (EEditor *editor)
{
	EEditorPrivate *priv;
	GError *error;
	gchar *filename;

	editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		editor, E_TYPE_EDITOR, EEditorPrivate);

	priv = editor->priv;

	priv->manager = gtk_ui_manager_new ();
	priv->core_actions = gtk_action_group_new ("core");
	priv->html_actions = gtk_action_group_new ("html");
	priv->context_actions = gtk_action_group_new ("core-context");
	priv->html_context_actions = gtk_action_group_new ("html-context");
	priv->language_actions = gtk_action_group_new ("language");
	priv->spell_check_actions = gtk_action_group_new ("spell-check");
	priv->suggestion_actions = gtk_action_group_new ("suggestion");
	priv->editor_widget = e_editor_widget_new ();
	priv->selection = e_editor_widget_get_selection (priv->editor_widget);

	filename = editor_find_ui_file ("e-editor-manager.ui");

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_file (priv->manager, filename, &error)) {
		g_critical ("Couldn't load builder file: %s\n", error->message);
		g_clear_error (&error);
	}

	g_free (filename);

	filename = editor_find_ui_file ("e-editor-builder.ui");

	priv->builder = gtk_builder_new ();
	/* To keep translated strings in subclasses */
	gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
	error = NULL;
	if (!gtk_builder_add_from_file (priv->builder, filename, &error)) {
		g_critical ("Couldn't load builder file: %s\n", error->message);
		g_clear_error (&error);
	}

	g_free (filename);

	editor_actions_init (editor);
}

GtkWidget *
e_editor_new (void)
{
	return g_object_new (E_TYPE_EDITOR,
		"orientation", GTK_ORIENTATION_VERTICAL,
		NULL);
}

EEditorWidget *
e_editor_get_editor_widget (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

	return editor->priv->editor_widget;
}


GtkBuilder *
e_editor_get_builder (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

	return editor->priv->builder;
}

GtkUIManager *
e_editor_get_ui_manager (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

	return editor->priv->manager;
}

GtkAction *
e_editor_get_action (EEditor *editor,
		     const gchar *action_name)
{
	GtkUIManager *manager;
	GtkAction *action = NULL;
	GList *iter;

	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	manager = e_editor_get_ui_manager (editor);
	iter = gtk_ui_manager_get_action_groups (manager);

	while (iter != NULL && action == NULL) {
		GtkActionGroup *action_group = iter->data;

		action = gtk_action_group_get_action (
			action_group, action_name);
		iter = g_list_next (iter);
	}

	g_return_val_if_fail (action != NULL, NULL);

	return action;
}

GtkActionGroup *
e_editor_get_action_group (EEditor *editor,
			   const gchar *group_name)
{
	GtkUIManager *manager;
	GList *iter;

	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	manager = e_editor_get_ui_manager (editor);
	iter = gtk_ui_manager_get_action_groups (manager);

	while (iter != NULL) {
		GtkActionGroup *action_group = iter->data;
		const gchar *name;

		name = gtk_action_group_get_name (action_group);
		if (strcmp (name, group_name) == 0)
			return action_group;

		iter = g_list_next (iter);
	}

	return NULL;
}

GtkWidget *
e_editor_get_widget (EEditor *editor,
                           const gchar *widget_name)
{
	GtkBuilder *builder;
	GObject *object;

	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (widget_name != NULL, NULL);

	builder = e_editor_get_builder (editor);
	object = gtk_builder_get_object (builder, widget_name);
	g_return_val_if_fail (GTK_IS_WIDGET (object), NULL);

	return GTK_WIDGET (object);
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

const gchar *
e_editor_get_filename (EEditor *editor)
{
	g_return_val_if_fail (E_IS_EDITOR (editor), NULL);

	return editor->priv->filename;
}

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

	if (as_html) {
		content = e_editor_widget_get_text_html (
				E_EDITOR_WIDGET (editor));
	} else {
		content = e_editor_widget_get_text_plain (
				E_EDITOR_WIDGET (editor));
	}

	if (!content || !*content) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
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
