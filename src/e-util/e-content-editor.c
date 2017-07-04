/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib-object.h>

#include <libedataserver/libedataserver.h>

#include "e-html-editor.h"
#include "e-util-enumtypes.h"
#include "e-content-editor.h"

G_DEFINE_INTERFACE (EContentEditor, e_content_editor, GTK_TYPE_WIDGET);

enum {
	LOAD_FINISHED,
	PASTE_CLIPBOARD,
	PASTE_PRIMARY_CLIPBOARD,
	CONTEXT_MENU_REQUESTED,
	FIND_DONE,
	REPLACE_ALL_DONE,
	DROP_HANDLED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
e_content_editor_default_init (EContentEditorInterface *iface)
{
	/**
	 * EContentEditor:can-copy
	 *
	 * Determines whether it's possible to copy to clipboard. The action
	 * is usually disabled when there is no selection to copy.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"can-copy",
			"Can Copy",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:can-cut
	 *
	 * Determines whether it's possible to cut to clipboard. The action
	 * is usually disabled when there is no selection to cut.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"can-cut",
			"Can Cut",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:can-paste
	 *
	 * Determines whether it's possible to paste from clipboard. The action
	 * is usually disabled when there is no valid content in clipboard to
	 * paste.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"can-paste",
			"Can Paste",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:can-redo
	 *
	 * Determines whether it's possible to redo previous action. The action
	 * is usually disabled when there is no action to redo.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"can-redo",
			"Can Redo",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:can-undo
	 *
	 * Determines whether it's possible to undo last action. The action
	 * is usually disabled when there is no previous action to undo.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"can-undo",
			"Can Undo",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:editable
	 *
	 * Determines whether the editor is editable or read-only.
	 **/
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"editable",
			"Editable",
			"Wheter editor is editable",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:changed
	 *
	 * Determines whether document has been modified
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"changed",
			"Changed property",
			"Whether editor changed",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:html-mode
	 *
	 * Determines whether HTML or plain text mode is enabled.
	 **/
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"html-mode",
			"HTML Mode",
			"Edit HTML or plain text",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:alignment
	 *
	 * Holds alignment of current paragraph.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_enum (
			"alignment",
			NULL,
			NULL,
			E_TYPE_CONTENT_EDITOR_ALIGNMENT,
			E_CONTENT_EDITOR_ALIGNMENT_LEFT,
			G_PARAM_READWRITE));

	/**
	 * EContentEditor:background-color
	 *
	 * Holds background color of current selection or at current cursor
	 * position.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boxed (
			"background-color",
			NULL,
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE));

	/**
	 * EContentEditor:block-format
	 *
	 * Holds block format of current paragraph. See
	 * #EContentEditorBlockFormat for valid values.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_enum (
			"block-format",
			NULL,
			NULL,
			E_TYPE_CONTENT_EDITOR_BLOCK_FORMAT,
			E_CONTENT_EDITOR_BLOCK_FORMAT_NONE,
			G_PARAM_READWRITE));

	/**
	 * EContentEditor:bold
	 *
	 * Holds whether current selection or text at current cursor position
	 * is bold.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"bold",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:font-color
	 *
	 * Holds font color of current selection or at current cursor position.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boxed (
			"font-color",
			NULL,
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:font-name
	 *
	 * Holds name of font in current selection or at current cursor
	 * position.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_string (
			"font-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:font-size
	 *
	 * Holds point size of current selection or at current cursor position.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_int (
			"font-size",
			NULL,
			NULL,
			1,
			7,
			3,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:indented
	 *
	 * Holds whether current paragraph is indented. This does not include
	 * citations.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"indented",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:italic
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is italic.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"italic",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:monospaced
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is monospaced.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"monospaced",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:strikethrough
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is strikethrough.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"strikethrough",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:superscript
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is in superscript.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"superscript",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:subscript
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is in subscript.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"subscript",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:underline
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is underlined.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"underline",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:start-bottom
	 *
	 * Holds where the cursor should be positioned after body of
	 * a reply or forward is loaded.
	 *
	 * Since: 3.26
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_enum (
			"start-bottom",
			NULL,
			NULL,
			E_TYPE_THREE_STATE,
			E_THREE_STATE_INCONSISTENT,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:top-signature
	 *
	 * Holds where the signature should be positioned after body of
	 * a reply or forward is loaded.
	 *
	 * Since: 3.26
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_enum (
			"top-signature",
			NULL,
			NULL,
			E_TYPE_THREE_STATE,
			E_THREE_STATE_INCONSISTENT,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:spell-check-enabled
	 *
	 * Holds whether the spell checking is enabled.
	 */
	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"spell-check-enabled",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:spell-checker:
	 *
	 * The #ESpellChecker used for spell checking.
	 **/
	g_object_interface_install_property (
		iface,
		g_param_spec_object (
			"spell-checker",
			"Spell Checker",
			"The spell checker",
			E_TYPE_SPELL_CHECKER,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EContentEditor:paste-clipboard
	 *
	 * Emitted when user presses middle button on EContentEditor.
	 */
	signals[PASTE_CLIPBOARD] = g_signal_new (
		"paste-clipboard",
		E_TYPE_CONTENT_EDITOR,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContentEditorInterface, paste_clipboard),
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 0);

	/**
	 * EContentEditor:paste-primary-clipboard
	 *
	 * Emitted when user presses middle button on EWebKitContentEditor.
	 */
	signals[PASTE_PRIMARY_CLIPBOARD] = g_signal_new (
		"paste-primary-clipboard",
		E_TYPE_CONTENT_EDITOR,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContentEditorInterface, paste_primary_clipboard),
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 0);

	/**
	 * EContentEditor:load-finished
	 *
	 * Emitted when the content editor has finished loading.
	 */
	signals[LOAD_FINISHED] = g_signal_new (
		"load-finished",
		E_TYPE_CONTENT_EDITOR,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContentEditorInterface, load_finished),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 0);

	/**
	 * EContentEditor:context-menu-requested
	 *
	 * Emitted whenever a context menu is requested.
	 */
	signals[CONTEXT_MENU_REQUESTED] = g_signal_new (
		"context-menu-requested",
		E_TYPE_CONTENT_EDITOR,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContentEditorInterface, context_menu_requested),
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 2,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * EContentEditor::find-done
	 *
	 * Emitted when the call to e_content_editor_find() is done.
	 **/
	signals[FIND_DONE] = g_signal_new (
		"find-done",
		E_TYPE_CONTENT_EDITOR,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContentEditorInterface, find_done),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);

	/**
	 * EContentEditor::replace-all-done
	 *
	 * Emitted when the call to e_content_editor_replace_all() is done.
	 **/
	signals[REPLACE_ALL_DONE] = g_signal_new (
		"replace-all-done",
		E_TYPE_CONTENT_EDITOR,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContentEditorInterface, replace_all_done),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);

	/**
	 * EContentEditor:drop-handled
	 *
	 * Emitted when the content editor successfully handled the drop operation.
	 */
	signals[DROP_HANDLED] = g_signal_new (
		"drop-handled",
		E_TYPE_CONTENT_EDITOR,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContentEditorInterface, drop_handled),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 0);
}

ESpellChecker *
e_content_editor_ref_spell_checker (EContentEditor *editor)
{
	ESpellChecker *spell_checker = NULL;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	g_object_get (G_OBJECT (editor), "spell-checker", &spell_checker, NULL);

	return spell_checker;
}

gboolean
e_content_editor_can_cut (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "can-cut", &value, NULL);

	return value;
}

gboolean
e_content_editor_can_copy (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "can-copy", &value, NULL);

	return value;
}

gboolean
e_content_editor_can_paste (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "can-paste", &value, NULL);

	return value;
}

gboolean
e_content_editor_can_undo (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "can-undo", &value, NULL);

	return value;
}

gboolean
e_content_editor_can_redo (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "can-redo", &value, NULL);

	return value;
}

/**
 * e_content_editor_is_indented:
 * @editor: an #EContentEditor
 *
 * Returns whether the current paragraph is indented. This does not include
 * citations.
 *
 * Returns: %TRUE when current paragraph is indented, %FALSE otherwise.
 *
 * Since: 3.22
 **/
gboolean
e_content_editor_is_indented (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "indented", &value, NULL);

	return value;
}

gboolean
e_content_editor_get_spell_check_enabled (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "spell-check-enabled", &value, NULL);

	return value;
}

void
e_content_editor_set_spell_check_enabled (EContentEditor *editor,
					  gboolean enable)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "spell-check-enabled", enable, NULL);
}

gboolean
e_content_editor_is_editable (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "editable", &value, NULL);

	return value;
}

void
e_content_editor_set_editable (EContentEditor *editor,
			       gboolean editable)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "editable", editable, NULL);
}

gboolean
e_content_editor_get_changed (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "changed", &value, NULL);

	return value;
}

void
e_content_editor_set_changed (EContentEditor *editor,
			      gboolean changed)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "changed", changed, NULL);
}

gboolean
e_content_editor_get_html_mode (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "html-mode", &value, NULL);

	return value;
}

void
e_content_editor_set_html_mode (EContentEditor *editor,
				gboolean html_mode)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "html-mode", html_mode, NULL);
}

/**
 * e_content_editor_set_alignment:
 * @editor: an #EContentEditor
 * @value: an #EContentEditorAlignment value to apply
 *
 * Sets alignment of current paragraph to @value.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_alignment (EContentEditor *editor,
				EContentEditorAlignment value)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "alignment", value, NULL);
}

/**
 * e_content_editor_get_alignment:
 * @editor: #an EContentEditor
 *
 * Returns alignment of the current paragraph.
 *
 * Returns: #EContentEditorAlignment
 *
 * Since: 3.22
 **/
EContentEditorAlignment
e_content_editor_get_alignment (EContentEditor *editor)
{
	EContentEditorAlignment value = E_CONTENT_EDITOR_ALIGNMENT_LEFT;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), E_CONTENT_EDITOR_ALIGNMENT_LEFT);

	g_object_get (G_OBJECT (editor), "alignment", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_background_color:
 * @editor: an #EContentEditor
 * @value: a #GdkRGBA
 *
 * Sets the background color of the current selection or letter at the current cursor position to
 * a color defined by @value.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_background_color (EContentEditor *editor,
				       const GdkRGBA *value)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	g_object_set (G_OBJECT (editor), "background-color", value, NULL);
}

/**
 * e_content_editor_dup_background_color:
 * @editor: an #EContentEditor
 *
 * Returns the background color used in the current selection or at letter
 * at the current cursor position.
 *
 * Returns: (transfer-full): A newly allocated #GdkRGBA structure with
 *   the current background color. Free the returned value with gdk_rgba_free()
 *   when done with it.
 *
 * Since: 3.22
 **/
GdkRGBA *
e_content_editor_dup_background_color (EContentEditor *editor)
{
	GdkRGBA *value = NULL;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	g_object_get (G_OBJECT (editor), "background-color", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_font_color:
 * @editor: an #EContentEditor
 * @value: a #GdkRGBA
 *
 * Sets the font color of the current selection or letter at the current cursor position to
 * a color defined by @value.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_font_color (EContentEditor *editor,
				 const GdkRGBA *value)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	g_object_set (G_OBJECT (editor), "font-color", value, NULL);
}

/**
 * e_content_editor_dup_font_color:
 * @editor: an #EContentEditor
 *
 * Returns the font color used in the current selection or at letter
 * at the current cursor position.
 *
 * Returns: (transfer-full): A newly allocated #GdkRGBA structure with
 *   the current font color. Free the returned value with gdk_rgba_free()
 *   when done with it.
 *
 * Since: 3.22
 **/
GdkRGBA *
e_content_editor_dup_font_color (EContentEditor *editor)
{
	GdkRGBA *value = NULL;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	g_object_get (G_OBJECT (editor), "font-color", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_font_name:
 * @editor: an #EContentEditor
 * @value: a font name to apply
 *
 * Sets font name of current selection or of letter at current cursor position
 * to @value.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_font_name (EContentEditor *editor,
				const gchar *value)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	g_object_set (G_OBJECT (editor), "font-name", value, NULL);
}

/**
 * e_content_editor_dup_font_name:
 * @editor: an #EContentEditor
 *
 * Returns a name of the font used in the current selection or at letter
 * at the current cursor position.
 *
 * Returns: (transfer-full): A newly allocated string with the font name.
 *    Free it with g_free() when done with it.
 *
 * Since: 3.22
 **/
gchar *
e_content_editor_dup_font_name (EContentEditor *editor)
{
	gchar *value = NULL;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	g_object_get (G_OBJECT (editor), "font-name", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_font_size:
 * @editor: an #EContentEditor
 * @value: font size to apply
 *
 * Sets font size of current selection or of letter at current cursor position
 * to @value.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_font_size (EContentEditor *editor,
				gint value)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "font-size", value, NULL);
}

/**
 * e_content_editor_get_font_size:
 * @editor: an #EContentEditor
 *
 * Returns fotn size of the current selection or letter at the current
 * cursor position.
 *
 * Returns: Current font size.
 *
 * Since: 3.22
 **/
gint
e_content_editor_get_font_size (EContentEditor *editor)
{
	gint value = -1;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), -1);

	g_object_get (G_OBJECT (editor), "font-size", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_block_format:
 * @editor: an #EContentEditor
 * @value: an #EContentEditorBlockFormat value
 *
 * Changes block format of the current paragraph to @value.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_block_format (EContentEditor *editor,
				   EContentEditorBlockFormat value)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "block-format", value, NULL);
}

/**
 * e_content_editor_get_block_format:
 * @editor: an #EContentEditor
 *
 * Returns block format of the current paragraph.
 *
 * Returns: #EContentEditorBlockFormat
 *
 * Since: 3.22
 **/
EContentEditorBlockFormat
e_content_editor_get_block_format (EContentEditor *editor)
{
	EContentEditorBlockFormat value = E_CONTENT_EDITOR_BLOCK_FORMAT_NONE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), E_CONTENT_EDITOR_BLOCK_FORMAT_NONE);

	g_object_get (G_OBJECT (editor), "block-format", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_bold:
 * @editor: an #EContentEditor
 * @bold: %TRUE to enable bold, %FALSE to disable
 *
 * Changes bold formatting of current selection or letter at current
 * cursor position.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_bold (EContentEditor *editor,
			   gboolean bold)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "bold", bold, NULL);
}

/**
 * e_content_editor_is_bold:
 * @editor: an #EContentEditor
 *
 * Returns whether current selection or letter at current cursor position is bold.
 *
 * Returns: %TRUE when selection is bold, %FALSE otherwise.
 *
 * Since: 3.22
 **/
gboolean
e_content_editor_is_bold (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "bold", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_italic:
 * @editor: an #EContentEditor
 * @italic: %TRUE to enable italic, %FALSE to disable
 *
 * Changes italic formatting of current selection or letter at current
 * cursor position.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_italic (EContentEditor *editor,
			     gboolean italic)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "italic", italic, NULL);
}

/**
 * e_content_editor_is_italic:
 * @editor: an #EContentEditor
 *
 * Returns whether current selection or letter at current cursor position
 * is italic.
 *
 * Returns: %TRUE when selection is italic, %FALSE otherwise.
 *
 * Since: 3.22
 **/
gboolean
e_content_editor_is_italic (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "italic", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_monospaced:
 * @editor: an #EContentEditor
 * @monospaced: %TRUE to enable monospaced, %FALSE to disable
 *
 * Changes monospaced formatting of current selection or letter
 * at current cursor position.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_monospaced (EContentEditor *editor,
				 gboolean monospaced)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "monospaced", monospaced, NULL);
}

/**
 * e_content_editor_is_monospaced:
 * @editor: an #EContentEditor
 *
 * Returns whether current selection or letter at current cursor position
 * is monospaced.
 *
 * Returns: %TRUE when selection is monospaced, %FALSE otherwise.
 *
 * Since: 3.22
 **/
gboolean
e_content_editor_is_monospaced (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "monospaced", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_strikethrough:
 * @editor: an #EContentEditor
 * @strikethrough: %TRUE to enable strikethrough, %FALSE to disable
 *
 * Changes strike through formatting of current selection or letter at current
 * cursor position.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_strikethrough (EContentEditor *editor,
				    gboolean strikethrough)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "strikethrough", strikethrough, NULL);
}

/**
 * e_content_editor_is_strikethrough:
 * @editor: an #EContentEditor
 *
 * Returns whether current selection or letter at current cursor position
 * is striked through.
 *
 * Returns: %TRUE when selection is striked through, %FALSE otherwise.
 *
 * Since: 3.22
 **/
gboolean
e_content_editor_is_strikethrough (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "strikethrough", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_subscript:
 * @editor: an #EContentEditor
 * @subscript: %TRUE to enable subscript, %FALSE to disable
 *
 * Changes subscript of current selection or letter at current
 * cursor position.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_subscript (EContentEditor *editor,
				gboolean subscript)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "subscript", subscript, NULL);
}

/**
 * e_content_editor_is_subscript:
 * @editor: an #EContentEditor
 *
 * Returns whether current selection or letter at current cursor position
 * is in subscript.
 *
 * Returns: %TRUE when selection is in subscript, %FALSE otherwise.
 *
 * Since: 3.22
 **/
gboolean
e_content_editor_is_subscript (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "subscript", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_superscript:
 * @editor: an #EContentEditor
 * @superscript: %TRUE to enable superscript, %FALSE to disable
 *
 * Changes superscript of the current selection or letter at current
 * cursor position.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_superscript (EContentEditor *editor,
				  gboolean superscript)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "superscript", superscript, NULL);
}

/**
 * e_content_editor_is_superscript:
 * @editor: an #EContentEditor
 *
 * Returns whether current selection or letter at current cursor position
 * is in superscript.
 *
 * Returns: %TRUE when selection is in superscript, %FALSE otherwise.
 *
 * Since: 3.22
 **/
gboolean
e_content_editor_is_superscript (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "superscript", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_underline:
 * @editor: an #EContentEditor
 * @underline: %TRUE to enable underline, %FALSE to disable
 *
 * Changes underline formatting of current selection or letter
 * at current cursor position.
 *
 * Since: 3.22
 **/
void
e_content_editor_set_underline (EContentEditor *editor,
				gboolean underline)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "underline", underline, NULL);
}

/**
 * e_content_editor_is_underline:
 * @editor: an #EContentEditor
 *
 * Returns whether current selection or letter at current cursor position
 * is underlined.
 *
 * Returns: %TRUE when selection is underlined, %FALSE otherwise.
 *
 * Since: 3.22
 **/
gboolean
e_content_editor_is_underline (EContentEditor *editor)
{
	gboolean value = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "underline", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_start_bottom:
 * @editor: an #EContentEditor
 * @value: an #EThreeState value to set
 *
 * Changes start-bottom property, which is used to position
 * cursor after setting message body in replies and forwards.
 *
 * Since: 3.26
 **/
void
e_content_editor_set_start_bottom (EContentEditor *editor,
				   EThreeState value)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "start-bottom", value, NULL);
}

/**
 * e_content_editor_get_start_bottom:
 * @editor: an #EContentEditor
 *
 * Returns: the current value of start-bottom property.
 *
 * Since: 3.26
 **/
EThreeState
e_content_editor_get_start_bottom (EContentEditor *editor)
{
	EThreeState value = E_THREE_STATE_INCONSISTENT;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "start-bottom", &value, NULL);

	return value;
}

/**
 * e_content_editor_set_top_signature:
 * @editor: an #EContentEditor
 * @value: an #EThreeState value to set
 *
 * Changes top-signature property, which is used to position
 * signature after setting message body in replies and forwards.
 *
 * Since: 3.26
 **/
void
e_content_editor_set_top_signature (EContentEditor *editor,
				    EThreeState value)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_object_set (G_OBJECT (editor), "top-signature", value, NULL);
}

/**
 * e_content_editor_get_top_signature:
 * @editor: an #EContentEditor
 *
 * Returns: the current value of top-signature property.
 *
 * Since: 3.26
 **/
EThreeState
e_content_editor_get_top_signature (EContentEditor *editor)
{
	EThreeState value = E_THREE_STATE_INCONSISTENT;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_object_get (G_OBJECT (editor), "top-signature", &value, NULL);

	return value;
}

/**
 * e_content_editor_setup_editor:
 * @content_editor: an #EContentEditor
 * @callback: an #EContentEditorInitializedCallback function
 * @user_data: data to pass to @callback
 *
 * Initilizes the @content_editor. Once the initialization is done,
 * the @callback is called with the passed @user_data.
 *
 * Since: 3.22
 **/
void
e_content_editor_initialize (EContentEditor *content_editor,
			     EContentEditorInitializedCallback callback,
			     gpointer user_data)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (content_editor));
	g_return_if_fail (callback != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (content_editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->initialize != NULL);

	iface->initialize (content_editor, callback, user_data);
}

/**
 * e_content_editor_setup_editor:
 * @content_editor: an #EContentEditor
 * @html_editor: an #EHTMLEditor
 *
 * Called the first time the @content_editor is picked to be used within
 * the @html_editor. This is typically used to modify the UI
 * of the @html_editor. This method implementation is optional.
 *
 * Since: 3.22
 **/
void
e_content_editor_setup_editor (EContentEditor *content_editor,
			       EHTMLEditor *html_editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (content_editor));
	g_return_if_fail (E_IS_HTML_EDITOR (html_editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (content_editor);
	g_return_if_fail (iface != NULL);

	if (iface->setup_editor)
		iface->setup_editor (content_editor, html_editor);
}

void
e_content_editor_update_styles (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->update_styles != NULL);

	iface->update_styles (editor);
}

void
e_content_editor_insert_content (EContentEditor *editor,
                                 const gchar *content,
                                 EContentEditorInsertContentFlags flags)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (content != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->insert_content != NULL);

	iface->insert_content (editor, content, flags);
}

gchar *
e_content_editor_get_content (EContentEditor *editor,
                              EContentEditorGetContentFlags flags,
			      const gchar *inline_images_from_domain,
			      GSList **inline_images_parts /* newly created CamelMimePart * */)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);
	if ((flags & E_CONTENT_EDITOR_GET_INLINE_IMAGES)) {
		g_return_val_if_fail (inline_images_from_domain != NULL, NULL);
		g_return_val_if_fail (inline_images_parts != NULL, NULL);
	}

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->get_content != NULL, NULL);

	return iface->get_content (editor, flags, inline_images_from_domain, inline_images_parts);
}

void
e_content_editor_insert_image_from_mime_part (EContentEditor *editor,
                                              CamelMimePart *part)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (part != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->insert_image_from_mime_part != NULL);

	iface->insert_image_from_mime_part (editor, part);
}

/**
 * e_content_editor_insert_image:
 * @editor: an #EContentEditor
 * @uri: an URI of the source image
 *
 * Inserts image at current cursor position using @uri as source. When a
 * text range is selected, it will be replaced by the image.
 *
 * Since: 3.22
 **/
void
e_content_editor_insert_image (EContentEditor *editor,
                               const gchar *uri)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (uri != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->insert_image != NULL);

	iface->insert_image (editor, uri);
}

void
e_content_editor_insert_emoticon (EContentEditor *editor,
                                  EEmoticon *emoticon)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (emoticon != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->insert_emoticon != NULL);

	iface->insert_emoticon (editor, emoticon);
}

void
e_content_editor_move_caret_on_coordinates (EContentEditor *editor,
                                            gint x,
                                            gint y,
                                            gboolean cancel_if_not_collapsed)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (x > 0);
	g_return_if_fail (y > 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->move_caret_on_coordinates != NULL);

	iface->move_caret_on_coordinates (editor, x, y, cancel_if_not_collapsed);
}

void
e_content_editor_cut (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cut != NULL);

	iface->cut (editor);
}

void
e_content_editor_copy (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->copy != NULL);

	iface->copy (editor);
}

void
e_content_editor_paste (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->paste != NULL);

	iface->paste (editor);
}

void
e_content_editor_paste_primary (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->paste_primary != NULL);

	iface->paste_primary (editor);
}

void
e_content_editor_undo (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->undo != NULL);

	iface->undo (editor);
}

void
e_content_editor_redo (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->redo != NULL);

	iface->redo (editor);
}

void
e_content_editor_clear_undo_redo_history (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->clear_undo_redo_history != NULL);

	iface->clear_undo_redo_history (editor);
}

void
e_content_editor_set_spell_checking_languages (EContentEditor *editor,
                                               const gchar **languages)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->set_spell_checking_languages != NULL);

	iface->set_spell_checking_languages (editor, languages);
}

void
e_content_editor_select_all (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->select_all != NULL);

	iface->select_all (editor);
}

/**
 * e_content_editor_get_selected_text:
 * @editor: an #EContentEditor
 *
 * Returns currently selected string.
 *
 * Returns: (transfer-full): A newly allocated string with the content of current selection.
 *
 * Since: 3.22
 **/
gchar *
e_content_editor_get_selected_text (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->get_selected_text != NULL, NULL);

	return iface->get_selected_text (editor);
}

/**
 * e_content_editor_get_caret_word:
 * @editor: an #EContentEditor
 *
 * Returns word under cursor.
 *
 * Returns: (transfer-full): A newly allocated string with current caret word or %NULL
 * when there is no text under cursor or when selection is active.
 *
 * Since: 3.22
 **/
gchar *
e_content_editor_get_caret_word (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->get_caret_word != NULL, NULL);

	return iface->get_caret_word (editor);
}

/**
 * e_content_editor_replace_caret_word:
 * @editor: an #EContentEditor
 * @replacement: a string to replace current caret word with
 *
 * Replaces current word under cursor with @replacement.
 *
 * Since: 3.22
 **/
void
e_content_editor_replace_caret_word (EContentEditor *editor,
                                     const gchar *replacement)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (replacement != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->replace_caret_word != NULL);

	iface->replace_caret_word (editor, replacement);
}

void
e_content_editor_selection_indent (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->selection_indent != NULL);

	iface->selection_indent (editor);
}

void
e_content_editor_selection_unindent (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->selection_unindent != NULL);

	iface->selection_unindent (editor);
}

/**
 * e_content_editor_selection_create_link:
 * @editor: an #EContentEditor
 * @uri: destination of the new link
 *
 * Converts current selection into a link pointing to @url.
 *
 * Since: 3.22
 **/
void
e_content_editor_selection_create_link (EContentEditor *editor,
                                        const gchar *uri)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (uri != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->selection_create_link != NULL);

	iface->selection_create_link (editor, uri);
}

/**
 * e_content_editor_selection_unlink:
 * @editor: an #EContentEditor
 *
 * Removes any links (&lt;A&gt; elements) from current selection or at current
 * cursor position.
 *
 * Since: 3.22
 **/
void
e_content_editor_selection_unlink (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->selection_unlink != NULL);

	iface->selection_unlink (editor);
}

/**
 * e_content_editor_find:
 * @editor: an #EContentEditor
 * @flags: a bit-OR of #EContentEditorFindFlags flags
 * @text: a text to find
 *
 * Searches the content of the @editor for the occurrence of the @text.
 * The @flags modify the behaviour of the search. The found text,
 * if any, is supposed to be selected.
 *
 * Once the search is done, the "find-done" signal should be
 * emitted, by using e_content_editor_emit_find_done().
 *
 * Since: 3.22
 **/
void
e_content_editor_find (EContentEditor *editor,
		       guint32 flags,
		       const gchar *text)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->find != NULL);

	iface->find (editor, flags, text);
}

/**
 * e_content_editor_replace:
 * @editor: an #EContentEditor
 * @replacement: a string to replace current selection with
 *
 * Replaces currently selected text with @replacement.
 *
 * Since: 3.22
 **/
void
e_content_editor_replace (EContentEditor *editor,
			  const gchar *replacement)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (replacement != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->replace != NULL);

	iface->replace (editor, replacement);
}

/**
 * e_content_editor_replace_all:
 * @editor: an #EContentEditor
 * @flags: a bit-OR of #EContentEditorFindFlags flags
 * @find_text: a text to find
 * @replace_with: a text to replace the found text with
 *
 * Searches the content of the @editor for all the occurrences of
 * the @find_text and replaces them with the @replace_with.
 * The @flags modify the behaviour of the search.
 *
 * Once the replace is done, the "replace-all-done" signal should be
 * emitted, by using e_content_editor_emit_replace_all_done().
 *
 * Since: 3.22
 **/
void
e_content_editor_replace_all (EContentEditor *editor,
			      guint32 flags,
			      const gchar *find_text,
			      const gchar *replace_with)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (find_text != NULL);
	g_return_if_fail (replace_with != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->replace_all != NULL);

	iface->replace_all (editor, flags, find_text, replace_with);
}

/**
 * e_content_editor_selection_save:
 * @editor: an #EContentEditor
 *
 * Saves current cursor position or current selection range. The selection can
 * be later restored by calling e_content_editor_selection_restore().
 *
 * Note that calling e_content_editor_selection_save() overwrites previously saved
 * position.
 *
 * Note that this method inserts special markings into the HTML code that are
 * used to later restore the selection. It can happen that by deleting some
 * segments of the document some of the markings are deleted too. In that case
 * restoring the selection by e_content_editor_selection_restore() can fail. Also by
 * moving text segments (Cut & Paste) can result in moving the markings
 * elsewhere, thus e_content_editor_selection_restore() will restore the selection
 * incorrectly.
 *
 * It is recommended to use this method only when you are not planning to make
 * bigger changes to content or structure of the document (formatting changes
 * are usually OK).
 *
 * Since: 3.22
 **/
void
e_content_editor_selection_save (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->selection_save != NULL);

	iface->selection_save (editor);
}

/**
 * e_content_editor_selection_restore:
 * @editor: an #EContentEditor
 *
 * Restores cursor position or selection range that was saved by
 * e_content_editor_selection_save().
 *
 * Note that calling this function without calling e_content_editor_selection_save()
 * before is a programming error and the behavior is undefined.
 *
 * Since: 3.22
 **/
void
e_content_editor_selection_restore (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->selection_restore != NULL);

	iface->selection_restore (editor);
}

void
e_content_editor_selection_wrap (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->selection_wrap != NULL);

	iface->selection_wrap (editor);
}

guint
e_content_editor_get_caret_position (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->get_caret_position != NULL, 0);

	return iface->get_caret_position (editor);
}

guint
e_content_editor_get_caret_offset (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->get_caret_offset != NULL, 0);

	return iface->get_caret_offset (editor);
}

gchar *
e_content_editor_get_current_signature_uid (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->get_current_signature_uid != NULL, NULL);

	return iface->get_current_signature_uid (editor);
}

gboolean
e_content_editor_is_ready (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->is_ready != NULL, FALSE);

	return iface->is_ready (editor);
}

gchar *
e_content_editor_insert_signature (EContentEditor *editor,
                                   const gchar *content,
                                   gboolean is_html,
                                   const gchar *signature_id,
                                   gboolean *set_signature_from_message,
                                   gboolean *check_if_signature_is_changed,
                                   gboolean *ignore_next_signature_change)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->insert_signature != NULL, FALSE);

	return iface->insert_signature (
		editor,
		content,
		is_html,
		signature_id,
		set_signature_from_message,
		check_if_signature_is_changed,
		ignore_next_signature_change);
}

void
e_content_editor_delete_cell_contents (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->delete_cell_contents != NULL);

	iface->delete_cell_contents (editor);
}

void
e_content_editor_delete_column (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->delete_column != NULL);

	iface->delete_column (editor);
}

void
e_content_editor_delete_row (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->delete_row != NULL);

	iface->delete_row (editor);
}

void
e_content_editor_delete_table (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->delete_table != NULL);

	iface->delete_table (editor);
}

void
e_content_editor_insert_column_after (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->insert_column_after != NULL);

	iface->insert_column_after (editor);
}

void
e_content_editor_insert_column_before (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->insert_column_before != NULL);

	iface->insert_column_before (editor);
}

void
e_content_editor_insert_row_above (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->insert_row_above != NULL);

	iface->insert_row_above (editor);
}

void
e_content_editor_insert_row_below (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->insert_row_below != NULL);

	iface->insert_row_below (editor);
}

gboolean
e_content_editor_on_h_rule_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->on_h_rule_dialog_open != NULL, FALSE);

	return iface->on_h_rule_dialog_open (editor);
}

void
e_content_editor_on_h_rule_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_h_rule_dialog_close != NULL);

	iface->on_h_rule_dialog_close (editor);
}

void
e_content_editor_h_rule_set_align (EContentEditor *editor,
                                   const gchar *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->h_rule_set_align != NULL);

	iface->h_rule_set_align (editor, value);
}

gchar *
e_content_editor_h_rule_get_align (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->h_rule_get_align != NULL, NULL);

	return iface->h_rule_get_align (editor);
}

void
e_content_editor_h_rule_set_size (EContentEditor *editor,
                                  gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->h_rule_set_size != NULL);

	iface->h_rule_set_size (editor, value);
}

gint
e_content_editor_h_rule_get_size (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->h_rule_get_size != NULL, 0);

	return iface->h_rule_get_size (editor);
}

void
e_content_editor_h_rule_set_width (EContentEditor *editor,
                                   gint value,
                                   EContentEditorUnit unit)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->h_rule_set_width != NULL);

	iface->h_rule_set_width (editor, value, unit);
}

gint
e_content_editor_h_rule_get_width (EContentEditor *editor,
                                   EContentEditorUnit *unit)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);
	g_return_val_if_fail (unit != NULL, 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->h_rule_get_width != NULL, 0);

	return iface->h_rule_get_width (editor, unit);
}

void
e_content_editor_h_rule_set_no_shade (EContentEditor *editor,
                                      gboolean value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->h_rule_set_no_shade != NULL);

	iface->h_rule_set_no_shade (editor, value);
}

gboolean
e_content_editor_h_rule_get_no_shade (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->h_rule_get_no_shade != NULL, FALSE);

	return iface->h_rule_get_no_shade (editor);
}

void
e_content_editor_on_image_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_image_dialog_open != NULL);

	iface->on_image_dialog_open (editor);
}

void
e_content_editor_on_image_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_image_dialog_close != NULL);

	iface->on_image_dialog_close (editor);
}

void
e_content_editor_image_set_width_follow (EContentEditor *editor,
                                         gboolean value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_width_follow != NULL);

	iface->image_set_width_follow (editor, value);
}

void
e_content_editor_image_set_src (EContentEditor *editor,
                                const gchar *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_src != NULL);

	iface->image_set_src (editor, value);
}

gchar *
e_content_editor_image_get_src (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->image_get_src != NULL, FALSE);

	return iface->image_get_src (editor);
}

void
e_content_editor_image_set_alt (EContentEditor *editor,
                                const gchar *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_alt != NULL);

	iface->image_set_alt (editor, value);
}

gchar *
e_content_editor_image_get_alt (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->image_get_alt != NULL, FALSE);

	return iface->image_get_alt (editor);
}

void
e_content_editor_image_set_url (EContentEditor *editor,
                                const gchar *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_url != NULL);

	iface->image_set_url (editor, value);
}

gchar *
e_content_editor_image_get_url (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->image_get_url != NULL, FALSE);

	return iface->image_get_url (editor);
}

void
e_content_editor_image_set_vspace (EContentEditor *editor,
                                   gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_vspace != NULL);

	iface->image_set_vspace (editor, value);
}

gint
e_content_editor_image_get_vspace (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->image_get_vspace != NULL, 0);

	return iface->image_get_vspace (editor);
}


void
e_content_editor_image_set_hspace (EContentEditor *editor,
                                   gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_hspace != NULL);

	iface->image_set_hspace (editor, value);
}

gint
e_content_editor_image_get_hspace (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->image_get_hspace != NULL, 0);

	return iface->image_get_hspace (editor);
}

void
e_content_editor_image_set_border (EContentEditor *editor,
                                   gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_border != NULL);

	iface->image_set_border (editor, value);
}

gint
e_content_editor_image_get_border (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->image_get_border != NULL, FALSE);

	return iface->image_get_border (editor);
}

void
e_content_editor_image_set_align (EContentEditor *editor,
                                  const gchar *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_align != NULL);

	iface->image_set_align (editor, value);
}

gchar *
e_content_editor_image_get_align (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->image_get_align != NULL, FALSE);

	return iface->image_get_align (editor);
}

gint32
e_content_editor_image_get_natural_width (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->image_get_natural_width != NULL, 0);

	return iface->image_get_natural_width (editor);
}

gint32
e_content_editor_image_get_natural_height (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->image_get_natural_height != NULL, 0);

	return iface->image_get_natural_height (editor);
}

void
e_content_editor_image_set_width (EContentEditor *editor,
                                  gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_width != NULL);

	iface->image_set_width (editor, value);
}

gint32
e_content_editor_image_get_width (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);
	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->image_get_width != NULL, 0);

	return iface->image_get_width (editor);
}

void
e_content_editor_image_set_height (EContentEditor *editor,
                                   gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_height != NULL);

	iface->image_set_height (editor, value);
}

gint32
e_content_editor_image_get_height (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->image_get_height != NULL, 0);

	return iface->image_get_height (editor);
}

void
e_content_editor_image_set_height_follow (EContentEditor *editor,
                                          gboolean value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->image_set_height_follow != NULL);

	iface->image_set_height_follow (editor, value);
}

void
e_content_editor_on_link_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_link_dialog_open != NULL);

	iface->on_link_dialog_open (editor);
}

void
e_content_editor_on_link_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_link_dialog_close != NULL);

	iface->on_link_dialog_close (editor);
}

void
e_content_editor_link_get_values (EContentEditor *editor,
                                  gchar **href,
                                  gchar **text)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->link_get_values != NULL);

	return iface->link_get_values (editor, href, text);
}

void
e_content_editor_link_set_values (EContentEditor *editor,
                                  const gchar *href,
                                  const gchar *text)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->link_set_values != NULL);

	iface->link_set_values (editor, href, text);
}

void
e_content_editor_on_page_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_page_dialog_open != NULL);

	iface->on_page_dialog_open (editor);
}

void
e_content_editor_on_page_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_page_dialog_close != NULL);

	iface->on_page_dialog_close (editor);
}

void
e_content_editor_page_set_text_color (EContentEditor *editor,
                                      const GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_set_text_color != NULL);

	iface->page_set_text_color (editor, value);
}

void
e_content_editor_page_get_text_color (EContentEditor *editor,
                                      GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_get_text_color != NULL);

	return iface->page_get_text_color (editor, value);
}

void
e_content_editor_page_set_background_color (EContentEditor *editor,
                                            const GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_set_background_color != NULL);

	iface->page_set_background_color (editor, value);
}

void
e_content_editor_page_get_background_color (EContentEditor *editor,
                                            GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_get_background_color != NULL);

	return iface->page_get_background_color (editor, value);
}

void
e_content_editor_page_set_link_color (EContentEditor *editor,
                                      const GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_set_link_color != NULL);

	iface->page_set_link_color (editor, value);
}

void
e_content_editor_page_get_link_color (EContentEditor *editor,
                                      GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_get_link_color != NULL);

	return iface->page_get_link_color (editor, value);
}

void
e_content_editor_page_set_visited_link_color (EContentEditor *editor,
                                              const GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_set_visited_link_color != NULL);

	iface->page_set_visited_link_color (editor, value);
}

void
e_content_editor_page_get_visited_link_color (EContentEditor *editor,
                                              GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_get_visited_link_color != NULL);

	return iface->page_get_visited_link_color (editor, value);
}

/* uri could be NULL -> removes the current image */
void
e_content_editor_page_set_background_image_uri (EContentEditor *editor,
                                                const gchar *uri)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->page_set_background_image_uri != NULL);

	iface->page_set_background_image_uri (editor, uri);
}

gchar *
e_content_editor_page_get_background_image_uri (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->page_get_background_image_uri != NULL, NULL);

	return iface->page_get_background_image_uri (editor);
}

void
e_content_editor_on_cell_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_cell_dialog_open != NULL);

	iface->on_cell_dialog_open (editor);
}

void
e_content_editor_on_cell_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_cell_dialog_close != NULL);

	iface->on_cell_dialog_close (editor);
}

void
e_content_editor_cell_set_v_align (EContentEditor *editor,
                                   const gchar *value,
                                   EContentEditorScope scope)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_v_align != NULL);

	iface->cell_set_v_align (editor, value, scope);
}

gchar *
e_content_editor_cell_get_v_align (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->cell_get_v_align != NULL, NULL);

	return iface->cell_get_v_align (editor);
}

void
e_content_editor_cell_set_align	(EContentEditor *editor,
                                 const gchar *value,
                                 EContentEditorScope scope)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_align != NULL);

	iface->cell_set_align (editor, value, scope);
}

gchar *
e_content_editor_cell_get_align (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->cell_get_align != NULL, NULL);

	return iface->cell_get_align (editor);
}

void
e_content_editor_cell_set_wrap (EContentEditor *editor,
                                gboolean value,
                                EContentEditorScope scope)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_wrap != NULL);

	iface->cell_set_wrap (editor, value, scope);
}

gboolean
e_content_editor_cell_get_wrap (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->cell_get_wrap != NULL, FALSE);

	return iface->cell_get_wrap (editor);
}

void
e_content_editor_cell_set_header_style (EContentEditor *editor,
                                        gboolean value,
                                        EContentEditorScope scope)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_header_style != NULL);

	iface->cell_set_header_style (editor, value, scope);
}

gboolean
e_content_editor_cell_is_header (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->cell_is_header != NULL, FALSE);

	return iface->cell_is_header (editor);
}

void
e_content_editor_cell_set_width (EContentEditor *editor,
                                 gint value,
                                 EContentEditorUnit unit,
                                 EContentEditorScope scope)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_width != NULL);

	iface->cell_set_width (editor, value, unit, scope);
}

gint
e_content_editor_cell_get_width (EContentEditor *editor,
                                 EContentEditorUnit *unit)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);
	g_return_val_if_fail (unit != NULL, 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->cell_get_width != NULL, 0);

	return iface->cell_get_width (editor, unit);
}

void
e_content_editor_cell_set_row_span (EContentEditor *editor,
                                    gint value,
                                    EContentEditorScope scope)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_row_span != NULL);

	iface->cell_set_row_span (editor, value, scope);
}

gint
e_content_editor_cell_get_row_span (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->cell_get_row_span != NULL, 0);

	return iface->cell_get_row_span (editor);
}

void
e_content_editor_cell_set_col_span (EContentEditor *editor,
                                    gint value,
                                    EContentEditorScope scope)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_col_span != NULL);

	iface->cell_set_col_span (editor, value, scope);
}

gint
e_content_editor_cell_get_col_span (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->cell_get_col_span != NULL, 0);

	return iface->cell_get_col_span (editor);
}

/* uri could be NULL -> removes the current image */
void
e_content_editor_cell_set_background_image_uri (EContentEditor *editor,
                                                const gchar *uri)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_background_image_uri != NULL);

	iface->cell_set_background_image_uri (editor, uri);
}

gchar *
e_content_editor_cell_get_background_image_uri (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->cell_get_background_image_uri != NULL, NULL);

	return iface->cell_get_background_image_uri (editor);
}

void
e_content_editor_cell_set_background_color (EContentEditor *editor,
                                            const GdkRGBA *value,
                                            EContentEditorScope scope)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (value != NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_set_background_color != NULL);

	iface->cell_set_background_color (editor, value, scope);
}

void
e_content_editor_cell_get_background_color (EContentEditor *editor,
                                            GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->cell_get_background_color != NULL);

	iface->cell_get_background_color (editor, value);
}

void
e_content_editor_table_set_row_count (EContentEditor *editor,
                                      guint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_row_count != NULL);

	iface->table_set_row_count (editor, value);
}

guint
e_content_editor_table_get_row_count (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->table_get_row_count != NULL, 0);

	return iface->table_get_row_count (editor);
}

void
e_content_editor_table_set_column_count (EContentEditor *editor,
                                         guint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_column_count != NULL);

	iface->table_set_column_count (editor, value);
}

guint
e_content_editor_table_get_column_count (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->table_get_column_count != NULL, 0);

	return iface->table_get_column_count (editor);
}

void
e_content_editor_table_set_width (EContentEditor *editor,
                                  gint value,
                                  EContentEditorUnit unit)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_width != NULL);

	iface->table_set_width (editor, value, unit);
}

guint
e_content_editor_table_get_width (EContentEditor *editor,
                                  EContentEditorUnit *unit)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->table_get_width != NULL, 0);

	return iface->table_get_width (editor, unit);
}

void
e_content_editor_table_set_align (EContentEditor *editor,
                                  const gchar *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_align != NULL);

	iface->table_set_align (editor, value);
}

gchar *
e_content_editor_table_get_align (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->table_get_align != NULL, NULL);

	return iface->table_get_align (editor);
}

void
e_content_editor_table_set_padding (EContentEditor *editor,
                                    gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_padding != NULL);

	iface->table_set_padding (editor, value);
}

gint
e_content_editor_table_get_padding (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->table_get_padding != NULL, 0);

	return iface->table_get_padding (editor);
}

void
e_content_editor_table_set_spacing (EContentEditor *editor,
                                    gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_spacing != NULL);

	iface->table_set_spacing (editor, value);
}

gint
e_content_editor_table_get_spacing (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->table_get_spacing != NULL, 0);

	return iface->table_get_spacing (editor);
}

void
e_content_editor_table_set_border (EContentEditor *editor,
                                   gint value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_border != NULL);

	iface->table_set_border (editor, value);
}

gint
e_content_editor_table_get_border (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), 0);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, 0);
	g_return_val_if_fail (iface->table_get_border != NULL, 0);

	return iface->table_get_border (editor);
}

gchar *
e_content_editor_table_get_background_image_uri (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->table_get_background_image_uri != NULL, NULL);

	return iface->table_get_background_image_uri (editor);
}

void
e_content_editor_table_set_background_image_uri (EContentEditor *editor,
                                                 const gchar *uri)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_background_image_uri != NULL);

	iface->table_set_background_image_uri (editor, uri);
}

void
e_content_editor_table_get_background_color (EContentEditor *editor,
                                             GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_get_background_color != NULL);

	iface->table_get_background_color (editor, value);
}

void
e_content_editor_table_set_background_color (EContentEditor *editor,
                                             const GdkRGBA *value)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->table_set_background_color != NULL);

	iface->table_set_background_color (editor, value);
}

gboolean
e_content_editor_on_table_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->on_table_dialog_open != NULL, FALSE);

	return iface->on_table_dialog_open (editor);
}

void
e_content_editor_on_table_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_table_dialog_close != NULL);

	iface->on_table_dialog_close (editor);
}

void
e_content_editor_on_spell_check_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_spell_check_dialog_close != NULL);

	iface->on_spell_check_dialog_close (editor);
}

void
e_content_editor_on_spell_check_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_spell_check_dialog_close != NULL);

	iface->on_spell_check_dialog_close (editor);
}

gchar *
e_content_editor_spell_check_next_word (EContentEditor *editor,
                                        const gchar *word)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->spell_check_next_word != NULL, NULL);

	return iface->spell_check_next_word (editor, word);
}

gchar *
e_content_editor_spell_check_prev_word (EContentEditor *editor,
                                        const gchar *word)
{
	EContentEditorInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), NULL);

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->spell_check_prev_word != NULL, NULL);

	return iface->spell_check_prev_word (editor, word);
}

void
e_content_editor_on_replace_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_replace_dialog_open != NULL);

	iface->on_replace_dialog_open (editor);
}

void
e_content_editor_on_replace_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_replace_dialog_close != NULL);

	iface->on_replace_dialog_close (editor);
}

void
e_content_editor_on_find_dialog_open (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_find_dialog_open != NULL);

	iface->on_find_dialog_open (editor);
}

void
e_content_editor_on_find_dialog_close (EContentEditor *editor)
{
	EContentEditorInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	iface = E_CONTENT_EDITOR_GET_IFACE (editor);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->on_find_dialog_close != NULL);

	iface->on_find_dialog_close (editor);
}

void
e_content_editor_emit_load_finished (EContentEditor *editor)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_signal_emit (editor, signals[LOAD_FINISHED], 0);
}

gboolean
e_content_editor_emit_paste_clipboard (EContentEditor *editor)
{
	gboolean handled = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_signal_emit (editor, signals[PASTE_CLIPBOARD], 0, &handled);

	return handled;
}

gboolean
e_content_editor_emit_paste_primary_clipboard (EContentEditor *editor)
{
	gboolean handled = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_signal_emit (editor, signals[PASTE_PRIMARY_CLIPBOARD], 0, &handled);

	return handled;
}

gboolean
e_content_editor_emit_context_menu_requested (EContentEditor *editor,
                                              EContentEditorNodeFlags flags,
                                              GdkEvent *event)
{
	gboolean handled = FALSE;

	g_return_val_if_fail (E_IS_CONTENT_EDITOR (editor), FALSE);

	g_signal_emit (editor, signals[CONTEXT_MENU_REQUESTED], 0, flags, event, &handled);

	return handled;
}

void
e_content_editor_emit_find_done (EContentEditor *editor,
                                 guint match_count)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_signal_emit (editor, signals[FIND_DONE], 0, match_count);
}

void
e_content_editor_emit_replace_all_done (EContentEditor *editor,
                                        guint replaced_count)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_signal_emit (editor, signals[REPLACE_ALL_DONE], 0, replaced_count);
}

void
e_content_editor_emit_drop_handled (EContentEditor *editor)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));

	g_signal_emit (editor, signals[DROP_HANDLED], 0);
}
