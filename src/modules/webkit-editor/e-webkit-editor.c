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

#include "e-webkit-editor.h"

#include "web-extension/e-editor-web-extension-names.h"

#include "e-util/e-util.h"
#include "composer/e-msg-composer.h"

#include <string.h>

#define E_WEBKIT_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEBKIT_EDITOR, EWebKitEditorPrivate))

/* FIXME WK2 Move to e-content-editor? */
#define UNICODE_NBSP "\xc2\xa0"
#define SPACES_PER_LIST_LEVEL 3
#define SPACES_ORDERED_LIST_FIRST_LEVEL 6

enum {
	PROP_0,
	PROP_WEB_EXTENSION, /* for test purposes */
	PROP_CAN_COPY,
	PROP_CAN_CUT,
	PROP_CAN_PASTE,
	PROP_CAN_REDO,
	PROP_CAN_UNDO,
	PROP_CHANGED,
	PROP_EDITABLE,
	PROP_HTML_MODE,
	PROP_SPELL_CHECK_ENABLED,
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

struct _EWebKitEditorPrivate {
	EContentEditorInitializedCallback initialized_callback;
	gpointer initialized_user_data;

	GCancellable *cancellable;
	GDBusProxy *web_extension;
	GDBusConnection *dbus_connection;
	guint web_extension_watch_name_id;
	guint web_extension_selection_changed_cb_id;
	guint web_extension_content_changed_cb_id;
	guint web_extension_undo_redo_state_changed_cb_id;
	guint web_extension_user_changed_default_colors_cb_id;

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
	gboolean pasting_from_itself_extension_value;
	gboolean suppress_color_changes;

	guint32 style_flags;
	gboolean is_indented;

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

	ESpellChecker *spell_checker;
	gboolean spell_check_enabled;

	gulong owner_change_primary_clipboard_cb_id;
	gulong owner_change_clipboard_cb_id;

	WebKitFindController *find_controller; /* not referenced; set to non-NULL only if the search is in progress */
	gboolean performing_replace_all;
	guint replaced_count;
	gchar *replace_with;
	gulong found_text_handler_id;
	gulong failed_to_find_text_handler_id;

	gboolean performing_drag;
	gulong drag_data_received_handler_id;

	gchar *last_hover_uri;
};

static const GdkRGBA black = { 0, 0, 0, 1 };
static const GdkRGBA white = { 1, 1, 1, 1 };
static const GdkRGBA transparent = { 0, 0, 0, 0 };

typedef void (*PostReloadOperationFunc) (EWebKitEditor *wk_editor, gpointer data, EContentEditorInsertContentFlags flags);

typedef struct {
	PostReloadOperationFunc func;
	EContentEditorInsertContentFlags flags;
	gpointer data;
	GDestroyNotify data_free_func;
} PostReloadOperation;

static void e_webkit_editor_content_editor_init (EContentEditorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EWebKitEditor,
	e_webkit_editor,
	WEBKIT_TYPE_WEB_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_CONTENT_EDITOR,
		e_webkit_editor_content_editor_init));

EWebKitEditor *
e_webkit_editor_new (void)
{
	return g_object_new (E_TYPE_WEBKIT_EDITOR, NULL);
}

static void
webkit_editor_can_paste_cb (WebKitWebView *view,
                            GAsyncResult *result,
                            EWebKitEditor *wk_editor)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (view, result, NULL);

	if (wk_editor->priv->can_paste != value) {
		wk_editor->priv->can_paste = value;
		g_object_notify (G_OBJECT (wk_editor), "can-paste");
	}
}

static gboolean
webkit_editor_can_paste (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_paste;
}

static void
webkit_editor_can_cut_cb (WebKitWebView *view,
                                  GAsyncResult *result,
                                  EWebKitEditor *wk_editor)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (view, result, NULL);

	if (wk_editor->priv->can_cut != value) {
		wk_editor->priv->can_cut = value;
		g_object_notify (G_OBJECT (wk_editor), "can-cut");
	}
}

static gboolean
webkit_editor_can_cut (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_cut;
}

static void
webkit_editor_can_copy_cb (WebKitWebView *view,
                           GAsyncResult *result,
                           EWebKitEditor *wk_editor)
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
webkit_editor_can_copy (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_copy;
}

static gboolean
webkit_editor_get_changed (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->changed;
}

static void
webkit_editor_set_changed (EWebKitEditor *wk_editor,
                           gboolean changed)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (wk_editor->priv->changed == changed)
		return;

	wk_editor->priv->changed = changed;

	g_object_notify (G_OBJECT (wk_editor), "changed");
}

static void
webkit_editor_set_can_undo (EWebKitEditor *wk_editor,
                            gboolean can_undo)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->can_undo ? 1 : 0) == (can_undo ? 1 : 0))
		return;

	wk_editor->priv->can_undo = can_undo;

	g_object_notify (G_OBJECT (wk_editor), "can-undo");
}

static void
webkit_editor_set_can_redo (EWebKitEditor *wk_editor,
                            gboolean can_redo)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->can_redo ? 1 : 0) == (can_redo ? 1 : 0))
		return;

	wk_editor->priv->can_redo = can_redo;

	g_object_notify (G_OBJECT (wk_editor), "can-redo");
}

static void
web_extension_content_changed_cb (GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  EWebKitEditor *wk_editor)
{
	if (g_strcmp0 (signal_name, "ContentChanged") != 0)
		return;

	if (parameters) {
		guint64 page_id = 0;

		g_variant_get (parameters, "(t)", &page_id);

		if (page_id == webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (wk_editor)))
			webkit_editor_set_changed (wk_editor, TRUE);
	}
}

static void
web_extension_selection_changed_cb (GDBusConnection *connection,
                                    const gchar *sender_name,
                                    const gchar *object_path,
                                    const gchar *interface_name,
                                    const gchar *signal_name,
                                    GVariant *parameters,
                                    EWebKitEditor *wk_editor)
{
	guint64 page_id = 0;
	gchar *font_color = NULL;
	guint32 alignment, block_format, style_flags, font_size;
	gboolean is_indented;

	if (g_strcmp0 (signal_name, "SelectionChanged") != 0)
		return;

	if (!parameters)
		return;

	g_variant_get (
		parameters,
		"(tiibiis)",
		&page_id,
		&alignment,
		&block_format,
		&is_indented,
		&style_flags,
		&font_size,
		&font_color);

	if (page_id != webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (wk_editor))) {
		g_free (font_color);
		return;
	}

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor),
		WEBKIT_EDITING_COMMAND_COPY,
		NULL, /* cancellable */
		(GAsyncReadyCallback) webkit_editor_can_copy_cb,
		wk_editor);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor),
		WEBKIT_EDITING_COMMAND_CUT,
		NULL, /* cancellable */
		(GAsyncReadyCallback) webkit_editor_can_cut_cb,
		wk_editor);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor),
		WEBKIT_EDITING_COMMAND_PASTE,
		NULL, /* cancellable */
		(GAsyncReadyCallback) webkit_editor_can_paste_cb,
		wk_editor);

	g_object_freeze_notify (G_OBJECT (wk_editor));

	wk_editor->priv->alignment = alignment;
	wk_editor->priv->block_format = block_format;
	wk_editor->priv->is_indented = is_indented;
	wk_editor->priv->style_flags = style_flags;
	wk_editor->priv->font_size = font_size;

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
		/* g_object_notify (G_OBJECT (wk_editor), "background-color"); */
		g_object_notify (G_OBJECT (wk_editor), "bold");
		/* g_object_notify (G_OBJECT (wk_editor), "font-name"); */
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
web_extension_undo_redo_state_changed_cb (GDBusConnection *connection,
                                          const gchar *sender_name,
                                          const gchar *object_path,
                                          const gchar *interface_name,
                                          const gchar *signal_name,
                                          GVariant *parameters,
                                          EWebKitEditor *wk_editor)
{
	guint64 page_id = 0;
	gboolean can_undo = FALSE, can_redo = FALSE;

	if (g_strcmp0 (signal_name, "UndoRedoStateChanged") != 0)
		return;

	g_variant_get (parameters, "(tbb)", &page_id, &can_undo, &can_redo);

	if (page_id == webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (wk_editor))) {
		webkit_editor_set_can_undo (wk_editor, can_undo);
		webkit_editor_set_can_redo (wk_editor, can_redo);
	}
}

static void
web_extension_user_changed_default_colors_cb (GDBusConnection *connection,
                                              const gchar *sender_name,
                                              const gchar *object_path,
                                              const gchar *interface_name,
                                              const gchar *signal_name,
                                              GVariant *parameters,
                                              EWebKitEditor *wk_editor)
{
	if (g_strcmp0 (signal_name, "UserChangedDefaultColors") != 0)
		return;

	if (parameters)
		g_variant_get (parameters, "(b)", &wk_editor->priv->suppress_color_changes);
}

static gchar *
webkit_editor_create_service_name_for_web_context (WebKitWebContext *web_context)
{
	g_warn_if_fail (WEBKIT_IS_WEB_CONTEXT (web_context));

	return g_strdup_printf ("%s.WC%p", E_WEBKIT_EDITOR_WEB_EXTENSION_SERVICE_NAME_PREFIX, web_context);
}

static void
dispatch_pending_operations (EWebKitEditor *wk_editor)
{
	if (wk_editor->priv->webkit_load_event != WEBKIT_LOAD_FINISHED ||
	    !wk_editor->priv->web_extension)
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
                                EWebKitEditor *wk_editor)
{
	GError *error = NULL;

	wk_editor->priv->web_extension = g_dbus_proxy_new_finish (result, &error);
	if (!wk_editor->priv->web_extension) {
		g_warning ("Error creating web extension proxy: %s\n", error->message);
		g_error_free (error);

		if (wk_editor->priv->initialized_callback) {
			wk_editor->priv->initialized_callback (E_CONTENT_EDITOR (wk_editor), wk_editor->priv->initialized_user_data);

			wk_editor->priv->initialized_callback = NULL;
			wk_editor->priv->initialized_user_data = NULL;
		}

		return;
	}

	g_clear_object (&wk_editor->priv->dbus_connection);
	wk_editor->priv->dbus_connection = g_object_ref (g_dbus_proxy_get_connection (wk_editor->priv->web_extension));

	if (wk_editor->priv->web_extension_selection_changed_cb_id == 0) {
		wk_editor->priv->web_extension_selection_changed_cb_id =
			g_dbus_connection_signal_subscribe (
				wk_editor->priv->dbus_connection,
				g_dbus_proxy_get_name (wk_editor->priv->web_extension),
				E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE,
				"SelectionChanged",
				E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				(GDBusSignalCallback) web_extension_selection_changed_cb,
				wk_editor,
				NULL);
	}

	if (wk_editor->priv->web_extension_content_changed_cb_id == 0) {
		wk_editor->priv->web_extension_content_changed_cb_id =
			g_dbus_connection_signal_subscribe (
				wk_editor->priv->dbus_connection,
				g_dbus_proxy_get_name (wk_editor->priv->web_extension),
				E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE,
				"ContentChanged",
				E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				(GDBusSignalCallback) web_extension_content_changed_cb,
				wk_editor,
				NULL);
	}

	if (wk_editor->priv->web_extension_undo_redo_state_changed_cb_id == 0) {
		wk_editor->priv->web_extension_undo_redo_state_changed_cb_id =
			g_dbus_connection_signal_subscribe (
				wk_editor->priv->dbus_connection,
				g_dbus_proxy_get_name (wk_editor->priv->web_extension),
				E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE,
				"UndoRedoStateChanged",
				E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				(GDBusSignalCallback) web_extension_undo_redo_state_changed_cb,
				wk_editor,
				NULL);
	}

	if (wk_editor->priv->web_extension_user_changed_default_colors_cb_id == 0) {
		wk_editor->priv->web_extension_user_changed_default_colors_cb_id =
			g_dbus_connection_signal_subscribe (
				wk_editor->priv->dbus_connection,
				g_dbus_proxy_get_name (wk_editor->priv->web_extension),
				E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE,
				"UserChangedDefaultColors",
				E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				(GDBusSignalCallback) web_extension_user_changed_default_colors_cb,
				wk_editor,
				NULL);
	}

	dispatch_pending_operations (wk_editor);

	if (wk_editor->priv->emit_load_finished_when_extension_is_ready) {
		e_content_editor_emit_load_finished (E_CONTENT_EDITOR (wk_editor));

		wk_editor->priv->emit_load_finished_when_extension_is_ready = FALSE;
	}

	g_object_notify (G_OBJECT (wk_editor), "web-extension");

	if (wk_editor->priv->initialized_callback) {
		wk_editor->priv->initialized_callback (E_CONTENT_EDITOR (wk_editor), wk_editor->priv->initialized_user_data);

		wk_editor->priv->initialized_callback = NULL;
		wk_editor->priv->initialized_user_data = NULL;
	}
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           EWebKitEditor *wk_editor)
{
	g_dbus_proxy_new (
		connection,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL,
		name,
		E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE,
		NULL,
		(GAsyncReadyCallback) web_extension_proxy_created_cb,
		wk_editor);
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           EWebKitEditor *wk_editor)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	/* The vanished callback can be sometimes called before the appeared
	   callback, in which case it doesn't make sense to unwatch the name. */
	if (wk_editor->priv->web_extension) {
		g_clear_object (&wk_editor->priv->web_extension);

		if (wk_editor->priv->web_extension_watch_name_id > 0) {
			g_bus_unwatch_name (wk_editor->priv->web_extension_watch_name_id);
			wk_editor->priv->web_extension_watch_name_id = 0;
		}
	}
}

static void
webkit_editor_watch_web_extension (EWebKitEditor *wk_editor)
{
	gchar *service_name;

	service_name = webkit_editor_create_service_name_for_web_context (webkit_web_view_get_context (WEBKIT_WEB_VIEW (wk_editor)));

	wk_editor->priv->web_extension_watch_name_id =
		g_bus_watch_name (
			G_BUS_TYPE_SESSION,
			service_name,
			G_BUS_NAME_WATCHER_FLAGS_NONE,
			(GBusNameAppearedCallback) web_extension_appeared_cb,
			(GBusNameVanishedCallback) web_extension_vanished_cb,
			wk_editor,
			NULL);

	g_free (service_name);
}

static guint64
current_page_id (EWebKitEditor *wk_editor)
{
	return webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (wk_editor));
}
static void
webkit_editor_call_simple_extension_function_sync (EWebKitEditor *wk_editor,
                                                   const gchar *function)
{
	GVariant *result;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		function,
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result)
		g_variant_unref (result);
}

static void
webkit_editor_call_simple_extension_function (EWebKitEditor *wk_editor,
                                              const gchar *function)
{
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		function,
		g_variant_new ("(t)", current_page_id (wk_editor)),
		wk_editor->priv->cancellable);
}

static GVariant *
webkit_editor_get_element_attribute (EWebKitEditor *wk_editor,
                                     const gchar *selector,
                                     const gchar *attribute)
{
	GVariant *result;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return NULL;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ElementGetAttributeBySelector",
		g_variant_new ("(tss)", current_page_id (wk_editor), selector, attribute),
		NULL);

	return result;
}

static void
webkit_editor_set_element_attribute (EWebKitEditor *wk_editor,
                                     const gchar *selector,
                                     const gchar *attribute,
                                     const gchar *value)
{
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"ElementSetAttributeBySelector",
		g_variant_new (
			"(tsss)", current_page_id (wk_editor), selector, attribute, value),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_remove_element_attribute (EWebKitEditor *wk_editor,
                                        const gchar *selector,
                                        const gchar *attribute)
{
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"ElementRemoveAttributeBySelector",
		g_variant_new ("(tss)", current_page_id (wk_editor), selector, attribute),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_set_format_boolean (EWebKitEditor *wk_editor,
                                  const gchar *format_dom_function,
                                  gboolean format_value)
{
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		format_dom_function,
		g_variant_new ("(tb)", current_page_id (wk_editor), format_value),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_set_format_int (EWebKitEditor *wk_editor,
                              const gchar *format_dom_function,
                              gint32 format_value)
{
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		format_dom_function,
		g_variant_new ("(ti)", current_page_id (wk_editor), format_value),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_set_format_string (EWebKitEditor *wk_editor,
                                 const gchar *format_name,
                                 const gchar *format_dom_function,
                                 const gchar *format_value)
{
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (!wk_editor->priv->html_mode)
		return;

	webkit_editor_set_changed (wk_editor, TRUE);

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		format_dom_function,
		g_variant_new ("(ts)", current_page_id (wk_editor), format_value),
		wk_editor->priv->cancellable);

	g_object_notify (G_OBJECT (wk_editor), format_name);
}

static void
webkit_editor_queue_post_reload_operation (EWebKitEditor *wk_editor,
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
webkit_editor_show_inspector (EWebKitEditor *wk_editor)
{
	WebKitWebInspector *inspector;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (wk_editor));

	webkit_web_inspector_show (inspector);
}

static void
webkit_editor_initialize (EContentEditor *content_editor,
                          EContentEditorInitializedCallback callback,
                          gpointer user_data)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (content_editor));
	g_return_if_fail (callback != NULL);

	wk_editor = E_WEBKIT_EDITOR (content_editor);

	if (wk_editor->priv->web_extension) {
		callback (content_editor, user_data);
	} else {
		g_return_if_fail (wk_editor->priv->initialized_callback == NULL);

		wk_editor->priv->initialized_callback = callback;
		wk_editor->priv->initialized_user_data = user_data;
	}
}

static void
webkit_editor_update_styles (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gboolean mark_citations, use_custom_font;
	gchar *font, *aa = NULL, *citation_color;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GString *stylesheet;
	PangoFontDescription *min_size, *ms, *vw;
	WebKitSettings *settings;
	WebKitUserContentManager *manager;
	WebKitUserStyleSheet *style_sheet;

	wk_editor = E_WEBKIT_EDITOR (editor);

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

	g_string_append (
		stylesheet,
		"body[data-evo-plain-text] "
		"{\n"
		"  font-family: Monospace; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"body[data-evo-plain-text] img.-x-evo-smiley-img, "
		"body:not([data-evo-plain-text]) span.-x-evo-smiley-text "
		"{\n"
		"  display: none \n"
		"}\n");

	g_string_append (
		stylesheet,
		"[data-evo-paragraph] "
		"{\n"
		"  white-space: pre-wrap; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"body[data-evo-plain-text] [data-evo-paragraph] "
		"{\n"
		"  word-wrap: break-word; \n"
		"  word-break: break-word; \n"
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
		".-x-evo-plaintext-table td "
		"{\n"
		"  vertical-align: top;\n"
		"}\n");

	g_string_append (
		stylesheet,
		"td > * "
		"{\n"
		"  display : inline-block;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"body[data-evo-plain-text] ul "
		"{\n"
		"  list-style: outside none;\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_PER_LIST_LEVEL);

	g_string_append_printf (
		stylesheet,
		"body[data-evo-plain-text] ul > li "
		"{\n"
		"  list-style-position: outside;\n"
		"  text-indent: -%dch;\n"
		"}\n", SPACES_PER_LIST_LEVEL - 1);

	g_string_append (
		stylesheet,
		"body[data-evo-plain-text] ul > li::before "
		"{\n"
		"  content: \"*"UNICODE_NBSP"\";\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"body[data-evo-plain-text] ul.-x-evo-indented "
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_PER_LIST_LEVEL);

	g_string_append (
		stylesheet,
		"body:not([data-evo-plain-text]) ul > li.-x-evo-align-center,ol > li.-x-evo-align-center "
		"{\n"
		"  list-style-position: inside;\n"
		"}\n");

	g_string_append (
		stylesheet,
		"body:not([data-evo-plain-text]) ul > li.-x-evo-align-right, ol > li.-x-evo-align-right "
		"{\n"
		"  list-style-position: inside;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"ol "
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_ORDERED_LIST_FIRST_LEVEL);

	g_string_append_printf (
		stylesheet,
		"ol.-x-evo-indented "
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
		"body:not([data-evo-plain-text]) "
		"blockquote[type=cite] "
		"{\n"
		"  padding: 0ch 1ch 0ch 1ch;\n"
		"  margin: 0ch;\n"
		"  border-width: 0px 2px 0px 2px;\n"
		"  border-style: none solid none solid;\n"
		"  border-radius: 2px;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"body:not([data-evo-plain-text]) "
		"blockquote[type=cite] "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (1));

	g_string_append_printf (
		stylesheet,
		"body:not([data-evo-plain-text]) "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (2));

	g_string_append_printf (
		stylesheet,
		"body:not([data-evo-plain-text]) "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (3));

	g_string_append_printf (
		stylesheet,
		"body:not([data-evo-plain-text]) "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (4));

	g_string_append_printf (
		stylesheet,
		"body:not([data-evo-plain-text]) "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
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
		"default-monospace-font-size",
		e_util_normalize_font_size (
			GTK_WIDGET (wk_editor), pango_font_description_get_size (ms) / PANGO_SCALE),
		"minimum-font-size",
		e_util_normalize_font_size (
			GTK_WIDGET (wk_editor), pango_font_description_get_size (min_size) / PANGO_SCALE),
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

static void
webkit_editor_page_set_text_color (EContentEditor *editor,
                                   const GdkRGBA *value)
{
	EWebKitEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_EDITOR (editor);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	webkit_editor_set_element_attribute (wk_editor, "body", "text", color);

	g_free (color);
}

static void
webkit_editor_page_get_text_color (EContentEditor *editor,
                                   GdkRGBA *color)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto theme;

	result = webkit_editor_get_element_attribute (wk_editor, "body", "text");
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
webkit_editor_page_set_background_color (EContentEditor *editor,
                                         const GdkRGBA *value)
{
	EWebKitEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (value->alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (value));
	else
		color = g_strdup ("");

	webkit_editor_set_element_attribute (wk_editor, "body", "bgcolor", color);

	g_free (color);
}

static void
webkit_editor_page_get_background_color (EContentEditor *editor,
                                         GdkRGBA *color)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto theme;

	result = webkit_editor_get_element_attribute (wk_editor, "body", "bgcolor");
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
webkit_editor_page_set_link_color (EContentEditor *editor,
                                   const GdkRGBA *value)
{
	EWebKitEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_EDITOR (editor);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	webkit_editor_set_element_attribute (wk_editor, "body", "link", color);

	g_free (color);
}

static void
webkit_editor_page_get_link_color (EContentEditor *editor,
                                   GdkRGBA *color)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto theme;

	result = webkit_editor_get_element_attribute (wk_editor, "body", "link");
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
webkit_editor_page_set_visited_link_color (EContentEditor *editor,
                                           const GdkRGBA *value)
{
	EWebKitEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_EDITOR (editor);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	webkit_editor_set_element_attribute (wk_editor, "body", "vlink", color);

	g_free (color);
}

static void
webkit_editor_page_get_visited_link_color (EContentEditor *editor,
                                           GdkRGBA *color)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto theme;

	result = webkit_editor_get_element_attribute (wk_editor, "body", "vlink");
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
get_color_from_context (GtkStyleContext *context,
                        const gchar *name,
                        GdkRGBA *out_color)
{
	GdkColor *color = NULL;

	gtk_style_context_get_style (context, name, &color, NULL);

	if (color == NULL) {
		gboolean is_visited = strstr (name, "visited") != NULL;
		#if GTK_CHECK_VERSION(3,12,0)
		GtkStateFlags state;
		#endif

		out_color->alpha = 1;
		out_color->red = is_visited ? 1 : 0;
		out_color->green = 0;
		out_color->blue = is_visited ? 0 : 1;

		#if GTK_CHECK_VERSION(3,12,0)
		state = gtk_style_context_get_state (context);
		state = state & (~(GTK_STATE_FLAG_VISITED | GTK_STATE_FLAG_LINK));
		state = state | (is_visited ? GTK_STATE_FLAG_VISITED : GTK_STATE_FLAG_LINK);

		gtk_style_context_save (context);
		gtk_style_context_set_state (context, state);
		gtk_style_context_get_color (context, state, out_color);
		gtk_style_context_restore (context);
		#endif
	} else {
		out_color->alpha = 1;
		out_color->red = ((gdouble) color->red) / G_MAXUINT16;
		out_color->green = ((gdouble) color->green) / G_MAXUINT16;
		out_color->blue = ((gdouble) color->blue) / G_MAXUINT16;

		gdk_color_free (color);
	}
}

static void
webkit_editor_style_updated_cb (EWebKitEditor *wk_editor)
{
	GdkRGBA rgba;
	GtkStateFlags state_flags;
	GtkStyleContext *style_context;
	gboolean backdrop;

	/* If the user set the colors in Page dialog, this callback is useless. */
	if (wk_editor->priv->suppress_color_changes)
		return;

	state_flags = gtk_widget_get_state_flags (GTK_WIDGET (wk_editor));
	style_context = gtk_widget_get_style_context (GTK_WIDGET (wk_editor));
	backdrop = (state_flags & GTK_STATE_FLAG_BACKDROP) != 0;

	if (!gtk_style_context_lookup_color (
			style_context,
			backdrop ? "theme_unfocused_base_color" : "theme_base_color",
			&rgba))
		gdk_rgba_parse (&rgba, E_UTILS_DEFAULT_THEME_BASE_COLOR);

	webkit_editor_page_set_background_color (E_CONTENT_EDITOR (wk_editor), &rgba);

	if (!gtk_style_context_lookup_color (
			style_context,
			backdrop ? "theme_unfocused_fg_color" : "theme_fg_color",
			&rgba))
		gdk_rgba_parse (&rgba, E_UTILS_DEFAULT_THEME_FG_COLOR);

	webkit_editor_page_set_text_color (E_CONTENT_EDITOR (wk_editor), &rgba);

	get_color_from_context (style_context, "link-color", &rgba);
	webkit_editor_page_set_link_color (E_CONTENT_EDITOR (wk_editor), &rgba);

	get_color_from_context (style_context, "visited-link-color", &rgba);
	webkit_editor_page_set_visited_link_color (E_CONTENT_EDITOR (wk_editor), &rgba);
}

static gboolean
webkit_editor_get_html_mode (EWebKitEditor *wk_editor)
{
	return wk_editor->priv->html_mode;
}

static gboolean
show_lose_formatting_dialog (EWebKitEditor *wk_editor)
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
webkit_editor_set_html_mode (EWebKitEditor *wk_editor,
                             gboolean html_mode)
{
	gboolean convert = FALSE;
	GVariant *result;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (html_mode == wk_editor->priv->html_mode)
		return;

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"DOMCheckIfConversionNeeded",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &convert);
		g_variant_unref (result);
	}

	/* If toggling from HTML to the plain text mode, ask the user first if
	 * he wants to convert the content. */
	if (convert) {
		if (!show_lose_formatting_dialog (wk_editor))
			return;

		webkit_editor_set_changed (wk_editor, TRUE);
	}

	wk_editor->priv->html_mode = html_mode;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"SetEditorHTMLMode",
		g_variant_new ("(tbb)", current_page_id (wk_editor), html_mode, convert),
		wk_editor->priv->cancellable);

	/* Update fonts - in plain text we only want monospaced */
	webkit_editor_update_styles (E_CONTENT_EDITOR (wk_editor));
	webkit_editor_style_updated_cb (wk_editor);

	g_object_notify (G_OBJECT (wk_editor), "html-mode");
}

static void
set_convert_in_situ (EWebKitEditor *wk_editor,
                     gboolean value)
{
	e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"SetConvertInSitu",
		g_variant_new ("(tb)", current_page_id (wk_editor), value),
		NULL);
}

static void
webkit_editor_insert_content (EContentEditor *editor,
                              const gchar *content,
                              EContentEditorInsertContentFlags flags)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	/* It can happen that the view is not ready yet (it is in the middle of
	 * another load operation) so we have to queue the current operation and
	 * redo it again when the view is ready. This was happening when loading
	 * the stuff in EMailSignatureEditor. */
	if (wk_editor->priv->webkit_load_event != WEBKIT_LOAD_FINISHED ||
	    wk_editor->priv->reload_in_progress) {
		webkit_editor_queue_post_reload_operation (
			wk_editor,
			(PostReloadOperationFunc) webkit_editor_insert_content,
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
		      (strstr (content, "data-evo-draft") ||
		       strstr (content, "data-evo-signature-plain-text-mode")))) {
			webkit_editor_queue_post_reload_operation (
				wk_editor,
				(PostReloadOperationFunc) webkit_editor_insert_content,
				g_strdup (content),
				g_free,
				flags);
			return;
		}
	}

	if ((flags & E_CONTENT_EDITOR_INSERT_CONVERT) &&
	    !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		/* e_html_editor_view_convert_and_insert_plain_text
		   e_html_editor_view_convert_and_insert_html_to_plain_text
		   e_html_editor_view_insert_text */
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"DOMConvertAndInsertHTMLIntoSelection",
			g_variant_new (
				"(tsb)",
				current_page_id (wk_editor),
				content,
				(flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML)),
			wk_editor->priv->cancellable);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		   (flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML)) {
		if ((strstr (content, "data-evo-draft") ||
		     strstr (content, "data-evo-signature-plain-text-mode"))) {
			wk_editor->priv->reload_in_progress = TRUE;
			webkit_web_view_load_html (WEBKIT_WEB_VIEW (wk_editor), content, "file://");
			return;
		}

		if (strstr (content, "data-evo-draft") && !(wk_editor->priv->html_mode)) {
			set_convert_in_situ (wk_editor, TRUE);
			wk_editor->priv->reload_in_progress = TRUE;
			webkit_web_view_load_html (WEBKIT_WEB_VIEW (wk_editor), content, "file://");
			return;
		}

		/* Only convert messages that are in HTML */
		if (!(wk_editor->priv->html_mode)) {
			if (strstr (content, "<!-- text/html -->")) {
				if (!show_lose_formatting_dialog (wk_editor)) {
					wk_editor->priv->reload_in_progress = TRUE;
					webkit_editor_set_html_mode (wk_editor, TRUE);
					webkit_web_view_load_html (
						WEBKIT_WEB_VIEW (wk_editor), content, "file://");
					return;
				}
			}
			set_convert_in_situ (wk_editor, TRUE);
		}

		wk_editor->priv->reload_in_progress = TRUE;
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (wk_editor), content, "file://");
	} else if ((flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		   (flags & E_CONTENT_EDITOR_INSERT_TEXT_PLAIN)) {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"DOMConvertContent",
			g_variant_new ("(ts)", current_page_id (wk_editor), content),
			wk_editor->priv->cancellable);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_CONVERT) &&
		    !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		    !(flags & E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT)) {
		/* e_html_editor_view_paste_as_text */
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"DOMConvertAndInsertHTMLIntoSelection",
			g_variant_new (
				"(tsb)", current_page_id (wk_editor), content, TRUE),
			wk_editor->priv->cancellable);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT) &&
		   !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		/* e_html_editor_view_paste_clipboard_quoted */
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"DOMQuoteAndInsertTextIntoSelection",
			g_variant_new (
				"(tsb)", current_page_id (wk_editor), content, (flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML) != 0),
			wk_editor->priv->cancellable);
	} else if (!(flags & E_CONTENT_EDITOR_INSERT_CONVERT) &&
		   !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		/* e_html_editor_view_insert_html */
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"DOMInsertHTML",
			g_variant_new (
				"(ts)", current_page_id (wk_editor), content),
			wk_editor->priv->cancellable);
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

static GSList *
webkit_editor_get_parts_for_inline_images (GVariant *images)
{
	const gchar *element_src, *name, *id;
	GVariantIter *iter;
	GSList *parts = NULL;

	if (g_variant_check_format_string (images, "a(sss)", FALSE)) {
		g_variant_get (images, "a(sss)", &iter);
		while (g_variant_iter_loop (iter, "(&s&s&s)", &element_src, &name, &id)) {
			CamelMimePart *part;

			part = create_part_for_inline_image_from_element_data (
				element_src, name, id);
			parts = g_slist_prepend (parts, part);
		}
		g_variant_iter_free (iter);
	}

	return parts ? g_slist_reverse (parts) : NULL;
}

static gchar *
webkit_editor_get_content (EContentEditor *editor,
                           EContentEditorGetContentFlags flags,
                           const gchar *inline_images_from_domain,
                           GSList **inline_images_parts)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension)
		return g_strdup ("");

	if ((flags & E_CONTENT_EDITOR_GET_TEXT_HTML) &&
	    !(flags & E_CONTENT_EDITOR_GET_PROCESSED) &&
            !(flags & E_CONTENT_EDITOR_GET_BODY))
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"DOMEmbedStyleSheet",
			g_variant_new (
				"(ts)",
				current_page_id (wk_editor),
				wk_editor->priv->current_user_stylesheet),
			wk_editor->priv->cancellable);

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"DOMGetContent",
		g_variant_new (
			"(tsi)",
			current_page_id (wk_editor),
			inline_images_from_domain ? inline_images_from_domain : "",
			(gint32) flags),
		NULL);

	if ((flags & E_CONTENT_EDITOR_GET_TEXT_HTML) &&
	    !(flags & E_CONTENT_EDITOR_GET_PROCESSED) &&
            !(flags & E_CONTENT_EDITOR_GET_BODY))
		webkit_editor_call_simple_extension_function (
			wk_editor, "DOMRemoveEmbeddedStyleSheet");

	if (result) {
		GVariant *images = NULL;
		gchar *value = NULL;

		g_variant_get (result, "(sv)", &value, &images);
		if (inline_images_parts)
			*inline_images_parts = webkit_editor_get_parts_for_inline_images (images);

		if (images)
			g_variant_unref (images);

		g_variant_unref (result);

		return value;
	}

	return g_strdup ("");
}

static gboolean
webkit_editor_can_undo (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_undo;
}

static void
webkit_editor_undo (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (wk_editor, "DOMUndo");
}

static gboolean
webkit_editor_can_redo (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_redo;
}

static void
webkit_editor_redo (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (wk_editor, "DOMRedo");
}

static void
webkit_editor_move_caret_on_coordinates (EContentEditor *editor,
                                         gint x,
                                         gint y,
                                         gboolean cancel_if_not_collapsed)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"DOMMoveSelectionOnPoint",
		g_variant_new (
			"(tiib)", current_page_id (wk_editor), x, y, cancel_if_not_collapsed),
		NULL);

	if (result)
		g_variant_unref (result);
}

static void
webkit_editor_insert_emoticon (EContentEditor *editor,
                               EEmoticon *emoticon)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"DOMInsertSmiley",
		g_variant_new (
			"(ts)", current_page_id (wk_editor), e_emoticon_get_name (emoticon)),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_insert_image_from_mime_part (EContentEditor *editor,
                                           CamelMimePart *part)
{
	CamelDataWrapper *dw;
	CamelStream *stream;
	EWebKitEditor *wk_editor;
	GByteArray *byte_array;
	gchar *src, *base64_encoded, *mime_type, *cid_uri;
	const gchar *cid, *name;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

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
	src = g_strconcat (name ? name : "", name ? ";data:" : "", mime_type, ";base64,", base64_encoded, NULL);

	cid = camel_mime_part_get_content_id (part);
	if (!cid) {
		camel_mime_part_set_content_id (part, NULL);
		cid = camel_mime_part_get_content_id (part);
	}
	cid_uri = g_strdup_printf ("cid:%s", cid);

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"DOMAddNewInlineImageIntoList",
		g_variant_new ("(tsss)", current_page_id (wk_editor), name ? name : "", cid_uri, src),
		wk_editor->priv->cancellable);

	g_free (base64_encoded);
	g_free (mime_type);
	g_free (cid_uri);
	g_free (src);
	g_object_unref (stream);
}

static void
webkit_editor_select_all (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor), WEBKIT_EDITING_COMMAND_SELECT_ALL);
}

static void
webkit_editor_selection_wrap (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (wk_editor, "DOMSelectionWrap");
}

static gboolean
webkit_editor_selection_is_indented (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_indented;
}

static void
webkit_editor_selection_indent (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "DOMSelectionIndent");
}

static void
webkit_editor_selection_unindent (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "DOMSelectionUnindent");
}

static void
webkit_editor_cut (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	wk_editor->priv->copy_cut_actions_triggered = TRUE;

	webkit_editor_call_simple_extension_function_sync (
		wk_editor, "EEditorActionsSaveHistoryForCut");

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor), WEBKIT_EDITING_COMMAND_CUT);
}

static void
webkit_editor_copy (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	wk_editor->priv->copy_cut_actions_triggered = TRUE;

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor), WEBKIT_EDITING_COMMAND_COPY);
}

static ESpellChecker *
webkit_editor_get_spell_checker (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	return wk_editor->priv->spell_checker;
}

static gchar *
webkit_editor_get_caret_word (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *ret_val = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return NULL;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"DOMGetCaretWord",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(s)", &ret_val);
		g_variant_unref (result);
	}

	return ret_val;
}

static void
webkit_editor_set_spell_checking_languages (EContentEditor *editor,
                                            const gchar **languages)
{
	EWebKitEditor *wk_editor;
	WebKitWebContext *web_context;

	wk_editor = E_WEBKIT_EDITOR (editor);
	web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (wk_editor));
	webkit_web_context_set_spell_checking_languages (web_context, (const gchar * const *) languages);
}

static void
webkit_editor_set_spell_check_enabled (EWebKitEditor *wk_editor,
                                       gboolean enable)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->spell_check_enabled ? 1 : 0) == (enable ? 1 : 0))
		return;

	wk_editor->priv->spell_check_enabled = enable;

	webkit_editor_call_simple_extension_function (
		wk_editor, enable ? "DOMForceSpellCheck" : "DOMTurnSpellCheckOff");

	g_object_notify (G_OBJECT (wk_editor), "spell-check-enabled");
}

static gboolean
webkit_editor_get_spell_check_enabled (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->spell_check_enabled;
}

static gboolean
webkit_editor_is_editable (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return webkit_web_view_is_editable (WEBKIT_WEB_VIEW (wk_editor));
}

static void
webkit_editor_set_editable (EWebKitEditor *wk_editor,
                                    gboolean editable)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	return webkit_web_view_set_editable (WEBKIT_WEB_VIEW (wk_editor), editable);
}

static gchar *
webkit_editor_get_current_signature_uid (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *ret_val= NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return NULL;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"DOMGetActiveSignatureUid",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(s)", &ret_val);
		g_variant_unref (result);
	}

	return ret_val;
}

static gboolean
webkit_editor_is_ready (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	/* Editor is ready just in case that the web view is not loading, there
	 * is no reload in progress and there is no pending post reload operation
	 * and the web extension for the editor is created. */
	return !webkit_web_view_is_loading (WEBKIT_WEB_VIEW (wk_editor)) &&
		!wk_editor->priv->reload_in_progress &&
		wk_editor->priv->web_extension &&
		(!wk_editor->priv->post_reload_operations || g_queue_is_empty (wk_editor->priv->post_reload_operations));
}

static char *
webkit_editor_insert_signature (EContentEditor *editor,
                                const gchar *content,
                                gboolean is_html,
                                const gchar *signature_id,
                                gboolean *set_signature_from_message,
                                gboolean *check_if_signature_is_changed,
                                gboolean *ignore_next_signature_change)
{
	EWebKitEditor *wk_editor;
	gchar *ret_val = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return NULL;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"DOMInsertSignature",
		g_variant_new (
			"(tsbsbbb)",
			current_page_id (wk_editor),
			content ? content : "",
			is_html,
			signature_id,
			*set_signature_from_message,
			*check_if_signature_is_changed,
			*ignore_next_signature_change),
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
webkit_editor_get_caret_position (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	guint ret_val = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"DOMGetCaretPosition",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(u)", &ret_val);
		g_variant_unref (result);
	}

	return ret_val;
}

static guint
webkit_editor_get_caret_offset (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	guint ret_val = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"DOMGetCaretOffset",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(u)", &ret_val);
		g_variant_unref (result);
	}

	return ret_val;
}

static void
webkit_editor_clear_undo_redo_history (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"DOMClearUndoRedoHistory",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_replace_caret_word (EContentEditor *editor,
                                  const gchar *replacement)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"DOMReplaceCaretWord",
		g_variant_new ("(ts)", current_page_id (wk_editor), replacement),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_finish_search (EWebKitEditor *wk_editor)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

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
                                      EWebKitEditor *wk_editor)
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
                                               EWebKitEditor *wk_editor)
{
	if (wk_editor->priv->performing_replace_all) {
		guint replaced_count = wk_editor->priv->replaced_count;

		webkit_editor_finish_search (wk_editor);
		e_content_editor_emit_replace_all_done (E_CONTENT_EDITOR (wk_editor), replaced_count);
	} else {
		e_content_editor_emit_find_done (E_CONTENT_EDITOR (wk_editor), 0);
	}
}

static void
webkit_editor_prepare_find_controller (EWebKitEditor *wk_editor)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));
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
webkit_editor_find (EContentEditor *editor,
                    guint32 flags,
                    const gchar *text)
{
	EWebKitEditor *wk_editor;
	guint32 wk_options;
	gboolean needs_init;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	wk_editor = E_WEBKIT_EDITOR (editor);

	wk_options = find_flags_to_webkit_find_options (flags);

	needs_init = !wk_editor->priv->find_controller;
	if (needs_init) {
		webkit_editor_prepare_find_controller (wk_editor);
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
webkit_editor_replace (EContentEditor *editor,
                       const gchar *replacement)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"DOMSelectionReplace",
		g_variant_new ("(ts)", current_page_id (wk_editor), replacement),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_replace_all (EContentEditor *editor,
                           guint32 flags,
                           const gchar *find_text,
                           const gchar *replace_with)
{
	EWebKitEditor *wk_editor;
	guint32 wk_options;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));
	g_return_if_fail (find_text != NULL);
	g_return_if_fail (replace_with != NULL);

	wk_editor = E_WEBKIT_EDITOR (editor);
	wk_options = find_flags_to_webkit_find_options (flags);

	if (!wk_editor->priv->find_controller)
		webkit_editor_prepare_find_controller (wk_editor);

	g_free (wk_editor->priv->replace_with);
	wk_editor->priv->replace_with = g_strdup (replace_with);

	wk_editor->priv->performing_replace_all = TRUE;
	wk_editor->priv->replaced_count = 0;

	webkit_find_controller_search (wk_editor->priv->find_controller, find_text, wk_options, G_MAXUINT);
}

static void
webkit_editor_selection_save (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "DOMSaveSelection");
}

static void
webkit_editor_selection_restore (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "DOMRestoreSelection");
}

static void
webkit_editor_delete_cell_contents (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorDialogDeleteCellContents");
}

static void
webkit_editor_delete_column (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorDialogDeleteColumn");
}

static void
webkit_editor_delete_row (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorDialogDeleteRow");
}

static void
webkit_editor_delete_table (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorDialogDeleteTable");
}

static void
webkit_editor_insert_column_after (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorDialogInsertColumnAfter");
}

static void
webkit_editor_insert_column_before (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorDialogInsertColumnBefore");
}


static void
webkit_editor_insert_row_above (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorDialogInsertRowAbove");
}

static void
webkit_editor_insert_row_below (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorDialogInsertRowBelow");
}

static gboolean
webkit_editor_on_h_rule_dialog_open (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gboolean value = FALSE;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return FALSE;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorHRuleDialogFindHRule",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_on_h_rule_dialog_close (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorHRuleDialogOnClose");
}

static void
webkit_editor_h_rule_set_align (EContentEditor *editor,
                                const gchar *value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-hr", "align", value);
}

static gchar *
webkit_editor_h_rule_get_align (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-hr", "align");
	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_h_rule_set_size (EContentEditor *editor,
                               gint value)
{
	EWebKitEditor *wk_editor;
	gchar *size;

	wk_editor = E_WEBKIT_EDITOR (editor);

	size = g_strdup_printf ("%d", value);

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-hr", "size", size);

	g_free (size);
}

static gint
webkit_editor_h_rule_get_size (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gint size = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
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
webkit_editor_h_rule_set_width (EContentEditor *editor,
                                gint value,
                                EContentEditorUnit unit)
{
	EWebKitEditor *wk_editor;
	gchar *width;

	wk_editor = E_WEBKIT_EDITOR (editor);

	width = g_strdup_printf (
		"%d%s",
		value,
		(unit == E_CONTENT_EDITOR_UNIT_PIXEL) ? "px" : "%");

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-hr", "width", width);

	g_free (width);
}

static gint
webkit_editor_h_rule_get_width (EContentEditor *editor,
                                EContentEditorUnit *unit)
{
	EWebKitEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	*unit = E_CONTENT_EDITOR_UNIT_PIXEL;

	result = webkit_editor_get_element_attribute (
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
webkit_editor_h_rule_set_no_shade (EContentEditor *editor,
                                   gboolean value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (value)
		webkit_editor_set_element_attribute (
			wk_editor, "#-x-evo-current-hr", "noshade", "");
	else
		webkit_editor_remove_element_attribute (
			wk_editor, "#-x-evo-current-hr", "noshade");
}

static gboolean
webkit_editor_h_rule_get_no_shade (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gboolean no_shade = FALSE;

	wk_editor = E_WEBKIT_EDITOR (editor);
	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return FALSE;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ElementHasAttribute",
		g_variant_new ("(tss)", current_page_id (wk_editor), "-x-evo-current-hr", "noshade"),
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &no_shade);
		g_variant_unref (result);
	}

	return no_shade;
}

static void
webkit_editor_on_image_dialog_open (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorImageDialogMarkImage");
}

static void
webkit_editor_on_image_dialog_close (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorImageDialogSaveHistoryOnExit");
}

static void
webkit_editor_insert_image (EContentEditor *editor,
                            const gchar *image_uri)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"DOMSelectionInsertImage",
		g_variant_new ("(ts)", current_page_id (wk_editor), image_uri),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_replace_image_src (EWebKitEditor *wk_editor,
                                 const gchar *selector,
                                 const gchar *image_uri)
{

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"DOMReplaceImageSrc",
		g_variant_new ("(tss)", current_page_id (wk_editor), selector, image_uri),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_image_set_src (EContentEditor *editor,
                             const gchar *value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_replace_image_src (
		wk_editor, "img#-x-evo-current-img", value);
}

static gchar *
webkit_editor_image_get_src (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-img", "data-uri");

	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_image_set_alt (EContentEditor *editor,
                             const gchar *value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-img", "alt", value);
}

static gchar *
webkit_editor_image_get_alt (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-img", "alt");

	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_image_set_url (EContentEditor *editor,
                             const gchar *value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorImageDialogSetElementUrl",
		g_variant_new ("(ts)", current_page_id (wk_editor), value),
		wk_editor->priv->cancellable);
}

static gchar *
webkit_editor_image_get_url (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gchar *value = NULL;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return NULL;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorImageDialogGetElementUrl",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_image_set_vspace (EContentEditor *editor,
                                gint value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementSetVSpace",
		g_variant_new (
			"(tsi)", current_page_id (wk_editor), "-x-evo-current-img", value),
		wk_editor->priv->cancellable);
}

static gint
webkit_editor_image_get_vspace (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementGetVSpace",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_image_set_hspace (EContentEditor *editor,
                                        gint value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementSetHSpace",
		g_variant_new (
			"(tsi)", current_page_id (wk_editor), "-x-evo-current-img", value),
		wk_editor->priv->cancellable);
}

static gint
webkit_editor_image_get_hspace (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementGetHSpace",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_image_set_border (EContentEditor *editor,
                                gint value)
{
	EWebKitEditor *wk_editor;
	gchar *border;

	wk_editor = E_WEBKIT_EDITOR (editor);

	border = g_strdup_printf ("%d", value);

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-img", "border", border);

	g_free (border);
}

static gint
webkit_editor_image_get_border (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
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
webkit_editor_image_set_align (EContentEditor *editor,
                               const gchar *value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-img", "align", value);
}

static gchar *
webkit_editor_image_get_align (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-img", "align");

	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gint32
webkit_editor_image_get_natural_width (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint32 value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementGetNaturalWidth",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gint32
webkit_editor_image_get_natural_height (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint32 value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementGetNaturalHeight",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_image_set_height (EContentEditor *editor,
                                gint value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementSetHeight",
		g_variant_new (
			"(tsi)", current_page_id (wk_editor), "-x-evo-current-img", value),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_image_set_width (EContentEditor *editor,
                               gint value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementSetWidth",
		g_variant_new (
			"(tsi)", current_page_id (wk_editor), "-x-evo-current-img", value),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_image_set_height_follow (EContentEditor *editor,
                                      gboolean value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (value)
		webkit_editor_set_element_attribute (
			wk_editor, "#-x-evo-current-img", "style", "height: auto;");
	else
		webkit_editor_remove_element_attribute (
			wk_editor, "#-x-evo-current-img", "style");
}

static void
webkit_editor_image_set_width_follow (EContentEditor *editor,
                                     gboolean value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (value)
		webkit_editor_set_element_attribute (
			wk_editor, "#-x-evo-current-img", "style", "width: auto;");
	else
		webkit_editor_remove_element_attribute (
			wk_editor, "#-x-evo-current-img", "style");
}

static gint32
webkit_editor_image_get_width (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint32 value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementGetWidth",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gint32
webkit_editor_image_get_height (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint32 value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ImageElementGetHeight",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-img"),
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_selection_unlink (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorLinkDialogUnlink");
}

static void
webkit_editor_on_link_dialog_open (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorLinkDialogOnOpen");
}

static void
webkit_editor_on_link_dialog_close (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorLinkDialogOnClose");
}

static void
webkit_editor_link_set_values (EContentEditor *editor,
                               const gchar *href,
                               const gchar *text)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorLinkDialogOk",
		g_variant_new ("(tss)", current_page_id (wk_editor), href, text),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_link_get_values (EContentEditor *editor,
                               gchar **href,
                               gchar **text)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorLinkDialogShow",
		g_variant_new ("(t)", current_page_id (wk_editor)),
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
webkit_editor_set_alignment (EWebKitEditor *wk_editor,
                             EContentEditorAlignment value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	webkit_editor_set_format_int (
		wk_editor, "DOMSelectionSetAlignment", (gint32) value);
}

static EContentEditorAlignment
webkit_editor_get_alignment (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), E_CONTENT_EDITOR_ALIGNMENT_LEFT);

	return wk_editor->priv->alignment;
}

static void
webkit_editor_set_block_format (EWebKitEditor *wk_editor,
                                EContentEditorBlockFormat value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	webkit_editor_set_format_int (
		wk_editor, "DOMSelectionSetBlockFormat", (gint32) value);
}

static EContentEditorBlockFormat
webkit_editor_get_block_format (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), E_CONTENT_EDITOR_BLOCK_FORMAT_NONE);

	return wk_editor->priv->block_format;
}

static void
webkit_editor_set_background_color (EWebKitEditor *wk_editor,
                                    const GdkRGBA *value)
{
	gchar *color;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (gdk_rgba_equal (value, wk_editor->priv->background_color))
		return;

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	if (wk_editor->priv->background_color)
		gdk_rgba_free (wk_editor->priv->background_color);

	wk_editor->priv->background_color = gdk_rgba_copy (value);

	webkit_editor_set_format_string (
		wk_editor,
		"background-color",
		"DOMSelectionSetBackgroundColor",
		color);

	g_free (color);
}

static const GdkRGBA *
webkit_editor_get_background_color (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return NULL;
	}

	if (!wk_editor->priv->html_mode || !wk_editor->priv->background_color)
		return &white;

	return wk_editor->priv->background_color;
}

static void
webkit_editor_set_font_name (EWebKitEditor *wk_editor,
                             const gchar *value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	wk_editor->priv->font_name = g_strdup (value);

	webkit_editor_set_format_string (
		wk_editor, "font-name", "DOMSelectionSetFontName", value);
}

static const gchar *
webkit_editor_get_font_name (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	return wk_editor->priv->font_name;
}

static void
webkit_editor_set_font_color (EWebKitEditor *wk_editor,
                              const GdkRGBA *value)
{
	gchar *color;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (gdk_rgba_equal (value, wk_editor->priv->font_color))
		return;

	color = g_strdup_printf ("#%06x", e_rgba_to_value (value));

	if (wk_editor->priv->font_color)
		gdk_rgba_free (wk_editor->priv->font_color);

	wk_editor->priv->font_color = gdk_rgba_copy (value);

	webkit_editor_set_format_string (
		wk_editor,
		"font-color",
		"DOMSelectionSetFontColor",
		color);

	g_free (color);
}

static const GdkRGBA *
webkit_editor_get_font_color (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return NULL;
	}

	if (!wk_editor->priv->html_mode || !wk_editor->priv->font_color)
		return &black;

	return wk_editor->priv->font_color;
}

static void
webkit_editor_set_font_size (EWebKitEditor *wk_editor,
                             gint value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (wk_editor->priv->font_size == value)
		return;

	wk_editor->priv->font_size = value;

	webkit_editor_set_format_int (
		wk_editor, "DOMSelectionSetFontSize", value);
}

static gint
webkit_editor_get_font_size (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), -1);

	return wk_editor->priv->font_size;
}

static void
webkit_editor_set_style_flag (EWebKitEditor *wk_editor,
			      EContentEditorStyleFlags flag,
			      gboolean do_set,
			      const gchar *dom_function_name)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (((wk_editor->priv->style_flags & flag) != 0 ? 1 : 0) == (do_set ? 1 : 0))
		return;

	wk_editor->priv->style_flags = (wk_editor->priv->style_flags & ~flag) | (do_set ? flag : 0);

	webkit_editor_set_format_boolean (wk_editor, dom_function_name, do_set);
}

static gboolean
webkit_editor_get_style_flag (EWebKitEditor *wk_editor,
			      EContentEditorStyleFlags flag)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return (wk_editor->priv->style_flags & flag) != 0;
}

static void
webkit_editor_on_page_dialog_open (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorPageDialogSaveHistory");
}

static void
webkit_editor_on_page_dialog_close (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorPageDialogSaveHistoryOnExit");
}

static gchar *
webkit_editor_page_get_background_image_uri (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_editor_get_element_attribute (wk_editor, "body", "data-uri");
	if (result) {
		gchar *value;

		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return NULL;
}

static void
webkit_editor_page_set_background_image_uri (EContentEditor *editor,
                                             const gchar *uri)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (uri && *uri)
		webkit_editor_replace_image_src (wk_editor, "body", uri);
	else {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new ("(ts)", current_page_id (wk_editor), "body"),
			wk_editor->priv->cancellable);
	}
}

static void
webkit_editor_on_cell_dialog_open (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogMarkCurrentCellElement",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_on_cell_dialog_close (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorCellDialogSaveHistoryOnExit");
}

static void
webkit_editor_cell_set_v_align (EContentEditor *editor,
                                const gchar *value,
                                EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogSetElementVAlign",
		g_variant_new ("(tsi)", current_page_id (wk_editor), value, (gint32) scope),
		wk_editor->priv->cancellable);
}

static gchar *
webkit_editor_cell_get_v_align (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-cell", "valign");
	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_cell_set_align (EContentEditor *editor,
                              const gchar *value,
                              EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogSetElementAlign",
		g_variant_new ("(tsi)", current_page_id (wk_editor), value, (gint32) scope),
		wk_editor->priv->cancellable);
}

static gchar *
webkit_editor_cell_get_align (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-cell", "align");
	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_cell_set_wrap (EContentEditor *editor,
                             gboolean value,
                             EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogSetElementNoWrap",
		g_variant_new ("(tbi)", current_page_id (wk_editor), !value, (gint32) scope),
		wk_editor->priv->cancellable);
}

static gboolean
webkit_editor_cell_get_wrap (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gboolean value = FALSE;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return FALSE;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return FALSE;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"TableCellElementGetNoWrap",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &value);
		value = !value;
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_cell_set_header_style (EContentEditor *editor,
                                     gboolean value,
                                     EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (!wk_editor->priv->html_mode)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogSetElementHeaderStyle",
		g_variant_new ("(tbi)", current_page_id (wk_editor), value, (gint32) scope),
		wk_editor->priv->cancellable);
}

static gboolean
webkit_editor_cell_is_header (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gboolean value = FALSE;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return FALSE;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return FALSE;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"ElementGetTagName",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
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
webkit_editor_cell_get_width (EContentEditor *editor,
                              EContentEditorUnit *unit)
{
	EWebKitEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	*unit = E_CONTENT_EDITOR_UNIT_AUTO;

	if (!wk_editor->priv->html_mode)
		return 0;

	result = webkit_editor_get_element_attribute (
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
		g_variant_unref (result);
	}

	return value;
}

static gint
webkit_editor_cell_get_row_span (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return 0;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"TableCellElementGetRowSpan",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gint
webkit_editor_cell_get_col_span (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return 0;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"TableCellElementGetColSpan",
		g_variant_new ("(ts)", current_page_id (wk_editor), "-x-evo-current-cell"),
		NULL);

	if (result) {
		g_variant_get (result, "(i)", &value);
		g_variant_unref (result);
	}

	return value;
}

static gchar *
webkit_editor_cell_get_background_image_uri (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-cell", "data-uri");
	if (result) {
		gchar *value;

		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return NULL;
}

static void
webkit_editor_cell_get_background_color (EContentEditor *editor,
                                         GdkRGBA *color)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto exit;

	result = webkit_editor_get_element_attribute (
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
webkit_editor_cell_set_row_span (EContentEditor *editor,
                                 gint value,
                                 EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogSetElementRowSpan",
		g_variant_new ("(tii)", current_page_id (wk_editor), value, (gint32) scope),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_cell_set_col_span (EContentEditor *editor,
                                 gint value,
                                 EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogSetElementColSpan",
		g_variant_new ("(tii)", current_page_id (wk_editor), value, (gint32) scope),
		wk_editor->priv->cancellable);
}

static void
webkit_editor_cell_set_width (EContentEditor *editor,
                              gint value,
                              EContentEditorUnit unit,
                              EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;
	gchar *width;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (unit == E_CONTENT_EDITOR_UNIT_AUTO)
		width = g_strdup ("auto");
	else
		width = g_strdup_printf (
			"%d%s",
			value,
			(unit == E_CONTENT_EDITOR_UNIT_PIXEL) ? "px" : "%");

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogSetElementWidth",
		g_variant_new ("(tsi)", current_page_id (wk_editor), width, (gint32) scope),
		wk_editor->priv->cancellable);

	g_free (width);
}

static void
webkit_editor_cell_set_background_color (EContentEditor *editor,
                                         const GdkRGBA *value,
                                         EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (value->alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (value));
	else
		color = g_strdup ("");

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorCellDialogSetElementBgColor",
		g_variant_new ("(tsi)", current_page_id (wk_editor), color, (gint32) scope),
		wk_editor->priv->cancellable);

	g_free (color);
}

static void
webkit_editor_cell_set_background_image_uri (EContentEditor *editor,
                                             const gchar *uri)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (!wk_editor->priv->html_mode)
		return;

	if (uri && *uri)
		webkit_editor_replace_image_src (wk_editor, "#-x-evo-current-cell", uri);
	else {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new ("(ts)", current_page_id (wk_editor), "#-x-evo-current-cell"),
			wk_editor->priv->cancellable);
	}
}

static void
webkit_editor_table_set_row_count (EContentEditor *editor,
                                   guint value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorTableDialogSetRowCount",
		g_variant_new ("(tu)", current_page_id (wk_editor), value),
		wk_editor->priv->cancellable);
}

static guint
webkit_editor_table_get_row_count (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	guint value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return 0;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorTableDialogGetRowCount",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(u)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_table_set_column_count (EContentEditor *editor,
                                      guint value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorTableDialogSetColumnCount",
		g_variant_new ("(tu)", current_page_id (wk_editor), value),
		wk_editor->priv->cancellable);
}

static guint
webkit_editor_table_get_column_count (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	guint value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return 0;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return 0;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorTableDialogGetColumnCount",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(u)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_table_set_width (EContentEditor *editor,
                               gint value,
                               EContentEditorUnit unit)
{
	EWebKitEditor *wk_editor;
	gchar *width;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (unit == E_CONTENT_EDITOR_UNIT_AUTO)
		width = g_strdup ("auto");
	else
		width = g_strdup_printf (
			"%d%s",
			value,
			(unit == E_CONTENT_EDITOR_UNIT_PIXEL) ? "px" : "%");

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "width", width);

	g_free (width);
}

static guint
webkit_editor_table_get_width (EContentEditor *editor,
                               EContentEditorUnit *unit)
{
	EWebKitEditor *wk_editor;
	guint value = 0;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	*unit = E_CONTENT_EDITOR_UNIT_PIXEL;

	if (!wk_editor->priv->html_mode)
		return 0;

	result = webkit_editor_get_element_attribute (
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
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_table_set_align (EContentEditor *editor,
                               const gchar *value)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return;

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "align", value);
}

static gchar *
webkit_editor_table_get_align (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gchar *value = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_editor_get_element_attribute (
		wk_editor, "#-x-evo-current-table", "align");
	if (result) {
		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_table_set_padding (EContentEditor *editor,
                                 gint value)
{
	EWebKitEditor *wk_editor;
	gchar *padding;

	wk_editor = E_WEBKIT_EDITOR (editor);

	padding = g_strdup_printf ("%d", value);

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "cellpadding", padding);

	g_free (padding);
}

static gint
webkit_editor_table_get_padding (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
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
webkit_editor_table_set_spacing (EContentEditor *editor,
                                 gint value)
{
	EWebKitEditor *wk_editor;
	gchar *spacing;

	wk_editor = E_WEBKIT_EDITOR (editor);

	spacing = g_strdup_printf ("%d", value);

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "cellspacing", spacing);

	g_free (spacing);
}

static gint
webkit_editor_table_get_spacing (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
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
webkit_editor_table_set_border (EContentEditor *editor,
                                gint value)
{
	EWebKitEditor *wk_editor;
	gchar *border;

	wk_editor = E_WEBKIT_EDITOR (editor);

	border = g_strdup_printf ("%d", value);

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "border", border);

	g_free (border);
}

static gint
webkit_editor_table_get_border (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gint value = 0;

	wk_editor = E_WEBKIT_EDITOR (editor);

	result = webkit_editor_get_element_attribute (
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
webkit_editor_table_get_background_color (EContentEditor *editor,
                                          GdkRGBA *color)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		goto exit;

	result = webkit_editor_get_element_attribute (
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
webkit_editor_table_set_background_color (EContentEditor *editor,
                                          const GdkRGBA *value)
{
	EWebKitEditor *wk_editor;
	gchar *color;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (value->alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (value));
	else
		color = g_strdup ("");

	webkit_editor_set_element_attribute (
		wk_editor, "#-x-evo-current-table", "bgcolor", color);

	g_free (color);
}

static gchar *
webkit_editor_table_get_background_image_uri (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->html_mode)
		return NULL;

	result = webkit_editor_get_element_attribute (wk_editor, "#-x-evo-current-table", "data-uri");
	if (result) {
		gchar *value;

		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
	}

	return NULL;
}

static void
webkit_editor_table_set_background_image_uri (EContentEditor *editor,
                                              const gchar *uri)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return;
	}

	if (!wk_editor->priv->html_mode)
		return;

	if (uri && *uri)
		webkit_editor_replace_image_src (wk_editor, "#-x-evo-current-table", uri);
	else {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			wk_editor->priv->web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new ("(ts)", current_page_id (wk_editor), "#-x-evo-current-table"),
			wk_editor->priv->cancellable);
	}
}

static gboolean
webkit_editor_on_table_dialog_open (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GVariant *result;
	gboolean value = FALSE;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return FALSE;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		"EEditorTableDialogShow",
		g_variant_new ("(t)", current_page_id (wk_editor)),
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &value);
		g_variant_unref (result);
	}

	return value;
}

static void
webkit_editor_on_table_dialog_close (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_call_simple_extension_function (
		wk_editor, "EEditorTableDialogSaveHistoryOnExit");

	webkit_editor_finish_search (E_WEBKIT_EDITOR (editor));
}

static void
webkit_editor_on_spell_check_dialog_open (EContentEditor *editor)
{
}

static void
webkit_editor_on_spell_check_dialog_close (EContentEditor *editor)
{
	webkit_editor_finish_search (E_WEBKIT_EDITOR (editor));
}

static gchar *
move_to_another_word (EContentEditor *editor,
                      const gchar *word,
                      const gchar *dom_function)
{
	EWebKitEditor *wk_editor;
	gchar **active_languages;
	gchar *another_word = NULL;
	GVariant *result;

	wk_editor = E_WEBKIT_EDITOR (editor);

	if (!wk_editor->priv->web_extension) {
		g_warning ("EHTMLEditorWebExtension not ready at %s!", G_STRFUNC);
		return NULL;
	}

	active_languages = e_spell_checker_list_active_languages (
		wk_editor->priv->spell_checker, NULL);
	if (!active_languages)
		return NULL;

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		wk_editor->priv->web_extension,
		dom_function,
		g_variant_new (
			"(ts^as)", current_page_id (wk_editor), word ? word : "", active_languages),
		NULL);

	g_strfreev (active_languages);

	if (result) {
		g_variant_get (result, "(s)", &another_word);
		g_variant_unref (result);
	}

	return another_word;
}

static gchar *
webkit_editor_spell_check_next_word (EContentEditor *editor,
                                     const gchar *word)
{
	return move_to_another_word (editor, word, "EEditorSpellCheckDialogNext");
}

static gchar *
webkit_editor_spell_check_prev_word (EContentEditor *editor,
                                     const gchar *word)
{
	return move_to_another_word (editor, word, "EEditorSpellCheckDialogPrev");
}

static void
webkit_editor_on_replace_dialog_open (EContentEditor *editor)
{
}

static void
webkit_editor_on_replace_dialog_close (EContentEditor *editor)
{
	webkit_editor_finish_search (E_WEBKIT_EDITOR (editor));
}

static void
webkit_editor_on_find_dialog_open (EContentEditor *editor)
{
}

static void
webkit_editor_on_find_dialog_close (EContentEditor *editor)
{
	webkit_editor_finish_search (E_WEBKIT_EDITOR (editor));
}

static GDBusProxy *
webkit_editor_get_web_extension (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	return wk_editor->priv->web_extension;
}

static void
webkit_editor_constructed (GObject *object)
{
	EWebKitEditor *wk_editor;
	gchar **languages;
	WebKitWebContext *web_context;
	WebKitSettings *web_settings;
	WebKitWebView *web_view;

	G_OBJECT_CLASS (e_webkit_editor_parent_class)->constructed (object);

	wk_editor = E_WEBKIT_EDITOR (object);
	web_view = WEBKIT_WEB_VIEW (wk_editor);

	webkit_editor_watch_web_extension (wk_editor);

	/* Give spell check languages to WebKit */
	languages = e_spell_checker_list_active_languages (wk_editor->priv->spell_checker, NULL);

	web_context = webkit_web_view_get_context (web_view);
	webkit_web_context_set_spell_checking_enabled (web_context, TRUE);
	webkit_web_context_set_spell_checking_languages (web_context, (const gchar * const *) languages);
	g_strfreev (languages);

	webkit_web_view_set_editable (web_view, TRUE);

	web_settings = webkit_web_view_get_settings (web_view);
	webkit_settings_set_allow_file_access_from_file_urls (web_settings, TRUE);
	webkit_settings_set_enable_developer_extras (web_settings, e_util_get_webkit_developer_mode_enabled ());

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
webkit_editor_constructor (GType type,
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
			/* Share one web_context between all editors, thus there is one WebProcess
			   for all the editors (and one for the preview). */
			static gpointer web_context = NULL;

			if (!web_context) {
				gchar *service_name;

				web_context = webkit_web_context_new ();

				service_name = webkit_editor_create_service_name_for_web_context (web_context);

				webkit_web_context_set_cache_model (web_context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
				webkit_web_context_set_web_extensions_directory (web_context, EVOLUTION_WEB_EXTENSIONS_WEBKIT_EDITOR_DIR);
				webkit_web_context_set_web_extensions_initialization_user_data (web_context,
					g_variant_new_string (service_name));

				g_object_add_weak_pointer (G_OBJECT (web_context), &web_context);

				g_free (service_name);
			} else {
				g_object_ref (web_context);
			}

			g_value_take_object (param->value, web_context);
		}
	}

	g_type_class_unref (object_class);

	return G_OBJECT_CLASS (e_webkit_editor_parent_class)->constructor (type, n_construct_properties, construct_properties);
}

static void
webkit_editor_dispose (GObject *object)
{
	EWebKitEditorPrivate *priv;

	priv = E_WEBKIT_EDITOR_GET_PRIVATE (object);

	if (priv->cancellable)
		g_cancellable_cancel (priv->cancellable);

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

	if (priv->web_extension_content_changed_cb_id > 0 && priv->dbus_connection) {
		g_dbus_connection_signal_unsubscribe (
			priv->dbus_connection,
			priv->web_extension_content_changed_cb_id);
		priv->web_extension_content_changed_cb_id = 0;
	}

	if (priv->web_extension_selection_changed_cb_id > 0 && priv->dbus_connection) {
		g_dbus_connection_signal_unsubscribe (
			priv->dbus_connection,
			priv->web_extension_selection_changed_cb_id);
		priv->web_extension_selection_changed_cb_id = 0;
	}

	if (priv->web_extension_undo_redo_state_changed_cb_id > 0 && priv->dbus_connection) {
		g_dbus_connection_signal_unsubscribe (
			priv->dbus_connection,
			priv->web_extension_undo_redo_state_changed_cb_id);
		priv->web_extension_undo_redo_state_changed_cb_id = 0;
	}

	if (priv->web_extension_user_changed_default_colors_cb_id > 0 && priv->dbus_connection) {
		g_dbus_connection_signal_unsubscribe (
			priv->dbus_connection,
			priv->web_extension_user_changed_default_colors_cb_id);
		priv->web_extension_user_changed_default_colors_cb_id = 0;
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

	webkit_editor_finish_search (E_WEBKIT_EDITOR (object));

	g_clear_object (&priv->web_extension);
	g_clear_object (&priv->dbus_connection);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_webkit_editor_parent_class)->dispose (object);
}

static void
webkit_editor_finalize (GObject *object)
{
	EWebKitEditorPrivate *priv;

	priv = E_WEBKIT_EDITOR_GET_PRIVATE (object);

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

	g_free (priv->last_hover_uri);
	priv->last_hover_uri = NULL;

	g_clear_object (&priv->spell_checker);
	g_clear_object (&priv->cancellable);

	g_free (priv->font_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_webkit_editor_parent_class)->finalize (object);
}

static void
webkit_editor_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			webkit_editor_set_changed (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_EDITABLE:
			webkit_editor_set_editable (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_HTML_MODE:
			webkit_editor_set_html_mode (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_ALIGNMENT:
			webkit_editor_set_alignment (
				E_WEBKIT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_BACKGROUND_COLOR:
			webkit_editor_set_background_color (
				E_WEBKIT_EDITOR (object),
				g_value_get_boxed (value));
			return;

		case PROP_BOLD:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_CONTENT_EDITOR_STYLE_IS_BOLD,
				g_value_get_boolean (value),
				"DOMSelectionSetBold");
			return;

		case PROP_FONT_COLOR:
			webkit_editor_set_font_color (
				E_WEBKIT_EDITOR (object),
				g_value_get_boxed (value));
			return;

		case PROP_BLOCK_FORMAT:
			webkit_editor_set_block_format (
				E_WEBKIT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_FONT_NAME:
			webkit_editor_set_font_name (
				E_WEBKIT_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_FONT_SIZE:
			webkit_editor_set_font_size (
				E_WEBKIT_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_ITALIC:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_CONTENT_EDITOR_STYLE_IS_ITALIC,
				g_value_get_boolean (value),
				"DOMSelectionSetItalic");
			return;

		case PROP_MONOSPACED:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_CONTENT_EDITOR_STYLE_IS_MONOSPACE,
				g_value_get_boolean (value),
				"DOMSelectionSetMonospaced");
			return;

		case PROP_STRIKETHROUGH:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_CONTENT_EDITOR_STYLE_IS_STRIKETHROUGH,
				g_value_get_boolean (value),
				"DOMSelectionSetStrikethrough");
			return;

		case PROP_SUBSCRIPT:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_CONTENT_EDITOR_STYLE_IS_SUBSCRIPT,
				g_value_get_boolean (value),
				"DOMSelectionSetSubscript");
			return;

		case PROP_SUPERSCRIPT:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_CONTENT_EDITOR_STYLE_IS_SUPERSCRIPT,
				g_value_get_boolean (value),
				"DOMSelectionSetSuperscript");
			return;

		case PROP_UNDERLINE:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_CONTENT_EDITOR_STYLE_IS_UNDERLINE,
				g_value_get_boolean (value),
				"DOMSelectionSetUnderline");
			return;

		case PROP_SPELL_CHECK_ENABLED:
			webkit_editor_set_spell_check_enabled (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
webkit_editor_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEB_EXTENSION:
			g_value_set_object (
				value, webkit_editor_get_web_extension (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_COPY:
			g_value_set_boolean (
				value, webkit_editor_can_copy (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_CUT:
			g_value_set_boolean (
				value, webkit_editor_can_cut (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_PASTE:
			g_value_set_boolean (
				value, webkit_editor_can_paste (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, webkit_editor_can_redo (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, webkit_editor_can_undo (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value, webkit_editor_get_changed (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_HTML_MODE:
			g_value_set_boolean (
				value, webkit_editor_get_html_mode (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value, webkit_editor_is_editable (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_ALIGNMENT:
			g_value_set_enum (
				value,
				webkit_editor_get_alignment (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_BACKGROUND_COLOR:
			g_value_set_boxed (
				value,
				webkit_editor_get_background_color (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_BLOCK_FORMAT:
			g_value_set_enum (
				value,
				webkit_editor_get_block_format (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_BOLD:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_CONTENT_EDITOR_STYLE_IS_BOLD));
			return;

		case PROP_FONT_COLOR:
			g_value_set_boxed (
				value,
				webkit_editor_get_font_color (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_FONT_NAME:
			g_value_set_string (
				value,
				webkit_editor_get_font_name (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_FONT_SIZE:
			g_value_set_int (
				value,
				webkit_editor_get_font_size (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_INDENTED:
			g_value_set_boolean (
				value,
				webkit_editor_selection_is_indented (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_ITALIC:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_CONTENT_EDITOR_STYLE_IS_ITALIC));
			return;

		case PROP_MONOSPACED:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_CONTENT_EDITOR_STYLE_IS_MONOSPACE));
			return;

		case PROP_STRIKETHROUGH:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_CONTENT_EDITOR_STYLE_IS_STRIKETHROUGH));
			return;

		case PROP_SUBSCRIPT:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_CONTENT_EDITOR_STYLE_IS_SUBSCRIPT));
			return;

		case PROP_SUPERSCRIPT:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_CONTENT_EDITOR_STYLE_IS_SUPERSCRIPT));
			return;

		case PROP_UNDERLINE:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_CONTENT_EDITOR_STYLE_IS_UNDERLINE));
			return;

		case PROP_SPELL_CHECK_ENABLED:
			g_value_set_boolean (
				value,
				webkit_editor_get_spell_check_enabled (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_SPELL_CHECKER:
			g_value_set_object (
				value,
				webkit_editor_get_spell_checker (
					E_WEBKIT_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
webkit_editor_move_caret_on_current_coordinates (GtkWidget *widget)
{
	gint x, y;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (
		gtk_widget_get_window (widget), pointer, &x, &y, NULL);
	webkit_editor_move_caret_on_coordinates
		(E_CONTENT_EDITOR (widget), x, y, TRUE);
}

static void
webkit_editor_settings_changed_cb (GSettings *settings,
                                   const gchar *key,
                                   EWebKitEditor *wk_editor)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (wk_editor->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (wk_editor->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (wk_editor->priv->old_settings, key);

		webkit_editor_update_styles (E_CONTENT_EDITOR (wk_editor));
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static void
webkit_editor_load_changed_cb (EWebKitEditor *wk_editor,
                               WebKitLoadEvent load_event)
{
	wk_editor->priv->webkit_load_event = load_event;

	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	wk_editor->priv->reload_in_progress = FALSE;

	if (webkit_editor_is_ready (E_CONTENT_EDITOR (wk_editor))) {
		e_content_editor_emit_load_finished (E_CONTENT_EDITOR (wk_editor));
		webkit_editor_style_updated_cb (wk_editor);
	} else
		wk_editor->priv->emit_load_finished_when_extension_is_ready = TRUE;

	dispatch_pending_operations (wk_editor);
}

static void
webkit_editor_clipboard_owner_change_cb (GtkClipboard *clipboard,
                                         GdkEventOwnerChange *event,
                                         EWebKitEditor *wk_editor)
{
	if (!E_IS_WEBKIT_EDITOR (wk_editor))
		return;

	if (!wk_editor->priv->web_extension)
		return;

	if (wk_editor->priv->copy_cut_actions_triggered && event->owner)
		wk_editor->priv->copy_paste_clipboard_in_view = TRUE;
	else
		wk_editor->priv->copy_paste_clipboard_in_view = FALSE;

	if (wk_editor->priv->copy_paste_clipboard_in_view == wk_editor->priv->pasting_from_itself_extension_value)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"SetPastingContentFromItself",
		g_variant_new (
			"(tb)",
			current_page_id (wk_editor),
			wk_editor->priv->copy_paste_clipboard_in_view),
		wk_editor->priv->cancellable);

	wk_editor->priv->copy_cut_actions_triggered = FALSE;

	wk_editor->priv->pasting_from_itself_extension_value = wk_editor->priv->copy_paste_clipboard_in_view;
}

static void
webkit_editor_primary_clipboard_owner_change_cb (GtkClipboard *clipboard,
                                                 GdkEventOwnerChange *event,
                                                 EWebKitEditor *wk_editor)
{
	if (!E_IS_WEBKIT_EDITOR (wk_editor) ||
	    !wk_editor->priv->web_extension)
		return;

	if (!event->owner || !wk_editor->priv->can_copy)
		wk_editor->priv->copy_paste_clipboard_in_view = FALSE;

	if (wk_editor->priv->copy_paste_clipboard_in_view == wk_editor->priv->pasting_from_itself_extension_value)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		wk_editor->priv->web_extension,
		"SetPastingContentFromItself",
		g_variant_new (
			"(tb)",
			current_page_id (wk_editor),
			wk_editor->priv->copy_paste_clipboard_in_view),
		wk_editor->priv->cancellable);

	wk_editor->priv->pasting_from_itself_extension_value = wk_editor->priv->copy_paste_clipboard_in_view;
}

static gboolean
webkit_editor_paste_prefer_text_html (EWebKitEditor *wk_editor)
{
	if (wk_editor->priv->pasting_primary_clipboard)
		return wk_editor->priv->copy_paste_primary_in_view;
	else
		return wk_editor->priv->copy_paste_clipboard_in_view;
}

static void
webkit_editor_paste_clipboard_targets_cb (GtkClipboard *clipboard,
                                          GdkAtom *targets,
                                          gint n_targets,
                                          EWebKitEditor *wk_editor)
{
	gchar *content = NULL;
	gboolean is_html = FALSE;

	if (targets == NULL || n_targets < 0)
		return;

	/* If view doesn't have focus, focus it */
	if (!gtk_widget_has_focus (GTK_WIDGET (wk_editor)))
		gtk_widget_grab_focus (GTK_WIDGET (wk_editor));

	/* Save the text content before we try to insert the image as it could
	 * happen that we fail to save the image from clipboard (not by our
	 * fault - right now it looks like current web engines can't handle IMG
	 * with SRCSET attribute in clipboard correctly). And if this fails the
	 * source application can cancel the content and we could not fallback
	 * to at least some content. */
	if (wk_editor->priv->html_mode ||
	    webkit_editor_paste_prefer_text_html (wk_editor)) {
		if (e_targets_include_html (targets, n_targets)) {
			content = e_clipboard_wait_for_html (clipboard);
			is_html = TRUE;
		} else if (gtk_targets_include_text (targets, n_targets))
			content = gtk_clipboard_wait_for_text (clipboard);
	} else {
		if (gtk_targets_include_text (targets, n_targets))
			content = gtk_clipboard_wait_for_text (clipboard);
		else if (e_targets_include_html (targets, n_targets)) {
			content = e_clipboard_wait_for_html (clipboard);
			is_html = TRUE;
		}
	}

	if (wk_editor->priv->html_mode &&
	    gtk_targets_include_image (targets, n_targets, TRUE)) {
		gchar *uri;

		if (!(uri = e_util_save_image_from_clipboard (clipboard)))
			goto fallback;

		webkit_editor_set_changed (wk_editor, TRUE);

		webkit_editor_insert_image (E_CONTENT_EDITOR (wk_editor), uri);

		g_free (content);
		g_free (uri);

		return;
	}

 fallback:
	/* Order is important here to ensure common use cases are
	 * handled correctly.  See GNOME bug #603715 for details. */
	/* Prefer plain text over HTML when in the plain text mode, but only
	 * when pasting content from outside the editor view. */

	if (!content || !*content) {
		g_free (content);
		return;
	}

	if (is_html)
		webkit_editor_insert_content (
			E_CONTENT_EDITOR (wk_editor),
			content,
			E_CONTENT_EDITOR_INSERT_TEXT_HTML);
	else
		webkit_editor_insert_content (
			E_CONTENT_EDITOR (wk_editor),
			content,
			E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
			E_CONTENT_EDITOR_INSERT_CONVERT);

	g_free (content);
}

static void
webkit_editor_paste_primary (EContentEditor *editor)
{

	GtkClipboard *clipboard;
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	/* Remember, that we are pasting primary clipboard to return
	 * correct value in e_html_editor_view_is_pasting_content_from_itself. */
	wk_editor->priv->pasting_primary_clipboard = TRUE;

	webkit_editor_move_caret_on_current_coordinates (GTK_WIDGET (wk_editor));

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

	gtk_clipboard_request_targets (
		clipboard, (GtkClipboardTargetsReceivedFunc)
		webkit_editor_paste_clipboard_targets_cb, wk_editor);
}

static void
webkit_editor_paste (EContentEditor *editor)
{
	GtkClipboard *clipboard;
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_targets (
		clipboard, (GtkClipboardTargetsReceivedFunc)
		webkit_editor_paste_clipboard_targets_cb, wk_editor);
}

static void
webkit_editor_mouse_target_changed_cb (EWebKitEditor *wk_editor,
                                       WebKitHitTestResult *hit_test_result,
                                       guint modifiers,
                                       gpointer user_data)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	g_free (wk_editor->priv->last_hover_uri);
	wk_editor->priv->last_hover_uri = NULL;

	if (webkit_hit_test_result_context_is_link (hit_test_result))
		wk_editor->priv->last_hover_uri = g_strdup (webkit_hit_test_result_get_link_uri (hit_test_result));
}

static gboolean
webkit_editor_context_menu_cb (EWebKitEditor *wk_editor,
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
webkit_editor_drag_begin_cb (EWebKitEditor *wk_editor,
                             GdkDragContext *context)
{
	wk_editor->priv->performing_drag = TRUE;
}

static void
webkit_editor_drag_failed_cb (EWebKitEditor *wk_editor,
                              GdkDragContext *context,
                              GtkDragResult result)
{
	wk_editor->priv->performing_drag = FALSE;
}

static void
webkit_editor_drag_end_cb (EWebKitEditor *wk_editor,
                           GdkDragContext *context)
{
	wk_editor->priv->performing_drag = FALSE;
}

static gchar *
next_uri (guchar **uri_list,
          gint *len,
          gint *list_len)
{
	guchar *uri, *begin;

	begin = *uri_list;
	*len = 0;
	while (**uri_list && **uri_list != '\n' && **uri_list != '\r' && *list_len) {
		(*uri_list) ++;
		(*len) ++;
		(*list_len) --;
	}

	uri = (guchar *) g_strndup ((gchar *) begin, *len);

	while ((!**uri_list || **uri_list == '\n' || **uri_list == '\r') && *list_len) {
		(*uri_list) ++;
		(*list_len) --;
	}

	return (gchar *) uri;
}

static void
webkit_editor_drag_data_received_cb (GtkWidget *widget,
                                     GdkDragContext *context,
                                     gint x,
                                     gint y,
                                     GtkSelectionData *selection,
                                     guint info,
                                     guint time)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (widget);
	gboolean is_move = FALSE;

	g_signal_handler_disconnect (wk_editor, wk_editor->priv->drag_data_received_handler_id);
	wk_editor->priv->drag_data_received_handler_id = 0;

	is_move = gdk_drag_context_get_selected_action(context) == GDK_ACTION_MOVE;

	/* Leave DnD inside the view on WebKit */
	/* Leave the text on WebKit to handle it. */
	if (wk_editor->priv->performing_drag ||
	    info == E_DND_TARGET_TYPE_UTF8_STRING || info == E_DND_TARGET_TYPE_STRING ||
	    info == E_DND_TARGET_TYPE_TEXT_PLAIN || info == E_DND_TARGET_TYPE_TEXT_PLAIN_UTF8) {
		gdk_drag_status (context, gdk_drag_context_get_selected_action(context), time);
		GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->drag_drop (widget, context, x, y, time);
		g_signal_stop_emission_by_name (widget, "drag-data-received");
		if (!is_move)
			webkit_editor_call_simple_extension_function (wk_editor, "DOMLastDropOperationDidCopy");
		return;
	}

	if (info == E_DND_TARGET_TYPE_TEXT_HTML) {
		const guchar *data;
		gint length;
		gint list_len, len;
		gchar *text;

		data = gtk_selection_data_get_data (selection);
		length = gtk_selection_data_get_length (selection);

		if (!data || length < 0) {
			gtk_drag_finish (context, FALSE, is_move, time);
			g_signal_stop_emission_by_name (widget, "drag-data-received");
			return;
		}

		webkit_editor_move_caret_on_coordinates (E_CONTENT_EDITOR (widget), x, y, FALSE);

		list_len = length;
		do {
			text = next_uri ((guchar **) &data, &len, &list_len);
			webkit_editor_insert_content (
				E_CONTENT_EDITOR (wk_editor),
				text,
				E_CONTENT_EDITOR_INSERT_TEXT_HTML);
			g_free (text);
		} while (list_len);

		gtk_drag_finish (context, TRUE, is_move, time);
		g_signal_stop_emission_by_name (widget, "drag-data-received");
		return;
	}
}

static gboolean
webkit_editor_drag_drop_cb (EWebKitEditor *wk_editor,
                            GdkDragContext *context,
                            gint x,
                            gint y,
                            guint time)
{
	wk_editor->priv->drag_data_received_handler_id = g_signal_connect (
		wk_editor, "drag-data-received",
		G_CALLBACK (webkit_editor_drag_data_received_cb), NULL);

	webkit_editor_set_changed (wk_editor, TRUE);

	return FALSE;
}

static void
webkit_editor_web_process_crashed_cb (EWebKitEditor *wk_editor)
{
	GtkWidget *widget;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	widget = GTK_WIDGET (wk_editor);
	while (widget) {
		if (E_IS_ALERT_SINK (widget)) {
			e_alert_submit (E_ALERT_SINK (widget), "mail-composer:webkit-web-process-crashed", NULL);
			break;
		}

		if (E_IS_MSG_COMPOSER (widget)) {
			EHTMLEditor *html_editor;

			html_editor = e_msg_composer_get_editor (E_MSG_COMPOSER (widget));
			if (html_editor) {
				e_alert_submit (E_ALERT_SINK (html_editor), "mail-composer:webkit-web-process-crashed", NULL);
				break;
			}
		}

		widget = gtk_widget_get_parent (widget);
	}

	/* No suitable EAlertSink found as the parent widget */
	if (!widget) {
		g_warning (
			"WebKitWebProcess (page id %ld) for EWebKitEditor crashed",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (wk_editor)));
	}
}

static void
paste_quote_text (EContentEditor *editor,
		  const gchar *text,
		  gboolean is_html)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	e_content_editor_insert_content (
		editor,
		text,
		E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT |
		(is_html ? E_CONTENT_EDITOR_INSERT_TEXT_HTML : E_CONTENT_EDITOR_INSERT_TEXT_PLAIN));
}

static void
clipboard_html_received_for_paste_quote (GtkClipboard *clipboard,
                                         const gchar *text,
                                         gpointer user_data)
{
	EContentEditor *editor = user_data;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	paste_quote_text (editor, text, TRUE);
}

static void
clipboard_text_received_for_paste_quote (GtkClipboard *clipboard,
                                         const gchar *text,
                                         gpointer user_data)
{
	EContentEditor *editor = user_data;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	paste_quote_text (editor, text, FALSE);
}

static void
paste_primary_clipboard_quoted (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GtkClipboard *clipboard;

	wk_editor = E_WEBKIT_EDITOR (editor);

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_PRIMARY);

       if (wk_editor->priv->html_mode) {
               if (e_clipboard_wait_is_html_available (clipboard))
                       e_clipboard_request_html (clipboard, clipboard_html_received_for_paste_quote, editor);
               else if (gtk_clipboard_wait_is_text_available (clipboard))
                       gtk_clipboard_request_text (clipboard, clipboard_text_received_for_paste_quote, editor);
       } else {
               if (gtk_clipboard_wait_is_text_available (clipboard))
                       gtk_clipboard_request_text (clipboard, clipboard_text_received_for_paste_quote, editor);
               else if (e_clipboard_wait_is_html_available (clipboard))
                       e_clipboard_request_html (clipboard, clipboard_html_received_for_paste_quote, editor);
       }
}

static gboolean
webkit_editor_button_press_event (GtkWidget *widget,
                                  GdkEventButton *event)
{
	EWebKitEditor *wk_editor;

	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (widget), FALSE);

	wk_editor = E_WEBKIT_EDITOR (widget);

	if (event->button == 2) {
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			paste_primary_clipboard_quoted (E_CONTENT_EDITOR (widget));
		} else if (!e_content_editor_emit_paste_primary_clipboard (E_CONTENT_EDITOR (widget)))
			webkit_editor_paste_primary (E_CONTENT_EDITOR( (widget)));

		return TRUE;
	}

	/* Ctrl + Left Click on link opens it. */
	if (event->button == 1 && wk_editor->priv->last_hover_uri &&
	    (event->state & GDK_CONTROL_MASK) != 0 &&
	    (event->state & GDK_SHIFT_MASK) == 0 &&
	    (event->state & GDK_MOD1_MASK) == 0) {
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (wk_editor));

		e_show_uri (GTK_WINDOW (toplevel), wk_editor->priv->last_hover_uri);
	}

	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->button_press_event (widget, event);
}

static gboolean
webkit_editor_key_press_event (GtkWidget *widget,
                               GdkEventKey *event)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (widget);

	if ((((event)->state & GDK_SHIFT_MASK) &&
	    ((event)->keyval == GDK_KEY_Insert)) ||
	    (((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->keyval == GDK_KEY_v))) {
		if (!e_content_editor_emit_paste_clipboard (E_CONTENT_EDITOR (widget)))
			webkit_editor_paste (E_CONTENT_EDITOR (widget));
		return TRUE;
	}

	if ((((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->keyval == GDK_KEY_Insert)) ||
	    (((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->keyval == GDK_KEY_c))) {
		webkit_editor_copy (E_CONTENT_EDITOR (wk_editor));
		return TRUE;
	}

	if (((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->keyval == GDK_KEY_z)) {
		webkit_editor_undo (E_CONTENT_EDITOR (wk_editor));
		return TRUE;
	}

	if (((event)->state & (GDK_CONTROL_MASK)) &&
	    ((event)->keyval == GDK_KEY_Z)) {
		webkit_editor_redo (E_CONTENT_EDITOR (wk_editor));
		return TRUE;
	}

	if ((((event)->state & GDK_SHIFT_MASK) &&
	    ((event)->keyval == GDK_KEY_Delete)) ||
	    (((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->keyval == GDK_KEY_x))) {
		webkit_editor_cut (E_CONTENT_EDITOR (wk_editor));
		return TRUE;
	}

	if (((event)->state & GDK_CONTROL_MASK) &&
	    ((event)->state & GDK_SHIFT_MASK) &&
	    ((event)->keyval == GDK_KEY_I) &&
	    e_util_get_webkit_developer_mode_enabled ()) {
		webkit_editor_show_inspector (wk_editor);
		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->key_press_event (widget, event);
}

static void
e_webkit_editor_class_init (EWebKitEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EWebKitEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = webkit_editor_constructed;
	object_class->constructor = webkit_editor_constructor;
	object_class->get_property = webkit_editor_get_property;
	object_class->set_property = webkit_editor_set_property;
	object_class->dispose = webkit_editor_dispose;
	object_class->finalize = webkit_editor_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = webkit_editor_button_press_event;
	widget_class->key_press_event = webkit_editor_key_press_event;

	g_object_class_install_property (
		object_class,
		PROP_WEB_EXTENSION,
		g_param_spec_object (
			"web-extension",
			"Web Extension",
			"The Web Extension to use to talk to the WebProcess",
			G_TYPE_DBUS_PROXY,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

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
		object_class, PROP_SPELL_CHECK_ENABLED, "spell-check-enabled");
	g_object_class_override_property (
		object_class, PROP_SPELL_CHECKER, "spell-checker");
}

static void
e_webkit_editor_init (EWebKitEditor *wk_editor)
{
	GSettings *g_settings;
	GSettingsSchema *settings_schema;

	wk_editor->priv = E_WEBKIT_EDITOR_GET_PRIVATE (wk_editor);

	/* To be able to cancel any pending calls when 'dispose' is called. */
	wk_editor->priv->cancellable = g_cancellable_new ();
	wk_editor->priv->spell_check_enabled = TRUE;
	wk_editor->priv->spell_checker = e_spell_checker_new ();
	wk_editor->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	g_signal_connect (
		wk_editor, "load-changed",
		G_CALLBACK (webkit_editor_load_changed_cb), NULL);

	g_signal_connect (
		wk_editor, "context-menu",
		G_CALLBACK (webkit_editor_context_menu_cb), NULL);

	g_signal_connect (
		wk_editor, "mouse-target-changed",
		G_CALLBACK (webkit_editor_mouse_target_changed_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-begin",
		G_CALLBACK (webkit_editor_drag_begin_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-failed",
		G_CALLBACK (webkit_editor_drag_failed_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-end",
		G_CALLBACK (webkit_editor_drag_end_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-drop",
		G_CALLBACK (webkit_editor_drag_drop_cb), NULL);

	g_signal_connect (
		wk_editor, "web-process-crashed",
		G_CALLBACK (webkit_editor_web_process_crashed_cb), NULL);

	g_signal_connect (
		wk_editor, "style-updated",
		G_CALLBACK (webkit_editor_style_updated_cb), NULL);

	g_signal_connect (
		wk_editor, "state-flags-changed",
		G_CALLBACK (webkit_editor_style_updated_cb), NULL);

	wk_editor->priv->owner_change_primary_clipboard_cb_id = g_signal_connect (
		gtk_clipboard_get (GDK_SELECTION_PRIMARY), "owner-change",
		G_CALLBACK (webkit_editor_primary_clipboard_owner_change_cb), wk_editor);

	wk_editor->priv->owner_change_clipboard_cb_id = g_signal_connect (
		gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), "owner-change",
		G_CALLBACK (webkit_editor_clipboard_owner_change_cb), wk_editor);

	g_settings = e_util_ref_settings ("org.gnome.desktop.interface");
	g_signal_connect (
		g_settings, "changed::font-name",
		G_CALLBACK (webkit_editor_settings_changed_cb), wk_editor);
	g_signal_connect (
		g_settings, "changed::monospace-font-name",
		G_CALLBACK (webkit_editor_settings_changed_cb), wk_editor);
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
			G_CALLBACK (webkit_editor_settings_changed_cb), wk_editor);
		wk_editor->priv->aliasing_settings = g_settings;
	}

	wk_editor->priv->html_mode = TRUE;
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
	wk_editor->priv->pasting_from_itself_extension_value = FALSE;
	wk_editor->priv->current_user_stylesheet = NULL;
	wk_editor->priv->emit_load_finished_when_extension_is_ready = FALSE;
	wk_editor->priv->suppress_color_changes = FALSE;

	wk_editor->priv->font_color = gdk_rgba_copy (&black);
	wk_editor->priv->background_color = gdk_rgba_copy (&white);
	wk_editor->priv->font_name = NULL;
	wk_editor->priv->font_size = E_CONTENT_EDITOR_FONT_SIZE_NORMAL;
	wk_editor->priv->block_format = E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH;
	wk_editor->priv->alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;

	wk_editor->priv->web_extension_selection_changed_cb_id = 0;
	wk_editor->priv->web_extension_content_changed_cb_id = 0;
	wk_editor->priv->web_extension_undo_redo_state_changed_cb_id = 0;
	wk_editor->priv->web_extension_user_changed_default_colors_cb_id = 0;
}

static void
e_webkit_editor_content_editor_init (EContentEditorInterface *iface)
{
	iface->initialize = webkit_editor_initialize;
	iface->update_styles = webkit_editor_update_styles;
	iface->insert_content = webkit_editor_insert_content;
	iface->get_content = webkit_editor_get_content;
	iface->insert_image = webkit_editor_insert_image;
	iface->insert_image_from_mime_part = webkit_editor_insert_image_from_mime_part;
	iface->insert_emoticon = webkit_editor_insert_emoticon;
	iface->move_caret_on_coordinates = webkit_editor_move_caret_on_coordinates;
	iface->cut = webkit_editor_cut;
	iface->copy = webkit_editor_copy;
	iface->paste = webkit_editor_paste;
	iface->paste_primary = webkit_editor_paste_primary;
	iface->undo = webkit_editor_undo;
	iface->redo = webkit_editor_redo;
	iface->clear_undo_redo_history = webkit_editor_clear_undo_redo_history;
	iface->set_spell_checking_languages = webkit_editor_set_spell_checking_languages;
	/* FIXME WK2 iface->get_selected_text = webkit_editor_get_selected_text; */
	iface->get_caret_word = webkit_editor_get_caret_word;
	iface->replace_caret_word = webkit_editor_replace_caret_word;
	iface->select_all = webkit_editor_select_all;
	iface->selection_indent = webkit_editor_selection_indent;
	iface->selection_unindent = webkit_editor_selection_unindent;
	/* FIXME WK2 iface->create_link = webkit_editor_create_link; */
	iface->selection_unlink = webkit_editor_selection_unlink;
	iface->find = webkit_editor_find;
	iface->replace = webkit_editor_replace;
	iface->replace_all = webkit_editor_replace_all;
	iface->selection_save = webkit_editor_selection_save;
	iface->selection_restore = webkit_editor_selection_restore;
	iface->selection_wrap = webkit_editor_selection_wrap;
	iface->get_caret_position = webkit_editor_get_caret_position;
	iface->get_caret_offset = webkit_editor_get_caret_offset;
	iface->get_current_signature_uid =  webkit_editor_get_current_signature_uid;
	iface->is_ready = webkit_editor_is_ready;
	iface->insert_signature = webkit_editor_insert_signature;
	iface->delete_cell_contents = webkit_editor_delete_cell_contents;
	iface->delete_column = webkit_editor_delete_column;
	iface->delete_row = webkit_editor_delete_row;
	iface->delete_table = webkit_editor_delete_table;
	iface->insert_column_after = webkit_editor_insert_column_after;
	iface->insert_column_before = webkit_editor_insert_column_before;
	iface->insert_row_above = webkit_editor_insert_row_above;
	iface->insert_row_below = webkit_editor_insert_row_below;
	iface->on_h_rule_dialog_open = webkit_editor_on_h_rule_dialog_open;
	iface->on_h_rule_dialog_close = webkit_editor_on_h_rule_dialog_close;
	iface->h_rule_set_align = webkit_editor_h_rule_set_align;
	iface->h_rule_get_align = webkit_editor_h_rule_get_align;
	iface->h_rule_set_size = webkit_editor_h_rule_set_size;
	iface->h_rule_get_size = webkit_editor_h_rule_get_size;
	iface->h_rule_set_width = webkit_editor_h_rule_set_width;
	iface->h_rule_get_width = webkit_editor_h_rule_get_width;
	iface->h_rule_set_no_shade = webkit_editor_h_rule_set_no_shade;
	iface->h_rule_get_no_shade = webkit_editor_h_rule_get_no_shade;
	iface->on_image_dialog_open = webkit_editor_on_image_dialog_open;
	iface->on_image_dialog_close = webkit_editor_on_image_dialog_close;
	iface->image_set_src = webkit_editor_image_set_src;
	iface->image_get_src = webkit_editor_image_get_src;
	iface->image_set_alt = webkit_editor_image_set_alt;
	iface->image_get_alt = webkit_editor_image_get_alt;
	iface->image_set_url = webkit_editor_image_set_url;
	iface->image_get_url = webkit_editor_image_get_url;
	iface->image_set_vspace = webkit_editor_image_set_vspace;
	iface->image_get_vspace = webkit_editor_image_get_vspace;
	iface->image_set_hspace = webkit_editor_image_set_hspace;
	iface->image_get_hspace = webkit_editor_image_get_hspace;
	iface->image_set_border = webkit_editor_image_set_border;
	iface->image_get_border = webkit_editor_image_get_border;
	iface->image_set_align = webkit_editor_image_set_align;
	iface->image_get_align = webkit_editor_image_get_align;
	iface->image_get_natural_width = webkit_editor_image_get_natural_width;
	iface->image_get_natural_height = webkit_editor_image_get_natural_height;
	iface->image_set_height = webkit_editor_image_set_height;
	iface->image_set_width = webkit_editor_image_set_width;
	iface->image_set_height_follow = webkit_editor_image_set_height_follow;
	iface->image_set_width_follow = webkit_editor_image_set_width_follow;
	iface->image_get_width = webkit_editor_image_get_width;
	iface->image_get_height = webkit_editor_image_get_height;
	iface->on_link_dialog_open = webkit_editor_on_link_dialog_open;
	iface->on_link_dialog_close = webkit_editor_on_link_dialog_close;
	iface->link_set_values = webkit_editor_link_set_values;
	iface->link_get_values = webkit_editor_link_get_values;
	iface->page_set_text_color = webkit_editor_page_set_text_color;
	iface->page_get_text_color = webkit_editor_page_get_text_color;
	iface->page_set_background_color = webkit_editor_page_set_background_color;
	iface->page_get_background_color = webkit_editor_page_get_background_color;
	iface->page_set_link_color = webkit_editor_page_set_link_color;
	iface->page_get_link_color = webkit_editor_page_get_link_color;
	iface->page_set_visited_link_color = webkit_editor_page_set_visited_link_color;
	iface->page_get_visited_link_color = webkit_editor_page_get_visited_link_color;
	iface->page_set_background_image_uri = webkit_editor_page_set_background_image_uri;
	iface->page_get_background_image_uri = webkit_editor_page_get_background_image_uri;
	iface->on_page_dialog_open = webkit_editor_on_page_dialog_open;
	iface->on_page_dialog_close = webkit_editor_on_page_dialog_close;
	iface->on_cell_dialog_open = webkit_editor_on_cell_dialog_open;
	iface->on_cell_dialog_close = webkit_editor_on_cell_dialog_close;
	iface->cell_set_v_align = webkit_editor_cell_set_v_align;
	iface->cell_get_v_align = webkit_editor_cell_get_v_align;
	iface->cell_set_align = webkit_editor_cell_set_align;
	iface->cell_get_align = webkit_editor_cell_get_align;
	iface->cell_set_wrap = webkit_editor_cell_set_wrap;
	iface->cell_get_wrap = webkit_editor_cell_get_wrap;
	iface->cell_set_header_style = webkit_editor_cell_set_header_style;
	iface->cell_is_header = webkit_editor_cell_is_header;
	iface->cell_get_width = webkit_editor_cell_get_width;
	iface->cell_set_width = webkit_editor_cell_set_width;
	iface->cell_get_row_span = webkit_editor_cell_get_row_span;
	iface->cell_set_row_span = webkit_editor_cell_set_row_span;
	iface->cell_get_col_span = webkit_editor_cell_get_col_span;
	iface->cell_set_col_span = webkit_editor_cell_set_col_span;
	iface->cell_get_background_image_uri = webkit_editor_cell_get_background_image_uri;
	iface->cell_set_background_image_uri = webkit_editor_cell_set_background_image_uri;
	iface->cell_get_background_color = webkit_editor_cell_get_background_color;
	iface->cell_set_background_color = webkit_editor_cell_set_background_color;
	iface->table_set_row_count = webkit_editor_table_set_row_count;
	iface->table_get_row_count = webkit_editor_table_get_row_count;
	iface->table_set_column_count = webkit_editor_table_set_column_count;
	iface->table_get_column_count = webkit_editor_table_get_column_count;
	iface->table_set_width = webkit_editor_table_set_width;
	iface->table_get_width = webkit_editor_table_get_width;
	iface->table_set_align = webkit_editor_table_set_align;
	iface->table_get_align = webkit_editor_table_get_align;
	iface->table_set_padding = webkit_editor_table_set_padding;
	iface->table_get_padding = webkit_editor_table_get_padding;
	iface->table_set_spacing = webkit_editor_table_set_spacing;
	iface->table_get_spacing = webkit_editor_table_get_spacing;
	iface->table_set_border = webkit_editor_table_set_border;
	iface->table_get_border = webkit_editor_table_get_border;
	iface->table_get_background_image_uri = webkit_editor_table_get_background_image_uri;
	iface->table_set_background_image_uri = webkit_editor_table_set_background_image_uri;
	iface->table_get_background_color = webkit_editor_table_get_background_color;
	iface->table_set_background_color = webkit_editor_table_set_background_color;
	iface->on_table_dialog_open = webkit_editor_on_table_dialog_open;
	iface->on_table_dialog_close = webkit_editor_on_table_dialog_close;
	iface->on_spell_check_dialog_open = webkit_editor_on_spell_check_dialog_open;
	iface->on_spell_check_dialog_close = webkit_editor_on_spell_check_dialog_close;
	iface->spell_check_next_word = webkit_editor_spell_check_next_word;
	iface->spell_check_prev_word = webkit_editor_spell_check_prev_word;
	iface->on_replace_dialog_open = webkit_editor_on_replace_dialog_open;
	iface->on_replace_dialog_close = webkit_editor_on_replace_dialog_close;
	iface->on_find_dialog_open = webkit_editor_on_find_dialog_open;
	iface->on_find_dialog_close = webkit_editor_on_find_dialog_close;
}
