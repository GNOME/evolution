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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-webkit-content-editor.h"

#include "web-extension/e-html-editor-web-extension-names.h"

#include <e-util/e-util.h>
#include <string.h>

#define E_WEBKIT_CONTENT_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEBKIT_CONTENT_EDITOR, EWebKitContentEditorPrivate))

/* FIXME WK2 Move to e-content-editor? */
#define UNICODE_NBSP "\xc2\xa0"
#define SPACES_PER_LIST_LEVEL 3
#define SPACES_ORDERED_LIST_FIRST_LEVEL 6

enum {
	PROP_0,
	PROP_CAN_COPY,
	PROP_CAN_CUT,
	PROP_CAN_PASTE,
	PROP_CAN_REDO,
	PROP_CAN_UNDO,
	PROP_CHANGED,
	PROP_EDITABLE,
	PROP_HTML_MODE,
	PROP_SPELL_CHECKER,

	PROP_ALIGNMENT,
	PROP_BACKGROUND_COLOR,
	PROP_BLOCK_FORMAT,
	PROP_BOLD,
	PROP_FONT_COLOR,
	PROP_FONT_NAME,
	PROP_FONT_SIZE,
	PROP_INDENTED,
	PROP_ITALIC,
	PROP_MONOSPACED,
	PROP_STRIKETHROUGH,
	PROP_SUBSCRIPT,
	PROP_SUPERSCRIPT,
	PROP_UNDERLINE
};

struct _EWebKitContentEditorPrivate {
	GDBusProxy *web_extension;
	guint web_extension_watch_name_id;
	guint web_extension_selection_changed_cb_id;
	guint web_extension_content_changed_cb_id;

	gboolean html_mode;
	gboolean changed;
	gboolean can_copy;
	gboolean can_cut;
	gboolean can_paste;
	gboolean can_undo;
	gboolean can_redo;

	gboolean emit_load_finished_when_extension_is_ready;
	gboolean reload_in_progress;
	gboolean copy_paste_clipboard_in_view;
	gboolean copy_paste_primary_in_view;
	gboolean copy_cut_actions_triggered;
	gboolean pasting_primary_clipboard;

	gboolean is_bold;
	gboolean is_italic;
	gboolean is_underline;
	gboolean is_monospaced;
	gboolean is_strikethrough;
	gboolean is_indented;
	gboolean is_superscript;
	gboolean is_subscript;

	GdkRGBA *background_color;
	GdkRGBA *font_color;

	gchar *font_name;

	guint font_size;

	EContentEditorBlockFormat block_format;
	EContentEditorAlignment alignment;

	gchar *current_user_stylesheet;

	WebKitLoadEvent webkit_load_event;

	GQueue *post_reload_operations;

	GSettings *mail_settings;
	GSettings *font_settings;
	GSettings *aliasing_settings;

	GHashTable *old_settings;

	EContentEditorContentFlags content_flags;

	ESpellChecker *spell_checker;

	gulong owner_change_primary_clipboard_cb_id;
	gulong owner_change_clipboard_cb_id;

	WebKitFindController *find_controller; /* not referenced; set to non-NULL only if the search is in progress */
	gboolean performing_replace_all;
	guint replaced_count;
	gchar *replace_with;
	gulong found_text_handler_id;
	gulong failed_to_find_text_handler_id;
};

static const GdkRGBA black = { 0, 0, 0, 1 };
static const GdkRGBA white = { 1, 1, 1, 1 };
static const GdkRGBA transparent = { 0, 0, 0, 0 };

typedef void (*PostReloadOperationFunc) (EWebKitContentEditor *wk_editor, gpointer data, EContentEditorInsertContentFlags flags);

typedef struct {
	PostReloadOperationFunc func;
	EContentEditorInsertContentFlags flags;
	gpointer data;
	GDestroyNotify data_free_func;
} PostReloadOperation;

static void e_webkit_content_editor_content_editor_init (EContentEditorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EWebKitContentEditor,
	e_webkit_content_editor,
	WEBKIT_TYPE_WEB_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_CONTENT_EDITOR,
		e_webkit_content_editor_content_editor_init));

EWebKitContentEditor *
e_webkit_content_editor_new (void)
{
	return g_object_new (E_TYPE_WEBKIT_CONTENT_EDITOR, NULL);
}

static void
webkit_content_editor_can_paste_cb (WebKitWebView *view,
                                    GAsyncResult *result,
                                    EWebKitContentEditor *wk_editor)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (view, result, NULL);

	if (wk_editor->priv->can_paste != value) {
		wk_editor->priv->can_paste = value;
		g_object_notify (G_OBJECT (wk_editor), "can-paste");
	}
}

static gboolean
webkit_content_editor_can_paste (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_paste;
}

static void
webkit_content_editor_can_cut_cb (WebKitWebView *view,
                                  GAsyncResult *result,
                                  EWebKitContentEditor *wk_editor)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (view, result, NULL);

	if (wk_editor->priv->can_cut != value) {
		wk_editor->priv->can_cut = value;
		g_object_notify (G_OBJECT (wk_editor), "can-cut");
	}
}

static gboolean
webkit_content_editor_can_cut (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_cut;
}

static void
webkit_content_editor_can_copy_cb (WebKitWebView *view,
                                   GAsyncResult *result,
                                   EWebKitContentEditor *wk_editor)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (view, result, NULL);

	if (wk_editor->priv->can_copy != value) {
		wk_editor->priv->can_copy = value;
		/* This means that we have an active selection thus the primary
		 * clipboard content is from composer. */
		if (value)
			wk_editor->priv->copy_paste_primary_in_view = TRUE;
		/* FIXME notify web extension about pasting content from itself */
		g_object_notify (G_OBJECT (wk_editor), "can-copy");
	}
}

static gboolean
webkit_content_editor_can_copy (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_copy;
}

static gboolean
webkit_content_editor_get_changed (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->changed;
}

static void
webkit_content_editor_set_changed (EWebKitContentEditor *wk_editor,
                                   gboolean changed)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->changed == changed)
		return;

	wk_editor->priv->changed = changed;

	g_object_notify (G_OBJECT (wk_editor), "changed");
}

static void
web_extension_content_changed_cb (GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  EWebKitContentEditor *wk_editor)
{
	if (g_strcmp0 (signal_name, "ContentChanged") != 0)
		return;

	webkit_content_editor_set_changed (wk_editor, TRUE);
}

static void
web_extension_selection_changed_cb (GDBusConnection *connection,
                                    const gchar *sender_name,
                                    const gchar *object_path,
                                    const gchar *interface_name,
                                    const gchar *signal_name,
                                    GVariant *parameters,
                                    EWebKitContentEditor *wk_editor)
{
	gchar *font_color = NULL;

	if (g_strcmp0 (signal_name, "SelectionChanged") != 0)
		return;

	if (!parameters)
		return;

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor),
		WEBKIT_EDITING_COMMAND_COPY,
		NULL, /* cancellable */
		(GAsyncReadyCallback) webkit_content_editor_can_copy_cb,
		wk_editor);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor),
		WEBKIT_EDITING_COMMAND_CUT,
		NULL, /* cancellable */
		(GAsyncReadyCallback) webkit_content_editor_can_cut_cb,
		wk_editor);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor),
		WEBKIT_EDITING_COMMAND_PASTE,
		NULL, /* cancellable */
		(GAsyncReadyCallback) webkit_content_editor_can_paste_cb,
		wk_editor);

	g_object_freeze_notify (G_OBJECT (wk_editor));

	g_variant_get (
		parameters,
		"(iibbbbbbbbbis)",
		&wk_editor->priv->alignment,
		&wk_editor->priv->block_format,
		&wk_editor->priv->is_indented,
		&wk_editor->priv->is_bold,
		&wk_editor->priv->is_italic,
		&wk_editor->priv->is_underline,
		&wk_editor->priv->is_strikethrough,
		&wk_editor->priv->is_monospaced,
		&wk_editor->priv->is_subscript,
		&wk_editor->priv->is_superscript,
		&wk_editor->priv->is_underline,
		&wk_editor->priv->font_size,
		&font_color);


	if (wk_editor->priv->html_mode) {
		GdkRGBA color;

		if (font_color && *font_color && gdk_rgba_parse (&color, font_color)) {
			if (wk_editor->priv->font_color)
				gdk_rgba_free (wk_editor->priv->font_color);
			wk_editor->priv->font_color = gdk_rgba_copy (&color);
		}
	}
	g_free (font_color);

	g_object_notify (G_OBJECT (wk_editor), "alignment");
	g_object_notify (G_OBJECT (wk_editor), "block-format");
	g_object_notify (G_OBJECT (wk_editor), "indented");

	if (wk_editor->priv->html_mode) {
//		g_object_notify (G_OBJECT (wk_editor), "background-color");
		g_object_notify (G_OBJECT (wk_editor), "bold");
//		g_object_notify (G_OBJECT (wk_editor), "font-name");
		g_object_notify (G_OBJECT (wk_editor), "font-size");
		g_object_notify (G_OBJECT (wk_editor), "font-color");
		g_object_notify (G_OBJECT (wk_editor), "italic");
		g_object_notify (G_OBJECT (wk_editor), "monospaced");
		g_object_notify (G_OBJECT (wk_editor), "strikethrough");
		g_object_notify (G_OBJECT (wk_editor), "subscript");
		g_object_notify (G_OBJECT (wk_editor), "superscript");
		g_object_notify (G_OBJECT (wk_editor), "underline");
	}

	g_object_thaw_notify (G_OBJECT (wk_editor));
}

static void
dispatch_pending_operations (EWebKitContentEditor *wk_editor)
{
	if (!wk_editor->priv->web_extension)
		return;

	if (wk_editor->priv->webkit_load_event != WEBKIT_LOAD_FINISHED)
		return;

	/* Dispatch queued operations - as we are using this just for load
	 * operations load just the latest request and throw away the rest. */
	if (wk_editor->priv->post_reload_operations &&
	    !g_queue_is_empty (wk_editor->priv->post_reload_operations)) {

		PostReloadOperation *op;

		op = g_queue_pop_head (wk_editor->priv->post_reload_operations);

		op->func (wk_editor, op->data, op->flags);

		if (op->data_free_func)
			op->data_free_func (op->data);
		g_free (op);

		while ((op = g_queue_pop_head (wk_editor->priv->post_reload_operations))) {
			if (op->data_free_func)
				op->data_free_func (op->data);
			g_free (op);
		}

		g_queue_clear (wk_editor->priv->post_reload_operations);
	}
}

static void
web_extension_proxy_created_cb (GDBusProxy *proxy,
                                GAsyncResult *result,
                                EWebKitContentEditor *wk_editor)
{
	GError *error = NULL;

	wk_editor->priv->web_extension = g_dbus_proxy_new_finish (result, &error);
	if (!wk_editor->priv->web_extension) {
		g_warning ("Error creating web extension proxy: %s\n", error->message);
		g_error_free (error);

		return;
	}

	if (wk_editor->priv->web_extension_selection_changed_cb_id == 0) {
		wk_editor->priv->web_extension_selection_changed_cb_id =
			g_dbus_connection_signal_subscribe (
				g_dbus_proxy_get_connection (wk_editor->priv->web_extension),
				g_dbus_proxy_get_name (wk_editor->priv->web_extension),
				E_HTML_EDITOR_WEB_EXTENSION_INTERFACE,
				"SelectionChanged",
				E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				(GDBusSignalCallback) web_extension_selection_changed_cb,
				wk_editor,
				NULL);
	}

	if (wk_editor->priv->web_extension_content_changed_cb_id == 0) {
		wk_editor->priv->web_extension_content_changed_cb_id =
			g_dbus_connection_signal_subscribe (
				g_dbus_proxy_get_connection (wk_editor->priv->web_extension),
				g_dbus_proxy_get_name (wk_editor->priv->web_extension),
				E_HTML_EDITOR_WEB_EXTENSION_INTERFACE,
				"ContentChanged",
				E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				(GDBusSignalCallback) web_extension_content_changed_cb,
				wk_editor,
				NULL);
	}

	dispatch_pending_operations (wk_editor);

	if (wk_editor->priv->emit_load_finished_when_extension_is_ready) {
		e_content_editor_emit_load_finished (E_CONTENT_EDITOR (wk_editor));

		wk_editor->priv->emit_load_finished_when_extension_is_ready = FALSE;
	}
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           EWebKitContentEditor *wk_editor)
{
	g_dbus_proxy_new (
		connection,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL,
		name,
		E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_HTML_EDITOR_WEB_EXTENSION_INTERFACE,
		NULL,
		(GAsyncReadyCallback) web_extension_proxy_created_cb,
		wk_editor);
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           EWebKitContentEditor *wk_editor)
{
	g_clear_object (&wk_editor->priv->web_extension);
}

static void
webkit_content_editor_watch_web_extension (EWebKitContentEditor *wk_editor)
{
	wk_editor->priv->web_extension_watch_name_id =
		g_bus_watch_name (
			G_BUS_TYPE_SESSION,
			E_HTML_EDITOR_WEB_EXTENSION_SERVICE_NAME,
			G_BUS_NAME_WATCHER_FLAGS_NONE,
			(GBusNameAppearedCallback) web_extension_appeared_cb,
			(GBusNameVanishedCallback) web_extension_vanished_cb,
			wk_editor,
			NULL);
}

static guint64
current_page_id (EWebKitContentEditor *wk_editor)
{
	return webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (wk_editor));
}

static void
webkit_content_editor_call_simple_extension_function (EWebKitContentEditor *wk_editor,
                                                      const gchar *function)
{
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		function,
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static GVariant *
webkit_content_editor_get_element_attribute (EWebKitContentEditor *wk_editor,
                                             const gchar *selector,
                                             const gchar *attribute)
{
	if (!wk_editor->priv->web_extension)
		return NULL;

	return g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"ElementGetAttributeBySelector",
		g_variant_new ("(tss)", current_page_id (wk_editor), selector, attribute),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);
}

static void
webkit_content_editor_set_element_attribute (EWebKitContentEditor *wk_editor,
                                             const gchar *selector,
                                             const gchar *attribute,
                                             const gchar *value)
{
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"ElementSetAttributeBySelector",
		g_variant_new (
			"(tsss)", current_page_id (wk_editor), selector, attribute, value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_remove_element_attribute (EWebKitContentEditor *wk_editor,
                                                const gchar *selector,
                                                const gchar *attribute)
{
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"ElementRemoveAttributeBySelector",
		g_variant_new ("(tss)", current_page_id (wk_editor), selector, attribute),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_set_format_boolean (EWebKitContentEditor *wk_editor,
                                          const gchar *format_dom_function,
                                          gboolean format_value)
{
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		format_dom_function,
		g_variant_new ("(tb)", current_page_id (wk_editor), format_value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_set_format_int (EWebKitContentEditor *wk_editor,
                                      const gchar *format_dom_function,
                                      gint32 format_value)
{
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		format_dom_function,
		g_variant_new ("(ti)", current_page_id (wk_editor), format_value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_set_format_string (EWebKitContentEditor *wk_editor,
                                         const gchar *format_name,
                                         const gchar *format_dom_function,
                                         const gchar *format_value)
{
	if (!wk_editor->priv->web_extension)
		return;

	if (!wk_editor->priv->html_mode)
		return;

	webkit_content_editor_set_changed (wk_editor, TRUE);

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		format_dom_function,
		g_variant_new ("(ts)", current_page_id (wk_editor), format_value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_object_notify (G_OBJECT (wk_editor), format_name);
}

static void
webkit_content_editor_queue_post_reload_operation (EWebKitContentEditor *wk_editor,
                                                   PostReloadOperationFunc func,
                                                   gpointer data,
                                                   GDestroyNotify data_free_func,
						   EContentEditorInsertContentFlags flags)
{
	PostReloadOperation *op;

	g_return_if_fail (func != NULL);

	if (wk_editor->priv->post_reload_operations == NULL)
		wk_editor->priv->post_reload_operations = g_queue_new ();

	op = g_new0 (PostReloadOperation, 1);
	op->func = func;
	op->flags = flags;
	op->data = data;
	op->data_free_func = data_free_func;

	g_queue_push_head (wk_editor->priv->post_reload_operations, op);
}

static void
webkit_content_editor_update_styles (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gboolean mark_citations, use_custom_font;
	gchar *font, *aa = NULL, *citation_color;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GString *stylesheet;
	PangoFontDescription *min_size, *ms, *vw;
	WebKitSettings *settings;
	WebKitUserContentManager *manager;
	WebKitUserStyleSheet *style_sheet;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	use_custom_font = g_settings_get_boolean (
		wk_editor->priv->mail_settings, "use-custom-font");

	if (use_custom_font) {
		font = g_settings_get_string (
			wk_editor->priv->mail_settings, "monospace-font");
		ms = pango_font_description_from_string (font ? font : "monospace 10");
		g_free (font);
	} else {
		font = g_settings_get_string (
			wk_editor->priv->font_settings, "monospace-font-name");
		ms = pango_font_description_from_string (font ? font : "monospace 10");
		g_free (font);
	}

	if (wk_editor->priv->html_mode) {
		if (use_custom_font) {
			font = g_settings_get_string (
				wk_editor->priv->mail_settings, "variable-width-font");
			vw = pango_font_description_from_string (font ? font : "serif 10");
			g_free (font);
		} else {
			font = g_settings_get_string (
				wk_editor->priv->font_settings, "font-name");
			vw = pango_font_description_from_string (font ? font : "serif 10");
			g_free (font);
		}
	} else {
		/* When in plain text mode, force monospace font */
		vw = pango_font_description_copy (ms);
	}

	stylesheet = g_string_new ("");
	g_string_append_printf (
		stylesheet,
		"body {\n"
		"  font-family: '%s';\n"
		"  font-size: %dpt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n"
		" -webkit-line-break: after-white-space;\n",
		pango_font_description_get_family (vw),
		pango_font_description_get_size (vw) / PANGO_SCALE,
		pango_font_description_get_weight (vw),
		styles[pango_font_description_get_style (vw)]);

	if (wk_editor->priv->aliasing_settings != NULL)
		aa = g_settings_get_string (
			wk_editor->priv->aliasing_settings, "antialiasing");

	if (g_strcmp0 (aa, "none") == 0)
		smoothing = "none";
	else if (g_strcmp0 (aa, "grayscale") == 0)
		smoothing = "antialiased";
	else if (g_strcmp0 (aa, "rgba") == 0)
		smoothing = "subpixel-antialiased";

	if (smoothing != NULL)
		g_string_append_printf (
			stylesheet,
			" -webkit-font-smoothing: %s;\n",
			smoothing);

	g_free (aa);

	g_string_append (stylesheet, "}\n");

	g_string_append_printf (
		stylesheet,
		"pre,code,.pre {\n"
		"  font-family: '%s';\n"
		"  font-size: %dpt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n"
		"}",
		pango_font_description_get_family (ms),
		pango_font_description_get_size (ms) / PANGO_SCALE,
		pango_font_description_get_weight (ms),
		styles[pango_font_description_get_style (ms)]);

	/* See bug #689777 for details */
	g_string_append (
		stylesheet,
		"p,pre,code,address {\n"
		"  margin: 0;\n"
		"}\n"
		"h1,h2,h3,h4,h5,h6 {\n"
		"  margin-top: 0.2em;\n"
		"  margin-bottom: 0.2em;\n"
		"}\n");

	/* When inserting a table into contenteditable element the width of the
	 * cells is nearly zero and the td { min-height } doesn't work so put
	 * unicode zero width space before each cell. */
	g_string_append (
		stylesheet,
		"td:before {\n"
		" content: \"\xe2\x80\x8b\";"
		"}\n");

	g_string_append (
		stylesheet,
		"img "
		"{\n"
		"  height: inherit; \n"
		"  width: inherit; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"span.-x-evo-resizable-wrapper:hover "
		"{\n"
		"  outline: 1px dashed red; \n"
		"  resize: both; \n"
		"  overflow: hidden; \n"
		"  display: inline-block; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"td:hover "
		"{\n"
		"  outline: 1px dotted red;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		".-x-evo-plaintext-table "
		"{\n"
		"  border-collapse: collapse;\n"
		"  width: %dch;\n"
		"}\n",
		g_settings_get_int (wk_editor->priv->mail_settings, "composer-word-wrap-length"));

	g_string_append (
		stylesheet,
		".-x-evo-plaintext-table td"
		"{\n"
		"  vertical-align: top;\n"
		"}\n");

	g_string_append (
		stylesheet,
		"td > *"
		"{\n"
		"  display : inline-block;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"ul[data-evo-plain-text]"
		"{\n"
		"  list-style: outside none;\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_PER_LIST_LEVEL);

	g_string_append_printf (
		stylesheet,
		"ul[data-evo-plain-text] > li"
		"{\n"
		"  list-style-position: outside;\n"
		"  text-indent: -%dch;\n"
		"}\n", SPACES_PER_LIST_LEVEL - 1);

	g_string_append (
		stylesheet,
		"ul[data-evo-plain-text] > li::before "
		"{\n"
		"  content: \"*"UNICODE_NBSP"\";\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"ul[data-evo-plain-text].-x-evo-indented "
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_PER_LIST_LEVEL);

	g_string_append (
		stylesheet,
		"ul:not([data-evo-plain-text]) > li.-x-evo-align-center,ol > li.-x-evo-align-center"
		"{\n"
		"  list-style-position: inside;\n"
		"}\n");

	g_string_append (
		stylesheet,
		"ul:not([data-evo-plain-text]) > li.-x-evo-align-right, ol > li.-x-evo-align-right"
		"{\n"
		"  list-style-position: inside;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"ol"
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_ORDERED_LIST_FIRST_LEVEL);

	g_string_append_printf (
		stylesheet,
		"ol.-x-evo-indented"
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_PER_LIST_LEVEL);

	g_string_append (
		stylesheet,
		".-x-evo-align-left "
		"{\n"
		"  text-align: left; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-align-center "
		"{\n"
		"  text-align: center; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-align-right "
		"{\n"
		"  text-align: right; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"ol,ul "
		"{\n"
		"  -webkit-margin-before: 0em; \n"
		"  -webkit-margin-after: 0em; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"blockquote "
		"{\n"
		"  -webkit-margin-before: 0em; \n"
		"  -webkit-margin-after: 0em; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"a "
		"{\n"
		"  word-wrap: break-word; \n"
		"  word-break: break-all; \n"
		"}\n");

	citation_color = g_settings_get_string (
		wk_editor->priv->mail_settings, "citation-color");
	mark_citations = g_settings_get_boolean (
		wk_editor->priv->mail_settings, "mark-citations");

	g_string_append (
		stylesheet,
		"blockquote[type=cite] "
		"{\n"
		"  padding: 0.0ex 0ex;\n"
		"  margin: 0ex;\n"
		"  -webkit-margin-start: 0em; \n"
		"  -webkit-margin-end : 0em; \n");

	if (mark_citations && citation_color)
		g_string_append_printf (
			stylesheet,
			"  color: %s !important; \n",
			citation_color);

	g_free (citation_color);
	citation_color = NULL;

	g_string_append (stylesheet, "}\n");

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character "
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (1));

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (2));

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (3));

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (4));

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (5));

	g_string_append (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  padding: 0ch 1ch 0ch 1ch;\n"
		"  margin: 0ch;\n"
		"  border-width: 0px 2px 0px 2px;\n"
		"  border-style: none solid none solid;\n"
		"  border-radius: 2px;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (1));

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (2));

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (3));

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (4));

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (5));

	if (pango_font_description_get_size (ms) < pango_font_description_get_size (vw) || !wk_editor->priv->html_mode)
		min_size = ms;
	else
		min_size = vw;

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (wk_editor));
	g_object_set (
		G_OBJECT (settings),
		"default-font-size",
		e_util_normalize_font_size (
			GTK_WIDGET (wk_editor), pango_font_description_get_size (vw) / PANGO_SCALE),
		"default-font-family",
		pango_font_description_get_family (vw),
		"monospace-font-family",
		pango_font_description_get_family (ms),
		"default-monospace-font-size", pango_font_description_get_size (ms) / PANGO_SCALE,
		"minimum-font-size", pango_font_description_get_size (min_size) / PANGO_SCALE,
		NULL);

	manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (wk_editor));
	webkit_user_content_manager_remove_all_style_sheets (manager);

	style_sheet = webkit_user_style_sheet_new (
		stylesheet->str,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
		WEBKIT_USER_STYLE_LEVEL_USER,
		NULL,
		NULL);

	webkit_user_content_manager_add_style_sheet (manager, style_sheet);

	g_free (wk_editor->priv->current_user_stylesheet);
	wk_editor->priv->current_user_stylesheet = g_string_free (stylesheet, FALSE);

	webkit_user_style_sheet_unref (style_sheet);

	pango_font_description_free (ms);
	pango_font_description_free (vw);
}

static gboolean
webkit_content_editor_get_html_mode (EWebKitContentEditor *wk_editor)
{
	return wk_editor->priv->html_mode;
}

static gboolean
show_lose_formatting_dialog (EWebKitContentEditor *wk_editor)
{
	gboolean lose;
	GtkWidget *toplevel;
	GtkWindow *parent = NULL;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (wk_editor));

	if (GTK_IS_WINDOW (toplevel))
		parent = GTK_WINDOW (toplevel);

	lose = e_util_prompt_user (
		parent, "org.gnome.evolution.mail", "prompt-on-composer-mode-switch",
		"mail-composer:prompt-composer-mode-switch", NULL);

	if (!lose) {
		/* Nothing has changed, but notify anyway */
		g_object_notify (G_OBJECT (wk_editor), "html-mode");
		return FALSE;
	}

	return TRUE;
}

static void
webkit_content_editor_set_html_mode (EWebKitContentEditor *wk_editor,
                                     gboolean html_mode)
{
	gboolean convert = FALSE;
	GVariant *result;

	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (!wk_editor->priv->web_extension)
		return;

	if (html_mode == wk_editor->priv->html_mode)
		return;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"DOMCheckIfConversionNeeded",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &convert);
		g_variant_unref (result);
	}

	/* If toggling from HTML to the plain text mode, ask the user first if
	 * he wants to convert the content. */
	if (convert && wk_editor->priv->html_mode && !html_mode)
		if (!show_lose_formatting_dialog (wk_editor))
			return;

	wk_editor->priv->html_mode = html_mode;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"SetEditorHTMLMode",
		g_variant_new ("(tbb)", current_page_id (wk_editor), html_mode, convert),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	/* Update fonts - in plain text we only want monospaced */
	webkit_content_editor_update_styles (E_CONTENT_EDITOR (wk_editor));

	g_object_notify (G_OBJECT (wk_editor), "html-mode");
}

static void
set_convert_in_situ (EWebKitContentEditor *wk_editor,
                     gboolean value)
{
	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"SetConvertInSitu",
		g_variant_new ("(tb)", current_page_id (wk_editor), value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

}

static void
webkit_content_editor_insert_content (EContentEditor *editor,
                                      const gchar *content,
                                      EContentEditorInsertContentFlags flags)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	/* It can happen that the view is not ready yet (it is in the middle of
	 * another load operation) so we have to queue the current operation and
	 * redo it again when the view is ready. This was happening when loading
	 * the stuff in EMailSignatureEditor. */
	if (wk_editor->priv->webkit_load_event != WEBKIT_LOAD_FINISHED ||
	    wk_editor->priv->reload_in_progress) {
		webkit_content_editor_queue_post_reload_operation (
			wk_editor,
			(PostReloadOperationFunc) webkit_content_editor_insert_content,
			g_strdup (content),
			g_free,
			flags);
		return;
	}

	if (!wk_editor->priv->web_extension) {
		/* If the operation needs a web extension and it is not ready yet
		 * we need to schedule the current operation again a dispatch it
		 * when the extension is ready */
		if (!((flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		      (flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML) &&
		      (wk_editor->priv->content_flags & E_CONTENT_EDITOR_MESSAGE_DRAFT))) {
			webkit_content_editor_queue_post_reload_operation (
				wk_editor,
				(PostReloadOperationFunc) webkit_content_editor_insert_content,
				g_strdup (content),
				g_free,
				flags);
			return;
		}
	}

	wk_editor->priv->reload_in_progress = TRUE;

	if ((flags & E_CONTENT_EDITOR_INSERT_CONVERT) &&
	    !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		// e_html_editor_view_convert_and_insert_plain_text
		// e_html_editor_view_convert_and_insert_html_to_plain_text
		// e_html_editor_view_insert_text
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"DOMConvertAndInsertHTMLIntoSelection",
			g_variant_new (
				"(tsb)",
				current_page_id (wk_editor),
				content,
				(flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML)),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		   (flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML)) {
		if (wk_editor->priv->content_flags & E_CONTENT_EDITOR_MESSAGE_DRAFT) {
			webkit_web_view_load_html (WEBKIT_WEB_VIEW (wk_editor), content, "file://");
			return;
		}

		if ((wk_editor->priv->content_flags & E_CONTENT_EDITOR_MESSAGE_DRAFT) &&
		    !(wk_editor->priv->html_mode)) {
			if (content && *content)
				set_convert_in_situ (wk_editor, TRUE);
			webkit_web_view_load_html (WEBKIT_WEB_VIEW (wk_editor), content, "file://");
			return;
		}

		/* Only convert messages that are in HTML */
		if (!(wk_editor->priv->html_mode)) {
			if (strstr (content, "<!-- text/html -->")) {
				if (!show_lose_formatting_dialog (wk_editor)) {
					webkit_content_editor_set_html_mode (wk_editor, TRUE);
					webkit_web_view_load_html (
						WEBKIT_WEB_VIEW (wk_editor), content, "file://");
					return;
				}
			}
			if (content && *content)
				set_convert_in_situ (wk_editor, TRUE);
		}

		webkit_web_view_load_html (WEBKIT_WEB_VIEW (wk_editor), content, "file://");
	} else if ((flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		   (flags & E_CONTENT_EDITOR_INSERT_TEXT_PLAIN)) {
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"DOMConvertContent",
			g_variant_new ("(ts)", current_page_id (wk_editor), content),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_CONVERT) &&
		    !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		    !(flags & E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT)) {
		// e_html_editor_view_paste_as_text
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"DOMConvertAndInsertHTMLIntoSelection",
			g_variant_new (
				"(tsb)", current_page_id (wk_editor), content, TRUE),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT) &&
		   !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		// e_html_editor_view_paste_clipboard_quoted
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"DOMQuoteAndInsertTextIntoSelection",
			g_variant_new (
				"(tsb)", current_page_id (wk_editor), content),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_CONVERT) &&
		    !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		// e_html_editor_view_insert_html
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"DOMInsertHTML",
			g_variant_new (
				"(ts)", current_page_id (wk_editor), content),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	} else
		g_warning ("Unsupported flags combination (%d) in (%s)", flags, G_STRFUNC);
}

static CamelMimePart *
create_part_for_inline_image_from_element_data (const gchar *element_src,
                                                const gchar *name,
                                                const gchar *id)
{
	CamelStream *stream;
	CamelDataWrapper *wrapper;
	CamelMimePart *part = NULL;
	gsize decoded_size;
	gssize size;
	gchar *mime_type = NULL;
	const gchar *base64_encoded_data;
	guchar *base64_decoded_data = NULL;

	base64_encoded_data = strstr (element_src, ";base64,");
	if (!base64_encoded_data)
		goto out;

	mime_type = g_strndup (
		element_src + 5,
		base64_encoded_data - (strstr (element_src, "data:") + 5));

	/* Move to actual data */
	base64_encoded_data += 8;

	base64_decoded_data = g_base64_decode (base64_encoded_data, &decoded_size);

	stream = camel_stream_mem_new ();
	size = camel_stream_write (
		stream, (gchar *) base64_decoded_data, decoded_size, NULL, NULL);

	if (size == -1)
		goto out;

	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream_sync (
		wrapper, stream, NULL, NULL);
	g_object_unref (stream);

	camel_data_wrapper_set_mime_type (wrapper, mime_type);

	part = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (part), wrapper);
	g_object_unref (wrapper);

	camel_mime_part_set_content_id (part, id);
	camel_mime_part_set_filename (part, name);
	camel_mime_part_set_disposition (part, "inline");
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);
out:
	g_free (mime_type);
	g_free (base64_decoded_data);

	return part;
}

static GList *
webkit_content_editor_get_parts_for_inline_images (GVariant *images)
{
	const gchar *element_src, *name, *id;
	GVariantIter *iter;
	GList *parts = NULL;

	g_variant_get (images, "asss", &iter);
	while (g_variant_iter_loop (iter, "&s&s&s", &element_src, &name, &id)) {
		CamelMimePart *part;

		part = create_part_for_inline_image_from_element_data (
			element_src, name, id);
		parts = g_list_append (parts, part);
	}
	g_variant_iter_free (iter);

	return parts;
}

static gchar *
webkit_content_editor_get_content (EContentEditor *editor,
                                   EContentEditorGetContentFlags flags,
                                   EContentEditorInlineImages **inline_images)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return g_strdup ("");

	if ((flags & E_CONTENT_EDITOR_GET_TEXT_HTML) &&
	    !(flags & E_CONTENT_EDITOR_GET_PROCESSED) &&
            !(flags & E_CONTENT_EDITOR_GET_BODY))
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"DOMEmbedStyleSheet",
			g_variant_new (
				"(ts)",
				current_page_id (wk_editor),
				wk_editor->priv->current_user_stylesheet),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"DOMGetContent",
		g_variant_new (
			"(tsi)",
			current_page_id (wk_editor),
			inline_images ?  (*inline_images)->from_domain : "",
			(gint32) flags),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if ((flags & E_CONTENT_EDITOR_GET_TEXT_HTML) &&
	    !(flags & E_CONTENT_EDITOR_GET_PROCESSED) &&
            !(flags & E_CONTENT_EDITOR_GET_BODY))
		webkit_content_editor_call_simple_extension_function (
			wk_editor, "DOMRemoveEmbeddedStyleSheet");

	if (result) {
		GVariant *images;
		gchar *value;

		g_variant_get (result, "(sv)", &value, &images);
		if (inline_images)
			(*inline_images)->images = webkit_content_editor_get_parts_for_inline_images (images);
		g_variant_unref (result);

		return value;
	}

	return g_strdup ("");
}

static gboolean
webkit_content_editor_can_undo (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_undo;
}

static void
webkit_content_editor_undo (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (wk_editor, "DOMUndo");
}

static gboolean
webkit_content_editor_can_redo (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_redo;
}

static void
webkit_content_editor_redo (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (editor));

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (wk_editor, "DOMRedo");
}

static void
webkit_content_editor_move_caret_on_coordinates (EContentEditor *editor,
                                                 gint x,
                                                 gint y,
                                                 gboolean cancel_if_not_collapsed)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"DOMMoveSelectionOnPoint",
		g_variant_new (
			"(tiib)", current_page_id (wk_editor), x, y, cancel_if_not_collapsed),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);
}

static void
webkit_content_editor_insert_emoticon (EContentEditor *editor,
                                       EEmoticon *emoticon)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"DOMInsertSmiley",
		g_variant_new (
			"(ts)", current_page_id (wk_editor), e_emoticon_get_name (emoticon)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_insert_image_from_mime_part (EContentEditor *editor,
                                                   CamelMimePart *part)
{
	CamelDataWrapper *dw;
	CamelStream *stream;
	EWebKitContentEditor *wk_editor;
	GByteArray *byte_array;
	gchar *src, *base64_encoded, *mime_type, *cid_uri;
	const gchar *cid, *name;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return;

	stream = camel_stream_mem_new ();
	dw = camel_medium_get_content (CAMEL_MEDIUM (part));
	g_return_if_fail (dw);

	mime_type = camel_data_wrapper_get_mime_type (dw);
	camel_data_wrapper_decode_to_stream_sync (dw, stream, NULL, NULL);
	camel_stream_close (stream, NULL, NULL);

	byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (stream));

	if (!byte_array->data)
		return;

	base64_encoded = g_base64_encode ((const guchar *) byte_array->data, byte_array->len);

	name = camel_mime_part_get_filename (part);
	/* Insert file name before new src */
	src = g_strconcat (name, ";data:", mime_type, ";base64,", base64_encoded, NULL);

	cid = camel_mime_part_get_content_id (part);
	if (!cid) {
		camel_mime_part_set_content_id (part, NULL);
		cid = camel_mime_part_get_content_id (part);
	}
	cid_uri = g_strdup_printf ("cid:%s", cid);

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"DOMAddNewInlineImageIntoList",
		g_variant_new ("(tss)", current_page_id (wk_editor), name, cid_uri, src),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_free (base64_encoded);
	g_free (mime_type);
	g_object_unref (stream);
}

static EContentEditorContentFlags
webkit_content_editor_get_current_content_flags (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	return wk_editor->priv->content_flags;
}

static void
webkit_content_editor_set_current_content_flags (EContentEditor *editor,
                                                 EContentEditorContentFlags flags)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	wk_editor->priv->content_flags = flags;

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"SetCurrentContentFlags",
		g_variant_new ("(ti)", current_page_id (wk_editor), (gint32) flags),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_select_all (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor), WEBKIT_EDITING_COMMAND_SELECT_ALL);
}

static void
webkit_content_editor_show_inspector (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	WebKitWebInspector *inspector;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (wk_editor));

	webkit_web_inspector_show (inspector);
}

static void
webkit_content_editor_selection_wrap (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (wk_editor, "DOMSelectionWrap");
}

static gboolean
webkit_content_editor_selection_is_indented (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_indented;
}

static void
webkit_content_editor_selection_indent (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "DOMSelectionIndent");
}

static void
webkit_content_editor_selection_unindent (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "DOMSelectionUnindent");
}

static void
webkit_content_editor_cut (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gboolean handled = FALSE;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	wk_editor->priv->copy_cut_actions_triggered = TRUE;

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorActionsSaveHistoryForCut");

	g_signal_emit_by_name (editor, "cut-clipboard", editor, &handled);

	if (!handled)
		webkit_web_view_execute_editing_command (
			WEBKIT_WEB_VIEW (wk_editor), WEBKIT_EDITING_COMMAND_CUT);
}

static void
webkit_content_editor_copy (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gboolean handled = FALSE;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	wk_editor->priv->copy_cut_actions_triggered = TRUE;

	g_signal_emit_by_name (editor, "copy-clipboard", editor, &handled);

	if (!handled)
		webkit_web_view_execute_editing_command (
			WEBKIT_WEB_VIEW (wk_editor), WEBKIT_EDITING_COMMAND_COPY);
}

static ESpellChecker *
webkit_content_editor_get_spell_checker (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	return wk_editor->priv->spell_checker;
}

static gchar *
webkit_content_editor_get_caret_word (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *ret_val = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return NULL;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"DOMGetCaretWord",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(s)", &ret_val);
		g_variant_unref (result);
	}

	return ret_val;
}

static void
webkit_content_editor_set_spell_checking_languages (EContentEditor *editor,
                                                    const gchar **languages)
{
	EWebKitContentEditor *wk_editor;
	WebKitWebContext *web_context;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (wk_editor));
	webkit_web_context_set_spell_checking_languages (web_context, (const gchar * const *) languages);
}

static void
webkit_content_editor_set_spell_check (EContentEditor *editor,
                                       gboolean enable)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, enable ? "DOMForceSpellCheck" : "DOMTurnSpellCheckOff");
}

static gboolean
webkit_content_editor_get_spell_check (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	return g_settings_get_boolean (
		wk_editor->priv->mail_settings, "composer-inline-spelling");
}

static gboolean
webkit_content_editor_is_editable (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return webkit_web_view_is_editable (WEBKIT_WEB_VIEW (wk_editor));
}

static void
webkit_content_editor_set_editable (EWebKitContentEditor *wk_editor,
                                    gboolean editable)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	return webkit_web_view_set_editable (WEBKIT_WEB_VIEW (wk_editor), editable);
}

static gchar *
webkit_content_editor_get_current_signature_uid (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *ret_val= NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return NULL;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"DOMGetActiveSignatureUid",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(s)", &ret_val);
		g_variant_unref (result);
	}

	return ret_val;
}

static gboolean
webkit_content_editor_is_ready (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	return !webkit_web_view_is_loading (WEBKIT_WEB_VIEW (wk_editor)) && wk_editor->priv->web_extension;
}

static char *
webkit_content_editor_insert_signature (EContentEditor *editor,
                                        const gchar *content,
                                        gboolean is_html,
                                        const gchar *signature_id,
                                        gboolean *set_signature_from_message,
                                        gboolean *check_if_signature_is_changed,
                                        gboolean *ignore_next_signature_change)
{
	EWebKitContentEditor *wk_editor;
	gchar *ret_val = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return NULL;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"DOMInsertSignature",
		g_variant_new (
			"(tsbsbbb)",
			current_page_id (wk_editor),
			content ? content : "",
			is_html,
			signature_id,
			set_signature_from_message,
			check_if_signature_is_changed,
			ignore_next_signature_change),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (
			result,
			"(sbbb)",
			&ret_val,
			set_signature_from_message,
			check_if_signature_is_changed,
			ignore_next_signature_change);
		g_variant_unref (result);
	}

	return ret_val;
}

static guint
webkit_content_editor_get_caret_position (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	guint ret_val = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"DOMGetCaretPosition",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		ret_val = g_variant_get_uint32 (result);
		g_variant_unref (result);
	}

	return ret_val;
}

static guint
webkit_content_editor_get_caret_offset (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	guint ret_val = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"DOMGetCaretOffset",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		ret_val = g_variant_get_uint32 (result);
		g_variant_unref (result);
	}

	return ret_val;
}

static void
webkit_content_editor_clear_undo_redo_history (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"DOMClearUndoRedoHistory",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_replace_caret_word (EContentEditor *editor,
                                          const gchar *replacement)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"DOMReplaceCaretWord",
		g_variant_new ("(ts)", current_page_id (wk_editor), replacement),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_finish_search (EWebKitContentEditor *wk_editor)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (!wk_editor->priv->find_controller)
		return;

	webkit_find_controller_search_finish (wk_editor->priv->find_controller);

	wk_editor->priv->performing_replace_all = FALSE;
	wk_editor->priv->replaced_count = 0;
	g_free (wk_editor->priv->replace_with);
	wk_editor->priv->replace_with = NULL;

	if (wk_editor->priv->found_text_handler_id) {
		g_signal_handler_disconnect (wk_editor->priv->find_controller, wk_editor->priv->found_text_handler_id);
		wk_editor->priv->found_text_handler_id = 0;
	}

	if (wk_editor->priv->failed_to_find_text_handler_id) {
		g_signal_handler_disconnect (wk_editor->priv->find_controller, wk_editor->priv->failed_to_find_text_handler_id);
		wk_editor->priv->failed_to_find_text_handler_id = 0;
	}

	wk_editor->priv->find_controller = NULL;
}

static guint32 /* WebKitFindOptions */
find_flags_to_webkit_find_options (guint32 flags /* EContentEditorFindFlags */)
{
	guint32 options = 0;

	if (flags & E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE)
		options |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;

	if (flags & E_CONTENT_EDITOR_FIND_WRAP_AROUND)
		options |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;

	if (flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS)
		options |= WEBKIT_FIND_OPTIONS_BACKWARDS;

	return options;
}

static void
webkit_find_controller_found_text_cb (WebKitFindController *find_controller,
                                      guint match_count,
                                      EWebKitContentEditor *wk_editor)
{
	if (wk_editor->priv->performing_replace_all) {
		if (!wk_editor->priv->replaced_count)
			wk_editor->priv->replaced_count = match_count;

		/* Repeatedly search for 'word', then replace selection by
		 * 'replacement'. Repeat until there's at least one occurrence of
		 * 'word' in the document */
		e_content_editor_insert_content (
			E_CONTENT_EDITOR (wk_editor),
			wk_editor->priv->replace_with,
			E_CONTENT_EDITOR_INSERT_TEXT_PLAIN);

		webkit_find_controller_search_next (find_controller);
	} else {
		e_content_editor_emit_find_done (E_CONTENT_EDITOR (wk_editor), match_count);
	}
}

static void
webkit_find_controller_failed_to_find_text_cb (WebKitFindController *find_controller,
                                               EWebKitContentEditor *wk_editor)
{
	if (wk_editor->priv->performing_replace_all) {
		guint replaced_count = wk_editor->priv->replaced_count;

		webkit_content_editor_finish_search (wk_editor);
		e_content_editor_emit_replace_all_done (E_CONTENT_EDITOR (wk_editor), replaced_count);
	} else {
		e_content_editor_emit_find_done (E_CONTENT_EDITOR (wk_editor), 0);
	}
}

static void
webkit_content_editor_prepare_find_controller (EWebKitContentEditor *wk_editor)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));
	g_return_if_fail (wk_editor->priv->find_controller == NULL);

	wk_editor->priv->find_controller = webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (wk_editor));

	wk_editor->priv->found_text_handler_id = g_signal_connect (
		wk_editor->priv->find_controller, "found-text",
		G_CALLBACK (webkit_find_controller_found_text_cb), wk_editor);

	wk_editor->priv->failed_to_find_text_handler_id = g_signal_connect (
		wk_editor->priv->find_controller, "failed-to-find-text",
		G_CALLBACK (webkit_find_controller_failed_to_find_text_cb), wk_editor);

	wk_editor->priv->performing_replace_all = FALSE;
	wk_editor->priv->replaced_count = 0;
	g_free (wk_editor->priv->replace_with);
	wk_editor->priv->replace_with = NULL;
}

static void
webkit_content_editor_find (EContentEditor *editor,
			    guint32 flags,
			    const gchar *text)
{
	EWebKitContentEditor *wk_editor;
	guint32 wk_options;
	gboolean needs_init;

	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	wk_options = find_flags_to_webkit_find_options (flags);

	needs_init = !wk_editor->priv->find_controller;
	if (needs_init) {
		webkit_content_editor_prepare_find_controller (wk_editor);
	} else {
		needs_init = wk_options != webkit_find_controller_get_options (wk_editor->priv->find_controller) ||
			g_strcmp0 (text, webkit_find_controller_get_search_text (wk_editor->priv->find_controller)) != 0;
	}

	if (needs_init) {
		webkit_find_controller_search (wk_editor->priv->find_controller, text, wk_options, G_MAXUINT);
	} else if ((flags & E_CONTENT_EDITOR_FIND_PREVIOUS) != 0) {
		webkit_find_controller_search_previous (wk_editor->priv->find_controller);
	} else {
		webkit_find_controller_search_next (wk_editor->priv->find_controller);
	}
}

static void
webkit_content_editor_replace (EContentEditor *editor,
			       const gchar *replacement)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"DOMSelectionReplace",
		g_variant_new ("(ts)", current_page_id (wk_editor), replacement),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_replace_all (EContentEditor *editor,
				   guint32 flags,
				   const gchar *find_text,
				   const gchar *replace_with)
{
	EWebKitContentEditor *wk_editor;
	guint32 wk_options;

	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (editor));
	g_return_if_fail (find_text != NULL);
	g_return_if_fail (replace_with != NULL);

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	wk_options = find_flags_to_webkit_find_options (flags);

	if (!wk_editor->priv->find_controller)
		webkit_content_editor_prepare_find_controller (wk_editor);

	g_free (wk_editor->priv->replace_with);
	wk_editor->priv->replace_with = g_strdup (replace_with);

	wk_editor->priv->performing_replace_all = TRUE;
	wk_editor->priv->replaced_count = 0;

	webkit_find_controller_search (wk_editor->priv->find_controller, find_text, wk_options, G_MAXUINT);
}

static void
webkit_content_editor_selection_save (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "DOMSelectionSave");
}

static void
webkit_content_editor_selection_restore (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "DOMSelectionRestore");
}

static void
webkit_content_editor_delete_cell_contents (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorDialogDeleteCellContents");
}

static void
webkit_content_editor_delete_column (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorDialogDeleteColumn");
}

static void
webkit_content_editor_delete_row (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorDialogDeleteRow");
}

static void
webkit_content_editor_delete_table (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorDialogDeleteTable");
}

static void
webkit_content_editor_insert_column_after (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorDialogInsertColumnAfter");
}

static void
webkit_content_editor_insert_column_before (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorDialogInsertColumnBefore");
}


static void
webkit_content_editor_insert_row_above (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorDialogInsertRowAbove");
}

static void
webkit_content_editor_insert_row_below (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorDialogInsertRowBelow");
}

static gboolean
webkit_content_editor_on_h_rule_dialog_open (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gboolean value = FALSE;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return FALSE;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"EHTMLEditorHRuleDialogFindHRule",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_on_h_rule_dialog_close (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorHRuleDialogSaveHistoryOnExit");
}

static void
webkit_content_editor_h_rule_set_align (EContentEditor *editor,
                                        const gchar *value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-hr", "align", value);
}

static gchar *
webkit_content_editor_h_rule_get_align (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-hr", "align");
	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_h_rule_set_size (EContentEditor *editor,
                                       gint value)
{
	EWebKitContentEditor *wk_editor;
	gchar *size;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	size = g_strdup_printf ("%d", value);

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-hr", "size", size);

	g_free (size);
}

static gint
webkit_content_editor_h_rule_get_size (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gint size = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-hr", "size");
	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (value && *value)
			size = atoi (value);

		if (size == 0)
			size = 2;

		g_variant_unref (result);
	}

	return size;
}

static void
webkit_content_editor_h_rule_set_width (EContentEditor *editor,
                                        gint value,
                                        EContentEditorUnit unit)
{
	EWebKitContentEditor *wk_editor;
	gchar *width;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	width = g_strdup_printf (
		"%d%s",
		value,
		(unit == E_CONTENT_EDITOR_UNIT_PIXEL) ? "px" : "%");

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-hr", "width", width);

	g_free (width);
}

static gint
webkit_content_editor_h_rule_get_width (EContentEditor *editor,
                                        EContentEditorUnit *unit)
{
	EWebKitContentEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	*unit = E_CONTENT_EDITOR_UNIT_PIXEL;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-hr", "width");
	if (result) {
		const gchar *width;
		g_variant_get (result, "(&s)", &width);
		if (width && *width) {
			value = atoi (width);
			if (strstr (width, "%"))
				*unit = E_CONTENT_EDITOR_UNIT_PERCENTAGE;
		}
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_h_rule_set_no_shade (EContentEditor *editor,
                                           gboolean value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"HRElementSetNoShade",
		g_variant_new (
			"(tsb)", current_page_id (wk_editor), "-x-evo-current-hr", value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gboolean
webkit_content_editor_h_rule_get_no_shade (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gboolean value = FALSE;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return FALSE;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"HRElementGetNoShade",
		g_variant_new (
			"(ts)", current_page_id (wk_editor), "-x-evo-current-hr"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_on_image_dialog_open (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorImageDialogMarkImage");
}

static void
webkit_content_editor_on_image_dialog_close (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorImageDialogSaveHistoryOnExit");
}

static void
webkit_content_editor_insert_image (EContentEditor *editor,
                                    const gchar *image_uri)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"DOMSelectionInsertImage",
		g_variant_new ("(ts)", current_page_id (wk_editor), image_uri),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_replace_image_src (EWebKitContentEditor *wk_editor,
                                         const gchar *selector,
                                         const gchar *image_uri)
{

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"DOMReplaceImageSrc",
		g_variant_new ("(tss)", current_page_id (wk_editor), selector, image_uri),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_image_set_src (EContentEditor *editor,
                                     const gchar *value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_replace_image_src (
		wk_editor, "img#-x-evo-current-img", value);
}

static gchar *
webkit_content_editor_image_get_src (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-img", "data-uri");

	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_image_set_alt (EContentEditor *editor,
                                     const gchar *value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-img", "alt", value);
}

static gchar *
webkit_content_editor_image_get_alt (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-img", "alt");

	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_image_set_url (EContentEditor *editor,
                                     const gchar *value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorImageDialogSetElementUrl",
		g_variant_new ("(ts)", current_page_id (wk_editor), value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gchar *
webkit_content_editor_image_get_url (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gchar *value = NULL;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return NULL;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"EHTMLEditorImageDialogGetElementUrl",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_image_set_vspace (EContentEditor *editor,
                                        gint value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"ImageElementSetVSpace",
		g_variant_new (
			"(tsi)", current_page_id (wk_editor), "-x-evo-current-img", value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gint
webkit_content_editor_image_get_vspace (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"ImageElementGetVSpace",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_image_set_hspace (EContentEditor *editor,
                                        gint value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"ImageElementSetHSpace",
		g_variant_new (
			"(tsi)", current_page_id (wk_editor), "-x-evo-current-img", value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gint
webkit_content_editor_image_get_hspace (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"ImageElementGetHSpace",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_image_set_border (EContentEditor *editor,
                                        gint value)
{
	EWebKitContentEditor *wk_editor;
	gchar *border;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	border = g_strdup_printf ("%d", value);

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-img", "border", border);

	g_free (border);
}

static gint
webkit_content_editor_image_get_border (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-img", "border");

	if (result) {
		const gchar *border;
		g_variant_get (result, "(&s)", &border);
		if (border && *border)
			value = atoi (border);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_image_set_align (EContentEditor *editor,
                                       const gchar *value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-img", "align", value);
}

static gchar *
webkit_content_editor_image_get_align (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-img", "align");

	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gint32
webkit_content_editor_image_get_natural_width (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint32 value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"ImageElementGetNaturalWidth",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gint32
webkit_content_editor_image_get_natural_height (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint32 value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"ImageElementGetNaturalHeight",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_image_set_height (EContentEditor *editor,
                                        gint value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"ImageElementSetHeight",
		g_variant_new (
			"(tsi)", current_page_id (wk_editor), "-x-evo-current-img", value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_image_set_width (EContentEditor *editor,
                                       gint value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"ImageElementSetWidth",
		g_variant_new (
			"(tsi)", current_page_id (wk_editor), "-x-evo-current-img", value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_image_set_height_follow (EContentEditor *editor,
                                              gboolean value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (value)
		webkit_content_editor_set_element_attribute (
			wk_editor, "#-x-evo-current-img", "style", "height: auto;");
	else
		webkit_content_editor_remove_element_attribute (
			wk_editor, "#-x-evo-current-img", "style");
}

static void
webkit_content_editor_image_set_width_follow (EContentEditor *editor,
                                             gboolean value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (value)
		webkit_content_editor_set_element_attribute (
			wk_editor, "#-x-evo-current-img", "style", "width: auto;");
	else
		webkit_content_editor_remove_element_attribute (
			wk_editor, "#-x-evo-current-img", "style");
}

static gint32
webkit_content_editor_image_get_width (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint32 value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"ImageElementGetWidth",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gint32
webkit_content_editor_image_get_height (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint32 value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"ImageElementGetHeight",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_selection_unlink (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorSelectionUnlink");
}

static void
webkit_content_editor_link_set_values (EContentEditor *editor,
                                       const gchar *href,
                                       const gchar *text)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorLinkDialogOk",
		g_variant_new ("(tss)", current_page_id (wk_editor), href, text),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_link_get_values (EContentEditor *editor,
                                       gchar **href,
                                       gchar **text)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"EHTMLEditorLinkDialogShow",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(ss)", href, text);
		g_variant_unref (result);
	} else {
		*href = NULL;
		*text = NULL;
	}
}

static void
webkit_content_editor_set_alignment (EWebKitContentEditor *wk_editor,
                                     EContentEditorAlignment value)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	webkit_content_editor_set_format_int (
		wk_editor, "DOMSelectionSetAlignment", (gint32) value);
}

static EContentEditorAlignment
webkit_content_editor_get_alignment (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), E_CONTENT_EDITOR_ALIGNMENT_LEFT);

	return wk_editor->priv->alignment;
}

static void
webkit_content_editor_set_block_format (EWebKitContentEditor *wk_editor,
                                        EContentEditorBlockFormat value)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	webkit_content_editor_set_format_int (
		wk_editor, "DOMSelectionSetBlockFormat", (gint32) value);
}

static EContentEditorBlockFormat
webkit_content_editor_get_block_format (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), E_CONTENT_EDITOR_BLOCK_FORMAT_NONE);

	return wk_editor->priv->block_format;
}

static void
webkit_content_editor_set_background_color (EWebKitContentEditor *wk_editor,
                                            const GdkRGBA *value)
{
	gchar *color;

	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (gdk_rgba_equal (value, wk_editor->priv->background_color))
		return;

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	if (wk_editor->priv->background_color)
		gdk_rgba_free (wk_editor->priv->background_color);

	wk_editor->priv->background_color = gdk_rgba_copy (value);

	webkit_content_editor_set_format_string (
		wk_editor,
		"background-color",
		"DOMSelectionSetBackgroundColor",
		color);

	g_free (color);
}

static const GdkRGBA *
webkit_content_editor_get_background_color (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), NULL);

	if (!wk_editor->priv->web_extension)
		return NULL;

	if (!wk_editor->priv->html_mode || !wk_editor->priv->background_color)
		return &white;

	return wk_editor->priv->background_color;
}

static void
webkit_content_editor_set_font_name (EWebKitContentEditor *wk_editor,
                                     const gchar *value)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	wk_editor->priv->font_name = g_strdup (value);

	webkit_content_editor_set_format_string (
		wk_editor, "font-name", "DOMSelectionSetFontName", value);
}

static const gchar *
webkit_content_editor_get_font_name (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), NULL);

	return wk_editor->priv->font_name;
}

static void
webkit_content_editor_set_font_color (EWebKitContentEditor *wk_editor,
                                      const GdkRGBA *value)
{
	gchar *color;

	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (gdk_rgba_equal (value, wk_editor->priv->font_color))
		return;

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	if (wk_editor->priv->font_color)
		gdk_rgba_free (wk_editor->priv->font_color);

	wk_editor->priv->font_color = gdk_rgba_copy (value);

	webkit_content_editor_set_format_string (
		wk_editor,
		"font-color",
		"DOMSelectionSetFontColor",
		color);

	g_free (color);
}

static const GdkRGBA *
webkit_content_editor_get_font_color (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), NULL);

	if (!wk_editor->priv->web_extension)
		return NULL;

	if (!wk_editor->priv->html_mode || !wk_editor->priv->font_color)
		return &black;

	return wk_editor->priv->font_color;
}

static void
webkit_content_editor_set_font_size (EWebKitContentEditor *wk_editor,
                                     gint value)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->font_size == value)
		return;

	wk_editor->priv->font_size = value;

	webkit_content_editor_set_format_int (
		wk_editor, "DOMSelectionSetFontSize", value);
}

static gint
webkit_content_editor_get_font_size (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), -1);

	return wk_editor->priv->font_size;
}

static void
webkit_content_editor_set_bold (EWebKitContentEditor *wk_editor,
                                gboolean bold)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->is_bold == bold)
		return;

	wk_editor->priv->is_bold = bold;

	webkit_content_editor_set_format_boolean (
		wk_editor, "DOMSelectionSetBold", bold);
}

static gboolean
webkit_content_editor_is_bold (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_bold;
}

static void
webkit_content_editor_set_italic (EWebKitContentEditor *wk_editor,
                                  gboolean italic)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->is_italic == italic)
		return;

	wk_editor->priv->is_italic = italic;

	webkit_content_editor_set_format_boolean (
		wk_editor, "DOMSelectionSetItalic", italic);
}

static gboolean
webkit_content_editor_is_italic (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_italic;
}

static void
webkit_content_editor_set_monospaced (EWebKitContentEditor *wk_editor,
                                      gboolean monospaced)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->is_monospaced == monospaced)
		return;

	wk_editor->priv->is_monospaced = monospaced;

	webkit_content_editor_set_format_boolean (
		wk_editor, "DOMSelectionSetMonospaced", monospaced);
}

static gboolean
webkit_content_editor_is_monospaced (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_monospaced;
}

static void
webkit_content_editor_set_strikethrough (EWebKitContentEditor *wk_editor,
                                         gboolean strikethrough)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->is_strikethrough == strikethrough)
		return;

	wk_editor->priv->is_strikethrough = strikethrough;

	webkit_content_editor_set_format_boolean (
		wk_editor, "DOMSelectionSetStrikethrough", strikethrough);
}

static gboolean
webkit_content_editor_is_strikethrough (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_strikethrough;
}

static void
webkit_content_editor_set_subscript (EWebKitContentEditor *wk_editor,
                                     gboolean subscript)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->is_subscript == subscript)
		return;

	wk_editor->priv->is_subscript = subscript;

	webkit_content_editor_set_format_boolean (
		wk_editor, "DOMSelectionSetSubscript", subscript);
}

static gboolean
webkit_content_editor_is_subscript (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_subscript;
}

static void
webkit_content_editor_set_superscript (EWebKitContentEditor *wk_editor,
                                       gboolean superscript)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->is_superscript == superscript)
		return;

	wk_editor->priv->is_superscript = superscript;

	webkit_content_editor_set_format_boolean (
		wk_editor, "DOMSelectionSetSuperscript", superscript);
}

static gboolean
webkit_content_editor_is_superscript (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_superscript;
}

static void
webkit_content_editor_set_underline (EWebKitContentEditor *wk_editor,
                                     gboolean underline)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->is_underline == underline)
		return;

	wk_editor->priv->is_underline = underline;

	webkit_content_editor_set_format_boolean (
		wk_editor, "DOMSelectionSetUnderline", underline);
}

static gboolean
webkit_content_editor_is_underline (EWebKitContentEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_underline;
}

static void
webkit_content_editor_page_set_text_color (EContentEditor *editor,
                                           const GdkRGBA *value)
{
	EWebKitContentEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	webkit_content_editor_set_element_attribute (wk_editor, "body", "text", color);

	g_free (color);
}

static void
webkit_content_editor_page_get_text_color (EContentEditor *editor,
                                           GdkRGBA *color)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto theme;

	result = webkit_content_editor_get_element_attribute (wk_editor, "body", "text");
	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value || !gdk_rgba_parse (color, value)) {
			g_variant_unref (result);
			goto theme;
		}
		g_variant_unref (result);
		return;
	}

 theme:
	e_utils_get_theme_color (
		GTK_WIDGET (wk_editor),
		"theme_text_color",
		E_UTILS_DEFAULT_THEME_TEXT_COLOR,
		color);
}

static void
webkit_content_editor_page_set_background_color (EContentEditor *editor,
                                                 const GdkRGBA *value)
{
	EWebKitContentEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (value->alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (value));
	else
		color = g_strdup ("");

	webkit_content_editor_set_element_attribute (wk_editor, "body", "bgcolor", color);

	g_free (color);
}

static void
webkit_content_editor_page_get_background_color (EContentEditor *editor,
                                                 GdkRGBA *color)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto theme;

	result = webkit_content_editor_get_element_attribute (wk_editor, "body", "bgcolor");
	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value || !gdk_rgba_parse (color, value)) {
			g_variant_unref (result);
			goto theme;
		}
		g_variant_unref (result);
		return;
	}

 theme:
	e_utils_get_theme_color (
		GTK_WIDGET (wk_editor),
		"theme_base_color",
		E_UTILS_DEFAULT_THEME_BASE_COLOR,
		color);
}

static void
webkit_content_editor_page_set_link_color (EContentEditor *editor,
                                           const GdkRGBA *value)
{
	EWebKitContentEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	webkit_content_editor_set_element_attribute (wk_editor, "body", "link", color);

	g_free (color);
}

static void
webkit_content_editor_page_get_link_color (EContentEditor *editor,
                                           GdkRGBA *color)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto theme;

	result = webkit_content_editor_get_element_attribute (wk_editor, "body", "link");
	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value || !gdk_rgba_parse (color, value)) {
			g_variant_unref (result);
			goto theme;
		}
		g_variant_unref (result);
		return;
	}

 theme:
	color->alpha = 1;
	color->red = 0;
	color->green = 0;
	color->blue = 1;
}

static void
webkit_content_editor_page_set_visited_link_color (EContentEditor *editor,
                                                   const GdkRGBA *value)
{
	EWebKitContentEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	webkit_content_editor_set_element_attribute (wk_editor, "body", "vlink", color);

	g_free (color);
}

static void
webkit_content_editor_page_get_visited_link_color (EContentEditor *editor,
                                                   GdkRGBA *color)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto theme;

	result = webkit_content_editor_get_element_attribute (wk_editor, "body", "vlink");
	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value || !gdk_rgba_parse (color, value)) {
			g_variant_unref (result);
			goto theme;
		}
		g_variant_unref (result);
		return;
	}

 theme:
	color->alpha = 1;
	color->red = 1;
	color->green = 0;
	color->blue = 0;
}

static void
webkit_content_editor_on_page_dialog_open (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorPageDialogSaveHistory");
}

static void
webkit_content_editor_on_page_dialog_close (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorPageDialogSaveHistoryOnExit");
}

static gchar *
webkit_content_editor_page_get_background_image_uri (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_content_editor_get_element_attribute (wk_editor, "body", "data-uri");
	if (result) {
		gchar *value;

		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return NULL;
}

static void
webkit_content_editor_page_set_background_image_uri (EContentEditor *editor,
                                                     const gchar *uri)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (uri && *uri)
		webkit_content_editor_replace_image_src (wk_editor, "body", uri);
	else {
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new ("(ts)", current_page_id (wk_editor), "body"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	}
}

static void
webkit_content_editor_on_cell_dialog_open (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogMarkCurrentCellElement",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-table-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_on_cell_dialog_close (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorCellDialogSaveHistoryOnExit");
}

static void
webkit_content_editor_cell_set_v_align (EContentEditor *editor,
                                        const gchar *value,
                                        EContentEditorScope scope)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogSetElementVAlign",
		g_variant_new ("(tsi)", current_page_id (wk_editor), value, (gint32) scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gchar *
webkit_content_editor_cell_get_v_align (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-cell", "valign");
	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_cell_set_align (EContentEditor *editor,
                                      const gchar *value,
                                      EContentEditorScope scope)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogSetElementAlign",
		g_variant_new ("(tsi)", current_page_id (wk_editor), value, (gint32) scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gchar *
webkit_content_editor_cell_get_align (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-cell", "align");
	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_cell_set_wrap (EContentEditor *editor,
                                     gboolean value,
                                     EContentEditorScope scope)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogSetElementNoWrap",
		g_variant_new ("(tbi)", current_page_id (wk_editor), !value, (gint32) scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gboolean
webkit_content_editor_cell_get_wrap (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gboolean value = FALSE;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return FALSE;

	if (!wk_editor->priv->web_extension)
		return FALSE;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"TableCellElementGetNoWrap",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &value);
		value = !value;
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_cell_set_header_style (EContentEditor *editor,
                                             gboolean value,
                                             EContentEditorScope scope)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	if (!wk_editor->priv->html_mode)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogSetElementHeaderStyle",
		g_variant_new ("(tbi)", current_page_id (wk_editor), value, (gint32) scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gboolean
webkit_content_editor_cell_is_header (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gboolean value = FALSE;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return FALSE;

	if (!wk_editor->priv->web_extension)
		return FALSE;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"ElementGetTagName",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		const gchar *tag_name;

		g_variant_get (result, "(&s)", &tag_name);
		value = g_ascii_strncasecmp (tag_name, "TH", 2) == 0;
		g_variant_unref (result);
	}

	return value;
}

static gint
webkit_content_editor_cell_get_width (EContentEditor *editor,
                                      EContentEditorUnit *unit)
{
	EWebKitContentEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	*unit = E_CONTENT_EDITOR_UNIT_AUTO;

	if (!wk_editor->priv->html_mode)
		return 0;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-cell", "width");

	if (result) {
		const gchar *width;

		g_variant_get (result, "(&s)", &width);
		if (width && *width) {
			value = atoi (width);
			if (strstr (width, "%"))
				*unit = E_CONTENT_EDITOR_UNIT_PERCENTAGE;
			else if (g_ascii_strncasecmp (width, "auto", 4) != 0)
				*unit = E_CONTENT_EDITOR_UNIT_PIXEL;
		}
		g_object_unref (result);
	}

	return value;
}

static gint
webkit_content_editor_cell_get_row_span (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return 0;

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"TableCellElementGetRowSpan",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gint
webkit_content_editor_cell_get_col_span (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return 0;

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"TableCellElementGetColSpan",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gchar *
webkit_content_editor_cell_get_background_image_uri (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-cell", "data-uri");
	if (result) {
		gchar *value;

		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return NULL;
}

static void
webkit_content_editor_cell_get_background_color (EContentEditor *editor,
                                                 GdkRGBA *color)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto exit;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-cell", "bgcolor");
	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value || !gdk_rgba_parse (color, value)) {
			g_variant_unref (result);
			goto exit;
		}
		g_variant_unref (result);
		return;
	}

 exit:
	*color = transparent;
}

static void
webkit_content_editor_cell_set_row_span (EContentEditor *editor,
                                         gint value,
                                         EContentEditorScope scope)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogSetElementRowSpan",
		g_variant_new ("(tii)", current_page_id (wk_editor), value, (gint32) scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_cell_set_col_span (EContentEditor *editor,
                                         gint value,
                                         EContentEditorScope scope)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogSetElementColSpan",
		g_variant_new ("(tii)", current_page_id (wk_editor), value, (gint32) scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
webkit_content_editor_cell_set_width (EContentEditor *editor,
                                      gint value,
                                      EContentEditorUnit unit,
                                      EContentEditorScope scope)
{
	EWebKitContentEditor *wk_editor;
	gchar *width;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	if (unit == E_CONTENT_EDITOR_UNIT_AUTO)
		width = g_strdup ("auto");
	else
		width = g_strdup_printf (
			"%d%s",
			value,
			(unit == E_CONTENT_EDITOR_UNIT_PIXEL) ? "px" : "%");

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogSetElementWidth",
		g_variant_new ("(tsi)", current_page_id (wk_editor), width, (gint32) scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_free (width);
}

static void
webkit_content_editor_cell_set_background_color (EContentEditor *editor,
                                                 const GdkRGBA *value,
                                                 EContentEditorScope scope)
{
	EWebKitContentEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	if (value->alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (value));
	else
		color = g_strdup ("");

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorCellDialogSetElementBgColor",
		g_variant_new ("(tsi)", current_page_id (wk_editor), color, (gint32) scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_free (color);
}

static void
webkit_content_editor_cell_set_background_image_uri (EContentEditor *editor,
                                                     const gchar *uri)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	if (!wk_editor->priv->html_mode)
		return;

	if (uri && *uri)
		webkit_content_editor_replace_image_src (wk_editor, "#-x-evo-current-cell", uri);
	else {
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new ("(ts)", current_page_id (wk_editor), "#-x-evo-current-cell"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	}
}

static void
webkit_content_editor_table_set_row_count (EContentEditor *editor,
                                           guint value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorTableDialogSetRowCount",
		g_variant_new ("(tu)", current_page_id (wk_editor), value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static guint
webkit_content_editor_table_get_row_count (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	guint value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return 0;

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"EHTMLEditorTableDialogGetRowCount",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(u)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_table_set_column_count (EContentEditor *editor,
                                              guint value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"EHTMLEditorTableDialogSetColumnCount",
		g_variant_new ("(tu)", current_page_id (wk_editor), value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static guint
webkit_content_editor_table_get_column_count (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	guint value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return 0;

	if (!wk_editor->priv->web_extension)
		return 0;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"EHTMLEditorTableDialogGetColumnCount",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(u)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_table_set_width (EContentEditor *editor,
                                       gint value,
                                       EContentEditorUnit unit)
{
	EWebKitContentEditor *wk_editor;
	gchar *width;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension)
		return;

	if (unit == E_CONTENT_EDITOR_UNIT_AUTO)
		width = g_strdup ("auto");
	else
		width = g_strdup_printf (
			"%d%s",
			value,
			(unit == E_CONTENT_EDITOR_UNIT_PIXEL) ? "px" : "%");

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "width", width);

	g_free (width);
}

static guint
webkit_content_editor_table_get_width (EContentEditor *editor,
                                       EContentEditorUnit *unit)
{
	EWebKitContentEditor *wk_editor;
	guint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	*unit = E_CONTENT_EDITOR_UNIT_PIXEL;

	if (!wk_editor->priv->html_mode)
		return 0;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-table", "width");

	if (result) {
		const gchar *width;

		g_variant_get (result, "(&s)", &width);
		if (width && *width) {
			value = atoi (width);
			if (strstr (width, "%"))
				*unit = E_CONTENT_EDITOR_UNIT_PERCENTAGE;
			else if (g_ascii_strncasecmp (width, "auto", 4) != 0)
				*unit = E_CONTENT_EDITOR_UNIT_PIXEL;
		}
		g_object_unref (result);
	}

	return value;
}

static void
webkit_content_editor_table_set_align (EContentEditor *editor,
                                       const gchar *value)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "align", value);
}

static gchar *
webkit_content_editor_table_get_align (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-table", "align");
	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_table_set_padding (EContentEditor *editor,
                                         gint value)
{
	EWebKitContentEditor *wk_editor;
	gchar *padding;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	padding = g_strdup_printf ("%d", value);

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "cellpadding", padding);

	g_free (padding);
}

static gint
webkit_content_editor_table_get_padding (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-table", "cellpadding");

	if (result) {
		const gchar *padding;

		g_variant_get (result, "(&s)", &padding);
		if (padding && *padding)
			value = atoi (padding);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_table_set_spacing (EContentEditor *editor,
                                         gint value)
{
	EWebKitContentEditor *wk_editor;
	gchar *spacing;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	spacing = g_strdup_printf ("%d", value);

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "cellspacing", spacing);

	g_free (spacing);
}

static gint
webkit_content_editor_table_get_spacing (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-table", "cellspacing");

	if (result) {
		const gchar *spacing;

		g_variant_get (result, "(&s)", &spacing);
		if (spacing && *spacing)
			value = atoi (spacing);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_table_set_border (EContentEditor *editor,
                                        gint value)
{
	EWebKitContentEditor *wk_editor;
	gchar *border;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	border = g_strdup_printf ("%d", value);

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "border", border);

	g_free (border);
}

static gint
webkit_content_editor_table_get_border (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-table", "border");

	if (result) {
		const gchar *border;

		g_variant_get (result, "(&s)", &border);
		if (border && *border)
			value = atoi (border);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_table_get_background_color (EContentEditor *editor,
                                                 GdkRGBA *color)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto exit;

	result = webkit_content_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-table", "bgcolor");
	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value || !gdk_rgba_parse (color, value)) {
			g_variant_unref (result);
			goto exit;
		}
		g_variant_unref (result);
		return;
	}

 exit:
	*color = transparent;
}

static void
webkit_content_editor_table_set_background_color (EContentEditor *editor,
                                                  const GdkRGBA *value)
{
	EWebKitContentEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	if (value->alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (value));
	else
		color = g_strdup ("");

	webkit_content_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "bgcolor", color);

	g_free (color);
}

static gchar *
webkit_content_editor_table_get_background_image_uri (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_content_editor_get_element_attribute (wk_editor, "#-x-evo-current-table", "data-uri");
	if (result) {
		gchar *value;

		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return NULL;
}

static void
webkit_content_editor_table_set_background_image_uri (EContentEditor *editor,
                                                     const gchar *uri)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return;

	if (!wk_editor->priv->html_mode)
		return;

	if (uri && *uri)
		webkit_content_editor_replace_image_src (wk_editor, "#-x-evo-current-table", uri);
	else {
		g_dbus_proxy_call (
			wk_editor->priv->web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new ("(ts)", current_page_id (wk_editor), "#-x-evo-current-table"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	}
}

static gboolean
webkit_content_editor_on_table_dialog_open (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;
	GVariant *result;
	gboolean value = FALSE;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return FALSE;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		"EHTMLEditorTableDialogShow",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_content_editor_on_table_dialog_close (EContentEditor *editor)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	webkit_content_editor_call_simple_extension_function (
		wk_editor, "EHTMLEditorTableDialogSaveHistoryOnExit");

	webkit_content_editor_finish_search (E_WEBKIT_CONTENT_EDITOR (editor));
}

static void
webkit_content_editor_on_spell_check_dialog_open (EContentEditor *editor)
{
}

static void
webkit_content_editor_on_spell_check_dialog_close (EContentEditor *editor)
{
	webkit_content_editor_finish_search (E_WEBKIT_CONTENT_EDITOR (editor));
}

static gchar *
move_to_another_word (EContentEditor *editor,
                      const gchar *word,
                      const gchar *dom_function)
{
	EWebKitContentEditor *wk_editor;
	gchar **active_languages;
	gchar *another_word = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	if (!wk_editor->priv->web_extension)
		return NULL;

	active_languages = e_spell_checker_list_active_languages (
		wk_editor->priv->spell_checker, NULL);
	if (!active_languages)
		return NULL;

	result = g_dbus_proxy_call_sync (
		wk_editor->priv->web_extension,
		dom_function,
		g_variant_new (
			"(ts^as)", current_page_id (wk_editor), word ? word : "", active_languages),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	g_strfreev (active_languages);

	if (result) {
		g_variant_get (result, "(s)", &another_word);
		g_variant_unref (result);
	}

	return another_word;
}

static gchar *
webkit_content_editor_spell_check_next_word (EContentEditor *editor,
                                             const gchar *word)
{
	return move_to_another_word (editor, word, "EHTMLEditorSpellCheckDialogNext");
}

static gchar *
webkit_content_editor_spell_check_prev_word (EContentEditor *editor,
                                             const gchar *word)
{
	return move_to_another_word (editor, word, "EHTMLEditorSpellCheckDialogPrev");
}

static void
webkit_content_editor_on_replace_dialog_open (EContentEditor *editor)
{
}

static void
webkit_content_editor_on_replace_dialog_close (EContentEditor *editor)
{
	webkit_content_editor_finish_search (E_WEBKIT_CONTENT_EDITOR (editor));
}

static void
webkit_content_editor_on_find_dialog_open (EContentEditor *editor)
{
}

static void
webkit_content_editor_on_find_dialog_close (EContentEditor *editor)
{
	webkit_content_editor_finish_search (E_WEBKIT_CONTENT_EDITOR (editor));
}

static void
webkit_content_editor_constructed (GObject *object)
{
	EWebKitContentEditor *wk_editor;
	gchar **languages;
	WebKitWebContext *web_context;
	WebKitSettings *web_settings;
	WebKitWebView *web_view;

	G_OBJECT_CLASS (e_webkit_content_editor_parent_class)->constructed (object);

	wk_editor = E_WEBKIT_CONTENT_EDITOR (object);
	web_view = WEBKIT_WEB_VIEW (wk_editor);

	/* Give spell check languages to WebKit */
	languages = e_spell_checker_list_active_languages (wk_editor->priv->spell_checker, NULL);

	web_context = webkit_web_view_get_context (web_view);
	webkit_web_context_set_spell_checking_enabled (web_context, TRUE);
	webkit_web_context_set_spell_checking_languages (web_context, (const gchar * const *) languages);
	g_strfreev (languages);

	webkit_web_view_set_editable (web_view, TRUE);

	web_settings = webkit_web_view_get_settings (web_view);
	webkit_settings_set_allow_file_access_from_file_urls (web_settings, TRUE);

	/* Make WebKit think we are displaying a local file, so that it
	 * does not block loading resources from file:// protocol */
	webkit_web_view_load_html (WEBKIT_WEB_VIEW (object), "", "file://");
}

static GObjectConstructParam*
find_property (guint n_properties,
               GObjectConstructParam* properties,
               GParamSpec* param_spec)
{
	while (n_properties--) {
		if (properties->pspec == param_spec)
			return properties;
		properties++;
	}

	return NULL;
}

static GObject *
webkit_content_editor_constructor (GType type,
                                   guint n_construct_properties,
                                   GObjectConstructParam *construct_properties)
{
	GObjectClass* object_class;
	GParamSpec* param_spec;
	GObjectConstructParam *param = NULL;

	object_class = G_OBJECT_CLASS (g_type_class_ref (type));
	g_return_val_if_fail (object_class != NULL, NULL);

	if (construct_properties && n_construct_properties != 0) {
		param_spec = g_object_class_find_property (object_class, "settings");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, e_web_view_get_default_webkit_settings ());
		param_spec = g_object_class_find_property (object_class, "user-content-manager");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, webkit_user_content_manager_new ());
		param_spec = g_object_class_find_property (object_class, "web-context");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec))) {
			WebKitWebContext *web_context;

			web_context = webkit_web_context_new ();

			webkit_web_context_set_cache_model (
				web_context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

			webkit_web_context_set_web_extensions_directory (
				web_context, EVOLUTION_WEB_EXTENSIONS_CONTENT_EDITOR_DIR);

			g_value_take_object (param->value, web_context);
		}
	}

	g_type_class_unref (object_class);

	return G_OBJECT_CLASS (e_webkit_content_editor_parent_class)->constructor (type, n_construct_properties, construct_properties);
}

static void
webkit_content_editor_dispose (GObject *object)
{
	EWebKitContentEditorPrivate *priv;

	priv = E_WEBKIT_CONTENT_EDITOR_GET_PRIVATE (object);

	if (priv->aliasing_settings != NULL) {
		g_signal_handlers_disconnect_by_data (priv->aliasing_settings, object);
		g_object_unref (priv->aliasing_settings);
		priv->aliasing_settings = NULL;
	}

	if (priv->current_user_stylesheet != NULL) {
		g_free (priv->current_user_stylesheet);
		priv->current_user_stylesheet = NULL;
	}

	if (priv->font_settings != NULL) {
		g_signal_handlers_disconnect_by_data (priv->font_settings, object);
		g_object_unref (priv->font_settings);
		priv->font_settings = NULL;
	}

	if (priv->mail_settings != NULL) {
		g_signal_handlers_disconnect_by_data (priv->mail_settings, object);
		g_object_unref (priv->mail_settings);
		priv->mail_settings = NULL;
	}

	if (priv->web_extension_content_changed_cb_id > 0) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (priv->web_extension),
			priv->web_extension_content_changed_cb_id);
		priv->web_extension_content_changed_cb_id = 0;
	}

	if (priv->web_extension_selection_changed_cb_id > 0) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (priv->web_extension),
			priv->web_extension_selection_changed_cb_id);
		priv->web_extension_selection_changed_cb_id = 0;
	}

	if (priv->web_extension_watch_name_id > 0) {
		g_bus_unwatch_name (priv->web_extension_watch_name_id);
		priv->web_extension_watch_name_id = 0;
	}

	if (priv->owner_change_clipboard_cb_id > 0) {
		g_signal_handler_disconnect (
			gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
			priv->owner_change_clipboard_cb_id);
		priv->owner_change_clipboard_cb_id = 0;
	}

	if (priv->owner_change_primary_clipboard_cb_id > 0) {
		g_signal_handler_disconnect (
			gtk_clipboard_get (GDK_SELECTION_PRIMARY),
			priv->owner_change_primary_clipboard_cb_id);
		priv->owner_change_primary_clipboard_cb_id = 0;
	}

	webkit_content_editor_finish_search (E_WEBKIT_CONTENT_EDITOR (object));

	g_clear_object (&priv->web_extension);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_webkit_content_editor_parent_class)->dispose (object);
}

static void
webkit_content_editor_finalize (GObject *object)
{
	EWebKitContentEditorPrivate *priv;

	priv = E_WEBKIT_CONTENT_EDITOR_GET_PRIVATE (object);

	if (priv->old_settings) {
		g_hash_table_destroy (priv->old_settings);
		priv->old_settings = NULL;
	}

	if (priv->post_reload_operations) {
		g_warn_if_fail (g_queue_is_empty (priv->post_reload_operations));

		g_queue_free (priv->post_reload_operations);
		priv->post_reload_operations = NULL;
	}

	if (priv->background_color != NULL) {
		gdk_rgba_free (priv->background_color);
		priv->background_color = NULL;
	}

	if (priv->font_color != NULL) {
		gdk_rgba_free (priv->font_color);
		priv->font_color = NULL;
	}

	g_clear_object (&priv->spell_checker);

	g_free (priv->font_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_webkit_content_editor_parent_class)->finalize (object);
}

static void
webkit_content_editor_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			webkit_content_editor_set_changed (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_EDITABLE:
			webkit_content_editor_set_editable (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_HTML_MODE:
			webkit_content_editor_set_html_mode (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_ALIGNMENT:
			webkit_content_editor_set_alignment (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_BACKGROUND_COLOR:
			webkit_content_editor_set_background_color (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boxed (value));
			return;

		case PROP_BOLD:
			webkit_content_editor_set_bold (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_FONT_COLOR:
			webkit_content_editor_set_font_color (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boxed (value));
			return;

		case PROP_BLOCK_FORMAT:
			webkit_content_editor_set_block_format (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_FONT_NAME:
			webkit_content_editor_set_font_name (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_FONT_SIZE:
			webkit_content_editor_set_font_size (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_ITALIC:
			webkit_content_editor_set_italic (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_MONOSPACED:
			webkit_content_editor_set_monospaced (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_STRIKETHROUGH:
			webkit_content_editor_set_strikethrough (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SUBSCRIPT:
			webkit_content_editor_set_subscript (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SUPERSCRIPT:
			webkit_content_editor_set_superscript (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_UNDERLINE:
			webkit_content_editor_set_underline (
				E_WEBKIT_CONTENT_EDITOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
webkit_content_editor_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_COPY:
			g_value_set_boolean (
				value, webkit_content_editor_can_copy (
				E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_CAN_CUT:
			g_value_set_boolean (
				value, webkit_content_editor_can_cut (
				E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_CAN_PASTE:
			g_value_set_boolean (
				value, webkit_content_editor_can_paste (
				E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, webkit_content_editor_can_redo (
				E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, webkit_content_editor_can_undo (
				E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value, webkit_content_editor_get_changed (
				E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_HTML_MODE:
			g_value_set_boolean (
				value, webkit_content_editor_get_html_mode (
				E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value, webkit_content_editor_is_editable (
				E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_ALIGNMENT:
			g_value_set_enum (
				value,
				webkit_content_editor_get_alignment (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_BACKGROUND_COLOR:
			g_value_set_boxed (
				value,
				webkit_content_editor_get_background_color (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_BLOCK_FORMAT:
			g_value_set_enum (
				value,
				webkit_content_editor_get_block_format (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_BOLD:
			g_value_set_boolean (
				value,
				webkit_content_editor_is_bold (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_FONT_COLOR:
			g_value_set_boxed (
				value,
				webkit_content_editor_get_font_color (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_FONT_NAME:
			g_value_set_string (
				value,
				webkit_content_editor_get_font_name (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_FONT_SIZE:
			g_value_set_int (
				value,
				webkit_content_editor_get_font_size (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_INDENTED:
			g_value_set_boolean (
				value,
				webkit_content_editor_selection_is_indented (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_ITALIC:
			g_value_set_boolean (
				value,
				webkit_content_editor_is_italic (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_MONOSPACED:
			g_value_set_boolean (
				value,
				webkit_content_editor_is_monospaced (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_STRIKETHROUGH:
			g_value_set_boolean (
				value,
				webkit_content_editor_is_strikethrough (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_SUBSCRIPT:
			g_value_set_boolean (
				value,
				webkit_content_editor_is_subscript (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_SUPERSCRIPT:
			g_value_set_boolean (
				value,
				webkit_content_editor_is_superscript (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_UNDERLINE:
			g_value_set_boolean (
				value,
				webkit_content_editor_is_underline (
					E_WEBKIT_CONTENT_EDITOR (object)));
			return;

		case PROP_SPELL_CHECKER:
			g_value_set_object (
				value,
				webkit_content_editor_get_spell_checker (
					E_CONTENT_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
webkit_content_editor_move_caret_on_current_coordinates (GtkWidget *widget)
{
	gint x, y;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (
		gtk_widget_get_window (widget), pointer, &x, &y, NULL);
	webkit_content_editor_move_caret_on_coordinates
		(E_CONTENT_EDITOR (widget), x, y, TRUE);
}

static void
webkit_content_editor_settings_changed_cb (GSettings *settings,
                                           const gchar *key,
                                           EWebKitContentEditor *wk_editor)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (wk_editor->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (wk_editor->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (wk_editor->priv->old_settings, key);

		webkit_content_editor_update_styles (E_CONTENT_EDITOR (wk_editor));
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static void
webkit_content_editor_load_changed_cb (EWebKitContentEditor *wk_editor,
                                       WebKitLoadEvent load_event)
{
	wk_editor->priv->webkit_load_event = load_event;

	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	if (wk_editor->priv->web_extension)
		e_content_editor_emit_load_finished (E_CONTENT_EDITOR (wk_editor));
	else
		wk_editor->priv->emit_load_finished_when_extension_is_ready = TRUE;

	dispatch_pending_operations (wk_editor);

	wk_editor->priv->reload_in_progress = FALSE;
}

static void
webkit_content_editor_clipboard_owner_change_cb (GtkClipboard *clipboard,
                                                 GdkEventOwnerChange *event,
                                                 EWebKitContentEditor *wk_editor)
{
	if (!E_IS_WEBKIT_CONTENT_EDITOR (wk_editor))
		return;

	if (wk_editor->priv->copy_cut_actions_triggered && event->owner)
		wk_editor->priv->copy_paste_clipboard_in_view = TRUE;
	else
		wk_editor->priv->copy_paste_clipboard_in_view = FALSE;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"SetPastingContentFromItself",
		g_variant_new (
			"(tb)",
			current_page_id (wk_editor),
			wk_editor->priv->copy_paste_clipboard_in_view),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	wk_editor->priv->copy_cut_actions_triggered = FALSE;
}

static void
webkit_content_editor_primary_clipboard_owner_change_cb (GtkClipboard *clipboard,
                                                         GdkEventOwnerChange *event,
                                                         EWebKitContentEditor *wk_editor)
{
	if (!E_IS_WEBKIT_CONTENT_EDITOR (wk_editor))
		return;

	if (!event->owner || !wk_editor->priv->can_copy)
		wk_editor->priv->copy_paste_clipboard_in_view = FALSE;

	g_dbus_proxy_call (
		wk_editor->priv->web_extension,
		"SetPastingContentFromItself",
		g_variant_new (
			"(tb)",
			current_page_id (wk_editor),
			wk_editor->priv->copy_paste_clipboard_in_view),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static gboolean
webkit_content_editor_paste_prefer_text_html (EWebKitContentEditor *wk_editor)
{
	if (wk_editor->priv->pasting_primary_clipboard)
		return wk_editor->priv->copy_paste_primary_in_view;
	else
		return wk_editor->priv->copy_paste_clipboard_in_view;
}

static void
webkit_content_editor_paste_clipboard_targets_cb (GtkClipboard *clipboard,
                                                  GdkAtom *targets,
                                                  gint n_targets,
                                                  EWebKitContentEditor *wk_editor)
{
	if (targets == NULL || n_targets < 0)
		return;

	/* If view doesn't have focus, focus it */
	if (!gtk_widget_has_focus (GTK_WIDGET (wk_editor)))
		gtk_widget_grab_focus (GTK_WIDGET (wk_editor));

	/* Order is important here to ensure common use cases are
	 * handled correctly.  See GNOME bug #603715 for details. */
	/* Prefer plain text over HTML when in the plain text mode, but only
	 * when pasting content from outside the editor view. */
	if (wk_editor->priv->html_mode ||
	    webkit_content_editor_paste_prefer_text_html (wk_editor)) {
		gchar *content = NULL;

		if (e_targets_include_html (targets, n_targets)) {
			if (!(content = e_clipboard_wait_for_html (clipboard)))
				return;

			webkit_content_editor_insert_content (
				E_CONTENT_EDITOR (wk_editor),
				content,
				E_CONTENT_EDITOR_INSERT_TEXT_HTML);

			g_free (content);
			return;
		}

		if (gtk_targets_include_text (targets, n_targets)) {
			if (!(content = gtk_clipboard_wait_for_text (clipboard)))
				return;

			webkit_content_editor_insert_content (
				E_CONTENT_EDITOR (wk_editor),
				content,
				E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
				E_CONTENT_EDITOR_INSERT_CONVERT);

			g_free (content);
			return;
		}
	} else {
		gchar *content = NULL;

		if (gtk_targets_include_text (targets, n_targets)) {
			if (!(content = gtk_clipboard_wait_for_text (clipboard)))
				return;

			webkit_content_editor_insert_content (
				E_CONTENT_EDITOR (wk_editor),
				content,
				E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
				E_CONTENT_EDITOR_INSERT_CONVERT);

			g_free (content);
			return;
		}

		if (e_targets_include_html (targets, n_targets)) {
			if (!(content = e_clipboard_wait_for_html (clipboard)))
				return;

			webkit_content_editor_insert_content (
				E_CONTENT_EDITOR (wk_editor),
				content,
				E_CONTENT_EDITOR_INSERT_TEXT_HTML);

			g_free (content);
			return;
		}
	}

	if (gtk_targets_include_image (targets, n_targets, TRUE)) {
		gchar *uri;

		if (!(uri = e_util_save_image_from_clipboard (clipboard)))
			return;

		webkit_content_editor_insert_image (E_CONTENT_EDITOR (wk_editor), uri);

		g_free (uri);

		return;
	}
}

static void
webkit_content_editor_paste_primary (EContentEditor *editor)
{

	GtkClipboard *clipboard;
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	/* Remember, that we are pasting primary clipboard to return
	 * correct value in e_html_editor_view_is_pasting_content_from_itself. */
	wk_editor->priv->pasting_primary_clipboard = TRUE;

	webkit_content_editor_move_caret_on_current_coordinates (GTK_WIDGET (wk_editor));

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

	gtk_clipboard_request_targets (
		clipboard, (GtkClipboardTargetsReceivedFunc)
		webkit_content_editor_paste_clipboard_targets_cb, wk_editor);
}

static void
webkit_content_editor_paste (EContentEditor *editor)
{
	GtkClipboard *clipboard;
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (editor);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_targets (
		clipboard, (GtkClipboardTargetsReceivedFunc)
		webkit_content_editor_paste_clipboard_targets_cb, wk_editor);
}

static void
webkit_content_editor_mouse_target_changed_cb (EWebKitContentEditor *wk_editor,
                                               WebKitHitTestResult *hit_test_result,
                                               guint modifiers,
                                               gpointer user_data)
{
	/* Ctrl + Left Click on link opens it. */
	if (webkit_hit_test_result_context_is_link (hit_test_result) &&
	    (modifiers & GDK_CONTROL_MASK)) {
		GdkScreen *screen;
		const gchar *uri;
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (wk_editor));
		screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

		uri = webkit_hit_test_result_get_link_uri (hit_test_result);

		gtk_show_uri (screen, uri, GDK_CURRENT_TIME, NULL);
	}
}

static gboolean
webkit_content_editor_context_menu_cb (EWebKitContentEditor *wk_editor,
                                       WebKitContextMenu *context_menu,
                                       GdkEvent *event,
                                       WebKitHitTestResult *hit_test_result)
{
	GVariant *result;
	EContentEditorNodeFlags flags = 0;
	gboolean handled;

	webkit_context_menu_remove_all (context_menu);

	if ((result = webkit_context_menu_get_user_data (context_menu)))
		flags = g_variant_get_int32 (result);

	handled = e_content_editor_emit_context_menu_requested (E_CONTENT_EDITOR (wk_editor), flags, event);

	return handled;
}

static void
webkit_content_editor_drag_end_cb (EWebKitContentEditor *editor,
                                   GdkDragContext *context)
{
	webkit_content_editor_call_simple_extension_function (editor, "DOMDragAndDropEnd");
}

static gboolean
webkit_content_editor_button_press_event (GtkWidget *widget,
                                          GdkEventButton *event)
{
	if (event->button == 2) {
		if (!e_content_editor_emit_paste_primary_clipboard (E_CONTENT_EDITOR (widget)))
			webkit_content_editor_paste_primary (E_CONTENT_EDITOR( (widget)));

		return TRUE;
	}

	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_webkit_content_editor_parent_class)->button_press_event (widget, event);
}

static gboolean
webkit_content_editor_key_press_event (GtkWidget *widget,
                                       GdkEventKey *event)
{
	EWebKitContentEditor *wk_editor;

	wk_editor = E_WEBKIT_CONTENT_EDITOR (widget);

	if ((((event)->state & GDK_SHIFT_MASK) &&
	    ((event)->keyval == GDK_KEY_Insert)) ||
	    (((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->keyval == GDK_KEY_v))) {
		if (!e_content_editor_emit_paste_clipboard (E_CONTENT_EDITOR (widget)))
			webkit_content_editor_paste (E_CONTENT_EDITOR (widget));

		return TRUE;
	}

	if (((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->keyval == GDK_KEY_Insert)) {
		webkit_content_editor_copy (E_CONTENT_EDITOR (wk_editor));
		return TRUE;
	}

	if (((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->keyval == GDK_KEY_z)) {
		webkit_content_editor_undo (E_CONTENT_EDITOR (wk_editor));
		return TRUE;
	}

	if (((event)->state & (GDK_CONTROL_MASK)) &&
	    ((event)->keyval == GDK_KEY_Z)) {
		webkit_content_editor_redo (E_CONTENT_EDITOR (wk_editor));
		return TRUE;
	}

	if (((event)->state & GDK_SHIFT_MASK) &&
	    ((event)->keyval == GDK_KEY_Delete)) {
		webkit_content_editor_cut (E_CONTENT_EDITOR (wk_editor));
		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_webkit_content_editor_parent_class)->key_press_event (widget, event);
}

static void
e_webkit_content_editor_class_init (EWebKitContentEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EWebKitContentEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = webkit_content_editor_constructed;
	object_class->constructor = webkit_content_editor_constructor;
	object_class->get_property = webkit_content_editor_get_property;
	object_class->set_property = webkit_content_editor_set_property;
	object_class->dispose = webkit_content_editor_dispose;
	object_class->finalize = webkit_content_editor_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = webkit_content_editor_button_press_event;
	widget_class->key_press_event = webkit_content_editor_key_press_event;

	g_object_class_override_property (
		object_class, PROP_CAN_COPY, "can-copy");
	g_object_class_override_property (
		object_class, PROP_CAN_CUT, "can-cut");
	g_object_class_override_property (
		object_class, PROP_CAN_PASTE, "can-paste");
	g_object_class_override_property (
		object_class, PROP_CAN_REDO, "can-redo");
	g_object_class_override_property (
		object_class, PROP_CAN_UNDO, "can-undo");
	g_object_class_override_property (
		object_class, PROP_CHANGED, "changed");
	g_object_class_override_property (
		object_class, PROP_HTML_MODE, "html-mode");
	g_object_class_override_property (
		object_class, PROP_EDITABLE, "editable");
	g_object_class_override_property (
		object_class, PROP_ALIGNMENT, "alignment");
	g_object_class_override_property (
		object_class, PROP_BACKGROUND_COLOR, "background-color");
	g_object_class_override_property (
		object_class, PROP_BLOCK_FORMAT, "block-format");
	g_object_class_override_property (
		object_class, PROP_BOLD, "bold");
	g_object_class_override_property (
		object_class, PROP_FONT_COLOR, "font-color");
	g_object_class_override_property (
		object_class, PROP_FONT_NAME, "font-name");
	g_object_class_override_property (
		object_class, PROP_FONT_SIZE, "font-size");
	g_object_class_override_property (
		object_class, PROP_INDENTED, "indented");
	g_object_class_override_property (
		object_class, PROP_ITALIC, "italic");
	g_object_class_override_property (
		object_class, PROP_MONOSPACED, "monospaced");
	g_object_class_override_property (
		object_class, PROP_STRIKETHROUGH, "strikethrough");
	g_object_class_override_property (
		object_class, PROP_SUBSCRIPT, "subscript");
	g_object_class_override_property (
		object_class, PROP_SUPERSCRIPT, "superscript");
	g_object_class_override_property (
		object_class, PROP_UNDERLINE, "underline");
	g_object_class_override_property (
		object_class, PROP_SPELL_CHECKER, "spell-checker");
}

static void
e_webkit_content_editor_init (EWebKitContentEditor *wk_editor)
{
	GSettings *g_settings;
	GSettingsSchema *settings_schema;

	wk_editor->priv = E_WEBKIT_CONTENT_EDITOR_GET_PRIVATE (wk_editor);

	wk_editor->priv->spell_checker = e_spell_checker_new ();
	wk_editor->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	webkit_content_editor_watch_web_extension (wk_editor);

	g_signal_connect (
		wk_editor, "load-changed",
		G_CALLBACK (webkit_content_editor_load_changed_cb), NULL);

	g_signal_connect (
		wk_editor, "context-menu",
		G_CALLBACK (webkit_content_editor_context_menu_cb), NULL);

	g_signal_connect (
		wk_editor, "mouse-target-changed",
		G_CALLBACK (webkit_content_editor_mouse_target_changed_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-end",
		G_CALLBACK (webkit_content_editor_drag_end_cb), NULL);

	wk_editor->priv->owner_change_primary_clipboard_cb_id = g_signal_connect (
		gtk_clipboard_get (GDK_SELECTION_PRIMARY), "owner-change",
		G_CALLBACK (webkit_content_editor_primary_clipboard_owner_change_cb), wk_editor);

	wk_editor->priv->owner_change_clipboard_cb_id = g_signal_connect (
		gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), "owner-change",
		G_CALLBACK (webkit_content_editor_clipboard_owner_change_cb), wk_editor);

	g_settings = e_util_ref_settings ("org.gnome.desktop.interface");
	g_signal_connect (
		g_settings, "changed::font-name",
		G_CALLBACK (webkit_content_editor_settings_changed_cb), wk_editor);
	g_signal_connect (
		g_settings, "changed::monospace-font-name",
		G_CALLBACK (webkit_content_editor_settings_changed_cb), wk_editor);
	wk_editor->priv->font_settings = g_settings;

	g_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	wk_editor->priv->mail_settings = g_settings;

	/* This schema is optional.  Use if available. */
	settings_schema = g_settings_schema_source_lookup (
		g_settings_schema_source_get_default (),
		"org.gnome.settings-daemon.plugins.xsettings", FALSE);
	if (settings_schema != NULL) {
		g_settings = e_util_ref_settings ("org.gnome.settings-daemon.plugins.xsettings");
		g_signal_connect (
			g_settings, "changed::antialiasing",
			G_CALLBACK (webkit_content_editor_settings_changed_cb), wk_editor);
		wk_editor->priv->aliasing_settings = g_settings;
	}

	wk_editor->priv->html_mode = FALSE;
	wk_editor->priv->changed = FALSE;
	wk_editor->priv->can_copy = FALSE;
	wk_editor->priv->can_cut = FALSE;
	wk_editor->priv->can_paste = FALSE;
	wk_editor->priv->can_undo = FALSE;
	wk_editor->priv->can_redo = FALSE;
	wk_editor->priv->copy_paste_clipboard_in_view = FALSE;
	wk_editor->priv->copy_paste_primary_in_view = FALSE;
	wk_editor->priv->copy_cut_actions_triggered = FALSE;
	wk_editor->priv->pasting_primary_clipboard = FALSE;
	wk_editor->priv->content_flags = 0;
	wk_editor->priv->current_user_stylesheet = NULL;
	wk_editor->priv->emit_load_finished_when_extension_is_ready = FALSE;

	wk_editor->priv->font_color = gdk_rgba_copy (&black);
	wk_editor->priv->font_color = gdk_rgba_copy (&white);
	wk_editor->priv->font_name = NULL;
	wk_editor->priv->font_size = E_CONTENT_EDITOR_FONT_SIZE_NORMAL;
	wk_editor->priv->block_format = E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH;
	wk_editor->priv->alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;

	wk_editor->priv->web_extension_selection_changed_cb_id = 0;
	wk_editor->priv->web_extension_content_changed_cb_id = 0;
}

static void
e_webkit_content_editor_content_editor_init (EContentEditorInterface *iface)
{
	iface->update_styles = webkit_content_editor_update_styles;
	iface->insert_content = webkit_content_editor_insert_content;
	iface->get_content = webkit_content_editor_get_content;
	iface->insert_image = webkit_content_editor_insert_image;
	iface->insert_image_from_mime_part = webkit_content_editor_insert_image_from_mime_part;
	iface->insert_emoticon = webkit_content_editor_insert_emoticon;
	iface->set_current_content_flags = webkit_content_editor_set_current_content_flags;
	iface->get_current_content_flags = webkit_content_editor_get_current_content_flags;
	iface->move_caret_on_coordinates = webkit_content_editor_move_caret_on_coordinates;
	iface->cut = webkit_content_editor_cut;
	iface->copy = webkit_content_editor_copy;
	iface->paste = webkit_content_editor_paste;
	iface->paste_primary = webkit_content_editor_paste_primary;
	iface->undo = webkit_content_editor_undo;
	iface->redo = webkit_content_editor_redo;
	iface->clear_undo_redo_history = webkit_content_editor_clear_undo_redo_history;
	iface->get_spell_checker = webkit_content_editor_get_spell_checker;
	iface->set_spell_checking_languages = webkit_content_editor_set_spell_checking_languages;
	iface->set_spell_check = webkit_content_editor_set_spell_check;
	iface->get_spell_check = webkit_content_editor_get_spell_check;
//	iface->get_selected_text = webkit_content_editor_get_selected_text; /* FIXME WK2 */
	iface->get_caret_word = webkit_content_editor_get_caret_word;
	iface->replace_caret_word = webkit_content_editor_replace_caret_word;
	iface->select_all = webkit_content_editor_select_all;
	iface->selection_indent = webkit_content_editor_selection_indent;
	iface->selection_unindent = webkit_content_editor_selection_unindent;
//	iface->create_link = webkit_content_editor_create_link; /* FIXME WK2 */
	iface->selection_unlink = webkit_content_editor_selection_unlink;
	iface->find = webkit_content_editor_find;
	iface->replace = webkit_content_editor_replace;
	iface->replace_all = webkit_content_editor_replace_all;
	iface->selection_save = webkit_content_editor_selection_save;
	iface->selection_restore = webkit_content_editor_selection_restore;
	iface->selection_wrap = webkit_content_editor_selection_wrap;
	iface->show_inspector = webkit_content_editor_show_inspector;
	iface->get_caret_position = webkit_content_editor_get_caret_position;
	iface->get_caret_offset = webkit_content_editor_get_caret_offset;
	iface->get_current_signature_uid =  webkit_content_editor_get_current_signature_uid;
	iface->is_ready = webkit_content_editor_is_ready;
	iface->insert_signature = webkit_content_editor_insert_signature;
	iface->delete_cell_contents = webkit_content_editor_delete_cell_contents;
	iface->delete_column = webkit_content_editor_delete_column;
	iface->delete_row = webkit_content_editor_delete_row;
	iface->delete_table = webkit_content_editor_delete_table;
	iface->insert_column_after = webkit_content_editor_insert_column_after;
	iface->insert_column_before = webkit_content_editor_insert_column_before;
	iface->insert_row_above = webkit_content_editor_insert_row_above;
	iface->insert_row_below = webkit_content_editor_insert_row_below;
	iface->on_h_rule_dialog_open = webkit_content_editor_on_h_rule_dialog_open;
	iface->on_h_rule_dialog_close = webkit_content_editor_on_h_rule_dialog_close;
	iface->h_rule_set_align = webkit_content_editor_h_rule_set_align;
	iface->h_rule_get_align = webkit_content_editor_h_rule_get_align;
	iface->h_rule_set_size = webkit_content_editor_h_rule_set_size;
	iface->h_rule_get_size = webkit_content_editor_h_rule_get_size;
	iface->h_rule_set_width = webkit_content_editor_h_rule_set_width;
	iface->h_rule_get_width = webkit_content_editor_h_rule_get_width;
	iface->h_rule_set_no_shade = webkit_content_editor_h_rule_set_no_shade;
	iface->h_rule_get_no_shade = webkit_content_editor_h_rule_get_no_shade;
	iface->on_image_dialog_open = webkit_content_editor_on_image_dialog_open;
	iface->on_image_dialog_close = webkit_content_editor_on_image_dialog_close;
	iface->image_set_src = webkit_content_editor_image_set_src;
	iface->image_get_src = webkit_content_editor_image_get_src;
	iface->image_set_alt = webkit_content_editor_image_set_alt;
	iface->image_get_alt = webkit_content_editor_image_get_alt;
	iface->image_set_url = webkit_content_editor_image_set_url;
	iface->image_get_url = webkit_content_editor_image_get_url;
	iface->image_set_vspace = webkit_content_editor_image_set_vspace;
	iface->image_get_vspace = webkit_content_editor_image_get_vspace;
	iface->image_set_hspace = webkit_content_editor_image_set_hspace;
	iface->image_get_hspace = webkit_content_editor_image_get_hspace;
	iface->image_set_border = webkit_content_editor_image_set_border;
	iface->image_get_border = webkit_content_editor_image_get_border;
	iface->image_set_align = webkit_content_editor_image_set_align;
	iface->image_get_align = webkit_content_editor_image_get_align;
	iface->image_get_natural_width = webkit_content_editor_image_get_natural_width;
	iface->image_get_natural_height = webkit_content_editor_image_get_natural_height;
	iface->image_set_height = webkit_content_editor_image_set_height;
	iface->image_set_width = webkit_content_editor_image_set_width;
	iface->image_set_height_follow = webkit_content_editor_image_set_height_follow;
	iface->image_set_width_follow = webkit_content_editor_image_set_width_follow;
	iface->image_get_width = webkit_content_editor_image_get_width;
	iface->image_get_height = webkit_content_editor_image_get_height;
	iface->link_set_values = webkit_content_editor_link_set_values;
	iface->link_get_values = webkit_content_editor_link_get_values;
	iface->page_set_text_color = webkit_content_editor_page_set_text_color;
	iface->page_get_text_color = webkit_content_editor_page_get_text_color;
	iface->page_set_background_color = webkit_content_editor_page_set_background_color;
	iface->page_get_background_color = webkit_content_editor_page_get_background_color;
	iface->page_set_link_color = webkit_content_editor_page_set_link_color;
	iface->page_get_link_color = webkit_content_editor_page_get_link_color;
	iface->page_set_visited_link_color = webkit_content_editor_page_set_visited_link_color;
	iface->page_get_visited_link_color = webkit_content_editor_page_get_visited_link_color;
	iface->page_set_background_image_uri = webkit_content_editor_page_set_background_image_uri;
	iface->page_get_background_image_uri = webkit_content_editor_page_get_background_image_uri;
	iface->on_page_dialog_open = webkit_content_editor_on_page_dialog_open;
	iface->on_page_dialog_close = webkit_content_editor_on_page_dialog_close;
	iface->on_cell_dialog_open = webkit_content_editor_on_cell_dialog_open;
	iface->on_cell_dialog_close = webkit_content_editor_on_cell_dialog_close;
	iface->cell_set_v_align = webkit_content_editor_cell_set_v_align;
	iface->cell_get_v_align = webkit_content_editor_cell_get_v_align;
	iface->cell_set_align = webkit_content_editor_cell_set_align;
	iface->cell_get_align = webkit_content_editor_cell_get_align;
	iface->cell_set_wrap = webkit_content_editor_cell_set_wrap;
	iface->cell_get_wrap = webkit_content_editor_cell_get_wrap;
	iface->cell_set_header_style = webkit_content_editor_cell_set_header_style;
	iface->cell_is_header = webkit_content_editor_cell_is_header;
	iface->cell_get_width = webkit_content_editor_cell_get_width;
	iface->cell_set_width = webkit_content_editor_cell_set_width;
	iface->cell_get_row_span = webkit_content_editor_cell_get_row_span;
	iface->cell_set_row_span = webkit_content_editor_cell_set_row_span;
	iface->cell_get_col_span = webkit_content_editor_cell_get_col_span;
	iface->cell_set_col_span = webkit_content_editor_cell_set_col_span;
	iface->cell_get_background_image_uri = webkit_content_editor_cell_get_background_image_uri;
	iface->cell_set_background_image_uri = webkit_content_editor_cell_set_background_image_uri;
	iface->cell_get_background_color = webkit_content_editor_cell_get_background_color;
	iface->cell_set_background_color = webkit_content_editor_cell_set_background_color;
	iface->table_set_row_count = webkit_content_editor_table_set_row_count;
	iface->table_get_row_count = webkit_content_editor_table_get_row_count;
	iface->table_set_column_count = webkit_content_editor_table_set_column_count;
	iface->table_get_column_count = webkit_content_editor_table_get_column_count;
	iface->table_set_width = webkit_content_editor_table_set_width;
	iface->table_get_width = webkit_content_editor_table_get_width;
	iface->table_set_align = webkit_content_editor_table_set_align;
	iface->table_get_align = webkit_content_editor_table_get_align;
	iface->table_set_padding = webkit_content_editor_table_set_padding;
	iface->table_get_padding = webkit_content_editor_table_get_padding;
	iface->table_set_spacing = webkit_content_editor_table_set_spacing;
	iface->table_get_spacing = webkit_content_editor_table_get_spacing;
	iface->table_set_border = webkit_content_editor_table_set_border;
	iface->table_get_border = webkit_content_editor_table_get_border;
	iface->table_get_background_image_uri = webkit_content_editor_table_get_background_image_uri;
	iface->table_set_background_image_uri = webkit_content_editor_table_set_background_image_uri;
	iface->table_get_background_color = webkit_content_editor_table_get_background_color;
	iface->table_set_background_color = webkit_content_editor_table_set_background_color;
	iface->on_table_dialog_open = webkit_content_editor_on_table_dialog_open;
	iface->on_table_dialog_close = webkit_content_editor_on_table_dialog_close;
	iface->on_spell_check_dialog_open = webkit_content_editor_on_spell_check_dialog_open;
	iface->on_spell_check_dialog_close = webkit_content_editor_on_spell_check_dialog_close;
	iface->spell_check_next_word = webkit_content_editor_spell_check_next_word;
	iface->spell_check_prev_word = webkit_content_editor_spell_check_prev_word;
	iface->on_replace_dialog_open = webkit_content_editor_on_replace_dialog_open;
	iface->on_replace_dialog_close = webkit_content_editor_on_replace_dialog_close;
	iface->on_find_dialog_open = webkit_content_editor_on_find_dialog_open;
	iface->on_find_dialog_close = webkit_content_editor_on_find_dialog_close;
}
