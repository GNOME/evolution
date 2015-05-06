/*
 * e-html-editor-view.c
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-html-editor-view.h"
#include "e-html-editor.h"
#include "e-emoticon-chooser.h"
#include "e-misc-utils.h"

#include <web-extensions/e-html-editor-web-extension-names.h>

#include <e-util/e-util.h>
#include <e-util/e-marshal.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#define E_HTML_EDITOR_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_VIEW, EHTMLEditorViewPrivate))

/**
 * EHTMLEditorView:
 *
 * The #EHTMLEditorView is a WebKit-based rich text editor. The view itself
 * only provides means to configure global behavior of the editor. To work
 * with the actual content, current cursor position or current selection,
 * use #EHTMLEditorSelection object.
 */

struct _EHTMLEditorViewPrivate {
	gint changed		: 1;
	gint inline_spelling	: 1;
	gint magic_links	: 1;
	gint magic_smileys	: 1;
	gint unicode_smileys	: 1;
	gint can_copy		: 1;
	gint can_cut		: 1;
	gint can_paste		: 1;
	gint can_redo		: 1;
	gint can_undo		: 1;
	gint reload_in_progress : 1;
	gint html_mode		: 1;

	EHTMLEditorSelection *selection;

	GSettings *mail_settings;
	GSettings *font_settings;
	GSettings *aliasing_settings;

	gboolean convert_in_situ;
	gboolean body_input_event_removed;
	gboolean is_message_from_draft;
	gboolean is_message_from_edit_as_new;
	gboolean is_message_from_selection;
	gboolean remove_initial_input_line;

	GDBusProxy *web_extension;
	guint web_extension_watch_name_id;

	GHashTable *old_settings;

	GQueue *post_reload_operations;
};

enum {
	PROP_0,
	PROP_CAN_COPY,
	PROP_CAN_CUT,
	PROP_CAN_PASTE,
	PROP_CAN_REDO,
	PROP_CAN_UNDO,
	PROP_CHANGED,
	PROP_HTML_MODE,
	PROP_INLINE_SPELLING,
	PROP_MAGIC_LINKS,
	PROP_MAGIC_SMILEYS,
	PROP_UNICODE_SMILEYS,
	PROP_SPELL_CHECKER
};

enum {
	POPUP_EVENT,
	PASTE_PRIMARY_CLIPBOARD,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef void (*PostReloadOperationFunc) (EHTMLEditorView *view, gpointer data);

typedef struct {
	PostReloadOperationFunc func;
	gpointer data;
	GDestroyNotify data_free_func;
} PostReloadOperation;

G_DEFINE_TYPE_WITH_CODE (
	EHTMLEditorView,
	e_html_editor_view,
	WEBKIT_TYPE_WEB_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
html_editor_view_queue_post_reload_operation (EHTMLEditorView *view,
                                              PostReloadOperationFunc func,
                                              gpointer data,
                                              GDestroyNotify data_free_func)
{
	PostReloadOperation *op;

	g_return_if_fail (func != NULL);

	if (view->priv->post_reload_operations == NULL)
		view->priv->post_reload_operations = g_queue_new ();

	op = g_new0 (PostReloadOperation, 1);
	op->func = func;
	op->data = data;
	op->data_free_func = data_free_func;

	g_queue_push_head (view->priv->post_reload_operations, op);
}

static void
html_editor_view_can_redo_cb (WebKitWebView *webkit_web_view,
                              GAsyncResult *result,
                              EHTMLEditorView *view)
{
	gboolean value;

/* FIXME WK2 Connect to extension */
	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_redo != value) {
		view->priv->can_redo = value;
		g_object_notify (G_OBJECT (view), "can-redo");
	}
}

static gboolean
html_editor_view_can_redo (EHTMLEditorView *view)
{
	return view->priv->can_redo;
}

static void
html_editor_view_can_undo_cb (WebKitWebView *webkit_web_view,
                              GAsyncResult *result,
                              EHTMLEditorView *view)
{
	gboolean value;

/* FIXME WK2 Connect to extension */
	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_undo != value) {
		view->priv->can_undo = value;
		g_object_notify (G_OBJECT (view), "can-undo");
	}
}

static gboolean
html_editor_view_can_undo (EHTMLEditorView *view)
{
	return view->priv->can_undo;
}

void
e_html_editor_view_undo (EHTMLEditorView *view)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	e_html_editor_view_call_simple_extension_function (view, "DOMUndo");
}

void
e_html_editor_view_redo (EHTMLEditorView *view)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	e_html_editor_view_call_simple_extension_function (view, "DOMUndo");
}

static void
html_editor_view_user_changed_contents_cb (EHTMLEditorView *view)
{
	e_html_editor_view_set_changed (view, TRUE);
}

static void
html_editor_view_can_copy_cb (WebKitWebView *webkit_web_view,
                              GAsyncResult *result,
                              EHTMLEditorView *view)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_copy != value) {
		view->priv->can_copy = value;
		g_object_notify (G_OBJECT (view), "can-copy");
	}
}

static gboolean
html_editor_view_can_copy (EHTMLEditorView *view)
{
	return view->priv->can_copy;
}

static void
html_editor_view_can_paste_cb (WebKitWebView *webkit_web_view,
                              GAsyncResult *result,
                              EHTMLEditorView *view)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_paste != value) {
		view->priv->can_paste = value;
		g_object_notify (G_OBJECT (view), "can-paste");
	}
}

static gboolean
html_editor_view_can_paste (EHTMLEditorView *view)
{
	return view->priv->can_paste;
}

static void
html_editor_view_can_cut_cb (WebKitWebView *webkit_web_view,
                             GAsyncResult *result,
                             EHTMLEditorView *view)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_cut != value) {
		view->priv->can_cut = value;
		g_object_notify (G_OBJECT (view), "can-cut");
	}
}

static gboolean
html_editor_view_can_cut (EHTMLEditorView *view)
{
	return view->priv->can_cut;
}

static void
html_editor_view_selection_changed_cb (EHTMLEditorView *view)
{
	WebKitWebView *web_view;

	web_view = WEBKIT_WEB_VIEW (view);

	/* When the webview is being (re)loaded, the document is in an
	 * inconsistant state and there is no selection, so don't propagate
	 * the signal further to EHTMLEditorSelection and others and wait until
	 * the load is finished. */
	if (view->priv->reload_in_progress) {
		/* FIXME WK2
		g_signal_stop_emission_by_name (view, "selection-changed"); */
		return;
	}

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_COPY,
		NULL, /* cancellable */
		(GAsyncReadyCallback) html_editor_view_can_copy_cb,
		view);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_CUT,
		NULL, /* cancellable */
		(GAsyncReadyCallback) html_editor_view_can_cut_cb,
		view);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_PASTE,
		NULL, /* cancellable */
		(GAsyncReadyCallback) html_editor_view_can_paste_cb,
		view);
}

static void
insert_and_convert_html_into_selection (EHTMLEditorView *view,
                                        const gchar *text,
                                        gboolean is_html)
{
	GDBusProxy *web_extension;

	g_return_if_fail (view != NULL);

	if (!text || !*text)
		return;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"DOMConvertAndInsertHTMLIntoSelection",
		g_variant_new (
			"(tsb)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			text,
			is_html),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
clipboard_text_received_for_paste_as_text (GtkClipboard *clipboard,
                                           const gchar *text,
                                           EHTMLEditorView *view)
{
	insert_and_convert_html_into_selection (view, text, TRUE);
}

static void
clipboard_text_received (GtkClipboard *clipboard,
                         const gchar *text,
                         EHTMLEditorView *view)
{
	GDBusProxy *web_extension;

	g_return_if_fail (view != NULL);

	if (!text || !*text)
		return;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"DOMQuoteAndInsertTextIntoSelection",
		g_variant_new (
			"(tsb)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			text),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_view_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			e_html_editor_view_set_changed (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_HTML_MODE:
			e_html_editor_view_set_html_mode (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_INLINE_SPELLING:
			e_html_editor_view_set_inline_spelling (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_LINKS:
			e_html_editor_view_set_magic_links (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_SMILEYS:
			e_html_editor_view_set_magic_smileys (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_UNICODE_SMILEYS:
			e_html_editor_view_set_unicode_smileys (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_view_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_COPY:
			g_value_set_boolean (
				value, html_editor_view_can_copy (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_CUT:
			g_value_set_boolean (
				value, html_editor_view_can_cut (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_PASTE:
			g_value_set_boolean (
				value, html_editor_view_can_paste (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, html_editor_view_can_redo (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, html_editor_view_can_undo (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value, e_html_editor_view_get_changed (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_HTML_MODE:
			g_value_set_boolean (
				value, e_html_editor_view_get_html_mode (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_INLINE_SPELLING:
			g_value_set_boolean (
				value, e_html_editor_view_get_inline_spelling (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_MAGIC_LINKS:
			g_value_set_boolean (
				value, e_html_editor_view_get_magic_links (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_MAGIC_SMILEYS:
			g_value_set_boolean (
				value, e_html_editor_view_get_magic_smileys (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_UNICODE_SMILEYS:
			g_value_set_boolean (
				value, e_html_editor_view_get_unicode_smileys (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_SPELL_CHECKER:
			g_value_set_object (
				value, e_html_editor_view_get_spell_checker (
				E_HTML_EDITOR_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_view_dispose (GObject *object)
{
	EHTMLEditorViewPrivate *priv;

	priv = E_HTML_EDITOR_VIEW_GET_PRIVATE (object);

	if (priv->aliasing_settings != NULL) {
		g_signal_handlers_disconnect_by_data (priv->aliasing_settings, object);
		g_object_unref (priv->aliasing_settings);
		priv->aliasing_settings = NULL;
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

	if (priv->web_extension_watch_name_id > 0) {
		g_bus_unwatch_name (priv->web_extension_watch_name_id);
		priv->web_extension_watch_name_id = 0;
	}

	g_clear_object (&priv->selection);
	g_clear_object (&priv->web_extension);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_view_parent_class)->dispose (object);
}

static void
html_editor_view_finalize (GObject *object)
{
	EHTMLEditorViewPrivate *priv;

	priv = E_HTML_EDITOR_VIEW_GET_PRIVATE (object);

	if (priv->old_settings) {
		g_hash_table_destroy (priv->old_settings);
		priv->old_settings = NULL;
	}

	if (priv->post_reload_operations) {
		g_warn_if_fail (g_queue_is_empty (priv->post_reload_operations));

		g_queue_free (priv->post_reload_operations);
		priv->post_reload_operations = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_html_editor_view_parent_class)->finalize (object);
}

static void
html_editor_view_constructed (GObject *object)
{
	WebKitSettings *web_settings;

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_html_editor_view_parent_class)->constructed (object);

	web_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (object));

	e_html_editor_view_update_fonts (E_HTML_EDITOR_VIEW (object));

/* FIXME WK2
	g_object_set (
		G_OBJECT (web_settings),
		"enable-dom-paste", TRUE,
		"enable-file-access-from-file-uris", TRUE,
		"enable-spell-checking", TRUE,
		NULL);*/

	/* Make WebKit think we are displaying a local file, so that it
	 * does not block loading resources from file:// protocol */
	webkit_web_view_load_html (WEBKIT_WEB_VIEW (object), "", "file://");
}

void
e_html_editor_view_move_selection_on_point (EHTMLEditorView *view,
                                            gint x,
                                            gint y,
                                            gboolean cancel_if_not_collapsed)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (x >= 0);
	g_return_if_fail (y >= 0);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (web_extension)
		return;

	g_dbus_proxy_call_sync (
		web_extension,
		"DOMMoveSelectionOnPoint",
		g_variant_new (
			"(tiib)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			x,
			y,
			cancel_if_not_collapsed),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);
}

static void
html_editor_view_move_selection_on_point (GtkWidget *widget)
{
	gint x, y;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (gtk_widget_get_window (widget), pointer, &x, &y, NULL);

	e_html_editor_view_move_selection_on_point (E_HTML_EDITOR_VIEW (widget), x, y, TRUE);
}

static gboolean
html_editor_view_button_press_event (GtkWidget *widget,
                                     GdkEventButton *event)
{
	gboolean event_handled;

	if (event->button == 2) {
		/* Middle click paste */
		html_editor_view_move_selection_on_point (widget);
		g_signal_emit (widget, signals[PASTE_PRIMARY_CLIPBOARD], 0);
		event_handled = TRUE;
	} else if (event->button == 3) {
		html_editor_view_move_selection_on_point (widget);
		g_signal_emit (
			widget, signals[POPUP_EVENT],
			0, event, &event_handled);
	} else {
		event_handled = FALSE;
	}

	if (event_handled)
		return TRUE;

	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_html_editor_view_parent_class)->
		button_press_event (widget, event);
}

static void
editor_view_mouse_target_changed_cb (EHTMLEditorView *view,
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

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
		screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

		uri = webkit_hit_test_result_get_link_uri (hit_test_result);

		gtk_show_uri (screen, uri, GDK_CURRENT_TIME, NULL);
	}
}

static gboolean
html_editor_view_key_press_event (GtkWidget *widget,
                                  GdkEventKey *event)
{
	EHTMLEditorView *view = E_HTML_EDITOR_VIEW (widget);
	GDBusProxy *web_extension;
	GVariant *result;

	if (event->keyval == GDK_KEY_Menu) {
		gboolean event_handled;

		html_editor_view_move_selection_on_point (widget);
		g_signal_emit (
			widget, signals[POPUP_EVENT],
			0, event, &event_handled);

		return event_handled;
	}

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (web_extension)
		return FALSE;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMProcessOnKeyPress",
		g_variant_new (
			"(tu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			event->keyval),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gboolean ret_val = FALSE;

		ret_val = g_variant_get_boolean (result);
		g_variant_unref (result);

		if (ret_val)
			return ret_val;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_html_editor_view_parent_class)->key_press_event (widget, event);
}

static void
html_editor_view_paste_as_text (EHTMLEditorView *view)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_text (
		clipboard,
		(GtkClipboardTextReceivedFunc) clipboard_text_received_for_paste_as_text,
		view);
}

static void
html_editor_view_paste_clipboard_quoted (EHTMLEditorView *view)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_text (
		clipboard,
		(GtkClipboardTextReceivedFunc) clipboard_text_received,
		view);
}

static void
web_extension_proxy_created_cb (GDBusProxy *proxy,
                                GAsyncResult *result,
                                EHTMLEditorView *view)
{
	GError *error = NULL;

	view->priv->web_extension = g_dbus_proxy_new_finish (result, &error);
	if (!view->priv->web_extension) {
		g_warning ("Error creating web extension proxy: %s\n", error->message);
		g_error_free (error);
	}
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           EHTMLEditorView *view)
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
		view);
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           EHTMLEditorView *view)
{
	g_clear_object (&view->priv->web_extension);
}

static void
html_editor_view_watch_web_extension (EHTMLEditorView *view)
{
	view->priv->web_extension_watch_name_id =
		g_bus_watch_name (
			G_BUS_TYPE_SESSION,
			E_HTML_EDITOR_WEB_EXTENSION_SERVICE_NAME,
			G_BUS_NAME_WATCHER_FLAGS_NONE,
			(GBusNameAppearedCallback) web_extension_appeared_cb,
			(GBusNameVanishedCallback) web_extension_vanished_cb,
			view,
			NULL);
}

GDBusProxy *
e_html_editor_view_get_web_extension_proxy (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);

	return view->priv->web_extension;
}

void
e_html_editor_view_call_simple_extension_function_sync (EHTMLEditorView *view,
                                                        const gchar *function)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (function && *function);

	if (!view->priv->web_extension)
		return;

	g_dbus_proxy_call_sync (
		view->priv->web_extension,
		function,
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);
}

void
e_html_editor_view_call_simple_extension_function (EHTMLEditorView *view,
                                                   const gchar *function)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (function && *function);

	if (!view->priv->web_extension)
		return;

	g_dbus_proxy_call (
		view->priv->web_extension,
		function,
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

GVariant *
e_html_editor_view_get_element_attribute (EHTMLEditorView *view,
                                          const gchar *selector,
                                          const gchar *attribute)
{
	if (!view->priv->web_extension)
		return NULL;

	return g_dbus_proxy_call_sync (
		view->priv->web_extension,
		"ElementGetAttributeBySelector",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			selector,
			attribute),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);
}

void
e_html_editor_view_set_element_attribute (EHTMLEditorView *view,
                                          const gchar *selector,
                                          const gchar *attribute,
					  const gchar *value)
{
	if (!view->priv->web_extension)
		return;

	g_dbus_proxy_call (
		view->priv->web_extension,
		"ElementSetAttributeBySelector",
		g_variant_new (
			"(tsss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			selector,
			attribute,
			value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

void
e_html_editor_view_remove_element_attribute (EHTMLEditorView *view,
                                             const gchar *selector,
                                             const gchar *attribute)
{
	if (!view->priv->web_extension)
		return;

	g_dbus_proxy_call (
		view->priv->web_extension,
		"ElementRemoveAttributeBySelector",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			selector,
			attribute),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
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
html_editor_view_constructor (GType type,
                              guint n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
	GObjectClass* object_class;
	GParamSpec* param_spec;
	GObjectConstructParam *param = NULL;

	object_class = G_OBJECT_CLASS (g_type_class_ref(type));
	g_return_val_if_fail (object_class != NULL, NULL);

	if (construct_properties && n_construct_properties != 0) {
		param_spec = g_object_class_find_property(object_class, "settings");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, e_web_view_get_default_webkit_settings ());
		param_spec = g_object_class_find_property(object_class, "user-content-manager");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, webkit_user_content_manager_new ());
	}

	g_type_class_unref (object_class);

	return G_OBJECT_CLASS (e_html_editor_view_parent_class)->constructor(type, n_construct_properties, construct_properties);
}

static void
html_editor_view_initialize_web_context (void)
{
	WebKitWebContext *web_context = webkit_web_context_get_default ();

	webkit_web_context_set_cache_model (
		web_context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
}

static void
e_html_editor_view_class_init (EHTMLEditorViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorViewPrivate));

	html_editor_view_initialize_web_context ();

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = html_editor_view_constructor;
	object_class->get_property = html_editor_view_get_property;
	object_class->set_property = html_editor_view_set_property;
	object_class->dispose = html_editor_view_dispose;
	object_class->finalize = html_editor_view_finalize;
	object_class->constructed = html_editor_view_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = html_editor_view_button_press_event;
	widget_class->key_press_event = html_editor_view_key_press_event;

	class->paste_clipboard_quoted = html_editor_view_paste_clipboard_quoted;

	/**
	 * EHTMLEditorView:can-copy
	 *
	 * Determines whether it's possible to copy to clipboard. The action
	 * is usually disabled when there is no selection to copy.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_COPY,
		g_param_spec_boolean (
			"can-copy",
			"Can Copy",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:can-cut
	 *
	 * Determines whether it's possible to cut to clipboard. The action
	 * is usually disabled when there is no selection to cut.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_CUT,
		g_param_spec_boolean (
			"can-cut",
			"Can Cut",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:can-paste
	 *
	 * Determines whether it's possible to paste from clipboard. The action
	 * is usually disabled when there is no valid content in clipboard to
	 * paste.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_PASTE,
		g_param_spec_boolean (
			"can-paste",
			"Can Paste",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:can-redo
	 *
	 * Determines whether it's possible to redo previous action. The action
	 * is usually disabled when there is no action to redo.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_REDO,
		g_param_spec_boolean (
			"can-redo",
			"Can Redo",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:can-undo
	 *
	 * Determines whether it's possible to undo last action. The action
	 * is usually disabled when there is no previous action to undo.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_UNDO,
		g_param_spec_boolean (
			"can-undo",
			"Can Undo",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:changed
	 *
	 * Determines whether document has been modified
	 */
	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			_("Changed property"),
			_("Whether editor changed"),
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:html-mode
	 *
	 * Determines whether HTML or plain text mode is enabled.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_HTML_MODE,
		g_param_spec_boolean (
			"html-mode",
			"HTML Mode",
			"Edit HTML or plain text",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView::inline-spelling
	 *
	 * Determines whether automatic spellchecking is enabled.
	 */
	g_object_class_install_property (
		object_class,
		PROP_INLINE_SPELLING,
		g_param_spec_boolean (
			"inline-spelling",
			"Inline Spelling",
			"Check your spelling as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:magic-links
	 *
	 * Determines whether automatic conversion of text links into
	 * HTML links is enabled.
	 */
	g_object_class_install_property (
		object_class,
		PROP_MAGIC_LINKS,
		g_param_spec_boolean (
			"magic-links",
			"Magic Links",
			"Make URIs clickable as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:magic-smileys
	 *
	 * Determines whether automatic conversion of text smileys into
	 * images or Unicode characters is enabled.
	 */
	g_object_class_install_property (
		object_class,
		PROP_MAGIC_SMILEYS,
		g_param_spec_boolean (
			"magic-smileys",
			"Magic Smileys",
			"Convert emoticons to images or Unicode characters as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:unicode-smileys
	 *
	 * Determines whether Unicode characters should be used for smileys.
	 */
	g_object_class_install_property (
		object_class,
		PROP_UNICODE_SMILEYS,
		g_param_spec_boolean (
			"unicode-smileys",
			"Unicode Smileys",
			"Use Unicode characters for smileys",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
#if 0 /* FIXME WK2 */
	/**
	 * EHTMLEditorView:spell-checker:
	 *
	 * The #ESpellChecker used for spell checking.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SPELL_CHECKER,
		g_param_spec_object (
			"spell-checker",
			"Spell Checker",
			"The spell checker",
			E_TYPE_SPELL_CHECKER,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));
#endif
	/**
	 * EHTMLEditorView:popup-event
	 *
	 * Emitted whenever a context menu is requested.
	 */
	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EHTMLEditorViewClass, popup_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__BOXED,
		G_TYPE_BOOLEAN, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
	/**
	 * EHTMLEditorView:paste-primary-clipboad
	 *
	 * Emitted when user presses middle button on EHTMLEditorView
	 */
	signals[PASTE_PRIMARY_CLIPBOARD] = g_signal_new (
		"paste-primary-clipboard",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EHTMLEditorViewClass, paste_primary_clipboard),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
initialize_web_extensions_cb (WebKitWebContext *web_context)
{
	/* Set the web extensions dir before the process is launched */
	webkit_web_context_set_web_extensions_directory (
		web_context, EVOLUTION_WEB_EXTENSIONS_DIR);
}

static void
e_html_editor_settings_changed_cb (GSettings *settings,
				   const gchar *key,
				   EHTMLEditorView *view)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (view->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (view->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (view->priv->old_settings, key);

		e_html_editor_view_update_fonts (view);
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

/**
 * e_html_editor_view_new:
 *
 * Returns a new instance of the editor.
 *
 * Returns: A newly created #EHTMLEditorView. [transfer-full]
 */
EHTMLEditorView *
e_html_editor_view_new (void)
{
	return g_object_new (E_TYPE_HTML_EDITOR_VIEW, NULL);
}

/**
 * e_html_editor_view_get_selection:
 * @view: an #EHTMLEditorView
 *
 * Returns an #EHTMLEditorSelection object which represents current selection or
 * cursor position within the editor document. The #EHTMLEditorSelection allows
 * programmer to manipulate with formatting, selection, styles etc.
 *
 * Returns: An always valid #EHTMLEditorSelection object. The object is owned by
 * the @view and should never be free'd.
 */
EHTMLEditorSelection *
e_html_editor_view_get_selection (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);

	return view->priv->selection;
}

/**
 * e_html_editor_view_get_changed:
 * @view: an #EHTMLEditorView
 *
 * Whether content of the editor has been changed.
 *
 * Returns: @TRUE when document was changed, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_changed (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->changed;
}

/**
 * e_html_editor_view_set_changed:
 * @view: an #EHTMLEditorView
 * @changed: whether document has been changed or not
 *
 * Sets whether document has been changed or not. The editor is tracking changes
 * automatically, but sometimes it's necessary to change the dirty flag to force
 * "Save changes" dialog for example.
 */
void
e_html_editor_view_set_changed (EHTMLEditorView *view,
                                gboolean changed)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->changed == changed)
		return;

	view->priv->changed = changed;

	g_object_notify (G_OBJECT (view), "changed");
}

/**
 * e_html_editor_view_get_html_mode:
 * @view: an #EHTMLEditorView
 *
 * Whether the editor is in HTML mode or plain text mode. In HTML mode,
 * more formatting options are avilable an the email is sent as
 * multipart/alternative.
 *
 * Returns: @TRUE when HTML mode is enabled, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_html_mode (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->html_mode;
}

static gboolean
show_lose_formatting_dialog (EHTMLEditorView *view)
{
	gboolean lose;
	GtkWidget *toplevel;
	GtkWindow *parent = NULL;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));

	if (GTK_IS_WINDOW (toplevel))
		parent = GTK_WINDOW (toplevel);

	lose = e_util_prompt_user (
		parent, "org.gnome.evolution.mail", "prompt-on-composer-mode-switch",
		"mail-composer:prompt-composer-mode-switch", NULL);

	if (!lose) {
		/* Nothing has changed, but notify anyway */
		g_object_notify (G_OBJECT (view), "html-mode");
		return FALSE;
	}

	return TRUE;
}

static void
html_editor_view_load_changed_cb (EHTMLEditorView *view,
                                  WebKitLoadEvent load_event)
{
	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	/* Dispatch queued operations - as we are using this just for load
	 * operations load just the latest request and throw away the rest. */
	if (view->priv->post_reload_operations &&
	    !g_queue_is_empty (view->priv->post_reload_operations)) {

		PostReloadOperation *op;

		op = g_queue_pop_head (view->priv->post_reload_operations);

		op->func (view, op->data);

		if (op->data_free_func)
			op->data_free_func (op->data);
		g_free (op);

		g_queue_clear (view->priv->post_reload_operations);

		return;
	}

	view->priv->reload_in_progress = FALSE;

}

static void
set_cached_boolean_property (EHTMLEditorView *view,
                             const gchar *property_name,
                             gboolean value)
{
	GDBusProxy *web_extension;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_set_cached_property (
		web_extension, property_name, g_variant_new_boolean (value));
}

static gboolean
get_cached_boolean_property (EHTMLEditorView *view,
                             const gchar *property_name)
{
	gboolean value = FALSE;
	GVariant *result;
	GDBusProxy *web_extension;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return FALSE;

	result = g_dbus_proxy_get_cached_property (web_extension, property_name);
	if (!result)
		return FALSE;

	value = g_variant_get_boolean (result);
	g_variant_unref (result);

	return value;
}

/**
 * e_html_editor_view_set_html_mode:
 * @view: an #EHTMLEditorView
 * @html_mode: @TRUE to enable HTML mode, @FALSE to enable plain text mode
 *
 * When switching from HTML to plain text mode, user will be prompted whether
 * he/she really wants to switch the mode and lose all formatting. When user
 * declines, the property is not changed. When they accept, the all formatting
 * is lost.
 */
void
e_html_editor_view_set_html_mode (EHTMLEditorView *view,
                                  gboolean html_mode)
{
	gboolean convert = FALSE;
	GDBusProxy *web_extension;
	GVariant *result;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMCheckIfConversionNeeded",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		convert = g_variant_get_boolean (result);
		g_variant_unref (result);
	}

	/* If toggling from HTML to plain text mode, ask user first */
	if (convert && view->priv->html_mode && !html_mode) {
		if (!show_lose_formatting_dialog (view))
			return;

		view->priv->html_mode = html_mode;
		set_cached_boolean_property (view, "HTMLMode", html_mode);

		e_html_editor_view_call_simple_extension_function (
			view, "ConvertWhenChangingComposerMode");

		/* Update fonts - in plain text we only want monospace */
		e_html_editor_view_update_fonts (view);

		goto out;
	}

	if (html_mode == view->priv->html_mode)
		return;

	view->priv->html_mode = html_mode;

	e_html_editor_view_call_simple_extension_function_sync (
		view, "DOMProcessContentAfterModeChange");

	/* Update fonts - in plain text we only want monospace */
	e_html_editor_view_update_fonts (view);

 out:
	g_object_notify (G_OBJECT (view), "html-mode");
}

static void
html_editor_view_drag_end_cb (EHTMLEditorView *view,
                              GdkDragContext *context)
{
	e_html_editor_view_call_simple_extension_function (view, "DOMDragAndDropEnd");
}

static void
im_context_preedit_start_cb (GtkIMContext *context,
                             EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function_sync (
		view, "DOMIMContextPreEditStart");
}

static void
im_context_preedit_end_cb (GtkIMContext *context,
                           EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function_sync (
		view, "DOMIMContextPreEditEnd");
}

static void
e_html_editor_view_init (EHTMLEditorView *view)
{
	WebKitSettings *settings;
	GSettings *g_settings;
	GSettingsSchema *settings_schema;
/* FIXME WK2
	GtkIMContext *im_context;
	ESpellChecker *checker;*/
	gchar **languages;
	gchar *comma_separated;

	view->priv = E_HTML_EDITOR_VIEW_GET_PRIVATE (view);
/* FIXME WK2
	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (view), TRUE); */
	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));

	view->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	/* Override the spell-checker, use our own */
/* FIXME WK2
	checker = e_spell_checker_new ();
	webkit_set_text_checker (G_OBJECT (checker));
	g_object_unref (checker);

	g_signal_connect (
		view, "user-changed-contents",
		G_CALLBACK (html_editor_view_user_changed_contents_cb), NULL);
	g_signal_connect (
		view, "selection-changed",
		G_CALLBACK (html_editor_view_selection_changed_cb), NULL);
	g_signal_connect (
		view, "should-show-delete-interface-for-element",
		G_CALLBACK (html_editor_view_should_show_delete_interface_for_element), NULL);*/
	g_signal_connect (
		view, "drag-end",
		G_CALLBACK (html_editor_view_drag_end_cb), NULL);
	g_signal_connect (
		view, "load-changed",
		G_CALLBACK (html_editor_view_load_changed_cb), NULL);
	g_signal_connect (
		view, "mouse-target-changed",
		G_CALLBACK (editor_view_mouse_target_changed_cb), NULL);

	g_signal_connect (
		webkit_web_context_get_default (), "initialize-web-extensions",
		G_CALLBACK (initialize_web_extensions_cb), NULL);

	view->priv->selection = g_object_new (
		E_TYPE_HTML_EDITOR_SELECTION,
		"html-editor-view", view,
		NULL);

	html_editor_view_watch_web_extension (view);

	g_settings = e_util_ref_settings ("org.gnome.desktop.interface");
	g_signal_connect (
		g_settings, "changed::font-name",
		G_CALLBACK (e_html_editor_settings_changed_cb), view);
	g_signal_connect (
		g_settings, "changed::monospace-font-name",
		G_CALLBACK (e_html_editor_settings_changed_cb), view);
	view->priv->font_settings = g_settings;

	g_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	view->priv->mail_settings = g_settings;

	/* This schema is optional.  Use if available. */
	settings_schema = g_settings_schema_source_lookup (
		g_settings_schema_source_get_default (),
		"org.gnome.settings-daemon.plugins.xsettings", FALSE);
	if (settings_schema != NULL) {
		g_settings = e_util_ref_settings ("org.gnome.settings-daemon.plugins.xsettings");
		g_signal_connect (
			settings, "changed::antialiasing",
			G_CALLBACK (e_html_editor_settings_changed_cb), view);
		view->priv->aliasing_settings = g_settings;
	}

	/* Give spell check languages to WebKit */
/* FIXME WK2
	languages = e_spell_checker_list_active_languages (checker, NULL);
	comma_separated = g_strjoinv (",", languages);
	g_strfreev (languages);

	g_object_set (
		G_OBJECT (settings),
		"spell-checking-languages", comma_separated,
		NULL);

	g_free (comma_separated);
*/
	view->priv->body_input_event_removed = TRUE;
	view->priv->is_message_from_draft = FALSE;
	view->priv->is_message_from_selection = FALSE;
	view->priv->is_message_from_edit_as_new = FALSE;
	view->priv->remove_initial_input_line = FALSE;
	view->priv->convert_in_situ = FALSE;

/* FIXME WK2
	g_object_get (WEBKIT_WEB_VIEW (view), "im-context", &im_context, NULL);
	g_signal_connect (
		im_context, "preedit-start",
		G_CALLBACK (im_context_preedit_start_cb), view);
	g_signal_connect (
		im_context, "preedit-end",
		G_CALLBACK (im_context_preedit_end_cb), view);*/
}

void
e_html_editor_view_force_spell_check (EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function (view, "DOMForceSpellCheck");
}

void
e_html_editor_view_turn_spell_check_off (EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function (view, "DOMTurnSpellCheckOff");
}

/**
 * e_html_editor_view_get_inline_spelling:
 * @view: an #EHTMLEditorView
 *
 * Returns whether automatic spellchecking is enabled or not. When enabled,
 * editor will perform spellchecking as user is typing. Otherwise spellcheck
 * has to be run manually from menu.
 *
 * Returns: @TRUE when automatic spellchecking is enabled, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_inline_spelling (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->inline_spelling;
}

/**
 * e_html_editor_view_set_inline_spelling:
 * @view: an #EHTMLEditorView
 * @inline_spelling: @TRUE to enable automatic spellchecking, @FALSE otherwise
 *
 * Enables or disables automatic spellchecking.
 */
void
e_html_editor_view_set_inline_spelling (EHTMLEditorView *view,
                                        gboolean inline_spelling)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->inline_spelling == inline_spelling)
		return;

	view->priv->inline_spelling = inline_spelling;

/* FIXME WK2
	if (inline_spelling)
		e_html_editor_view_force_spell_check (view);
	else
		e_html_editor_view_turn_spell_check_off (view);
*/
	g_object_notify (G_OBJECT (view), "inline-spelling");
}

/**
 * e_html_editor_view_get_magic_links:
 * @view: an #EHTMLEditorView
 *
 * Returns whether automatic links conversion is enabled. When enabled, the editor
 * will automatically convert any HTTP links into clickable HTML links.
 *
 * Returns: @TRUE when magic links are enabled, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_magic_links (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->magic_links;
}

/**
 * e_html_editor_view_set_magic_links:
 * @view: an #EHTMLEditorView
 * @magic_links: @TRUE to enable magic links, @FALSE to disable them
 *
 * Enables or disables automatic links conversion.
 */
void
e_html_editor_view_set_magic_links (EHTMLEditorView *view,
                                    gboolean magic_links)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->magic_links == magic_links)
		return;

	view->priv->magic_links = magic_links;

	g_object_notify (G_OBJECT (view), "magic-links");
}

/**
 * e_html_editor_view_get_magic_smileys:
 * @view: an #EHTMLEditorView
 *
 * Returns whether automatic conversion of smileys is enabled or disabled. When
 * enabled, the editor will automatically convert text smileys ( :-), ;-),...)
 * into images or Unicode characters.
 *
 * Returns: @TRUE when magic smileys are enabled, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_magic_smileys (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->magic_smileys;
}

/**
 * e_html_editor_view_set_magic_smileys:
 * @view: an #EHTMLEditorView
 * @magic_smileys: @TRUE to enable magic smileys, @FALSE to disable them
 *
 * Enables or disables magic smileys.
 */
void
e_html_editor_view_set_magic_smileys (EHTMLEditorView *view,
                                      gboolean magic_smileys)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->magic_smileys == magic_smileys)
		return;

	view->priv->magic_smileys = magic_smileys;

	g_object_notify (G_OBJECT (view), "magic-smileys");
}

/**
 * e_html_editor_view_get_unicode_smileys:
 * @view: an #EHTMLEditorView
 *
 * Returns whether to use Unicode characters for smileys.
 *
 * Returns: @TRUE when Unicode characters should be used, @FALSE otherwise.
 *
 * Since: 3.16
 */
gboolean
e_html_editor_view_get_unicode_smileys (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->unicode_smileys;
}

/**
 * e_html_editor_view_set_unicode_smileys:
 * @view: an #EHTMLEditorView
 * @unicode_smileys: @TRUE to use Unicode characters, @FALSE to use images
 *
 * Enables or disables the usage of Unicode characters for smileys.
 *
 * Since: 3.16
 */
void
e_html_editor_view_set_unicode_smileys (EHTMLEditorView *view,
                                        gboolean unicode_smileys)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->unicode_smileys == unicode_smileys)
		return;

	view->priv->unicode_smileys = unicode_smileys;

	g_object_notify (G_OBJECT (view), "unicode-smileys");
}

/**
 * e_html_editor_view_get_spell_checker:
 * @view: an #EHTMLEditorView
 *
 * Returns an #ESpellChecker object that is used to perform spellchecking.
 *
 * Returns: An always-valid #ESpellChecker object
 */
ESpellChecker *
e_html_editor_view_get_spell_checker (EHTMLEditorView *view)
{
/* FIXME WK2
	return E_SPELL_CHECKER (webkit_get_text_checker ());*/
	return NULL;
}

static gchar *
process_document (EHTMLEditorView *view,
                  const gchar *function)
{
	GDBusProxy *web_extension;
	GVariant *result;

	g_return_val_if_fail (view != NULL, NULL);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return NULL;

	result = g_dbus_proxy_call_sync (
		web_extension,
		function,
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gchar *value;
		gsize length = 0;

		value = g_variant_dup_string (result, &length);
		g_variant_unref (result);

		return value;
	}

	return NULL;
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
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);
out:
	g_free (mime_type);
	g_free (base64_decoded_data);

	return part;
}

static GList *
html_editor_view_get_parts_for_inline_images (EHTMLEditorView *view,
                                              const gchar *uid_domain)
{
	GDBusProxy *web_extension;
	GList *parts = NULL;
	GVariant *result;

	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return NULL;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMGetInlineImagesData",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			uid_domain),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		const gchar *element_src, *name, *id;
		GVariantIter *iter;

		g_variant_get (result, "asss", &iter);
		while (g_variant_iter_loop (iter, "&s&s&s", &element_src, &name, &id)) {
			CamelMimePart *part;

			part = create_part_for_inline_image_from_element_data (
				element_src, name, id);
			parts = g_list_append (parts, part);
		}
		g_variant_iter_free (iter);
	}

	return parts;
}

/**
 * e_html_editor_view_get_text_html:
 * @view: an #EHTMLEditorView:
 *
 * Returns processed HTML content of the editor document (with elements attributes
 * used in Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_html_editor_view_get_text_html (EHTMLEditorView *view,
                                  const gchar *from_domain,
                                  GList **inline_images)
{
	GDBusProxy *web_extension;
	GVariant *result;

	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);

	if (inline_images && from_domain)
		*inline_images = html_editor_view_get_parts_for_inline_images (view, from_domain);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return NULL;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMProcessContentForHTML",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			from_domain),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gchar *value;
		gsize length = 0;

		value = g_variant_dup_string (result, &length);
		g_variant_unref (result);

		return value;
	}

	return NULL;
}

/**
 * e_html_editor_view_get_text_html_for_drafts:
 * @view: an #EHTMLEditorView:
 *
 * Returns HTML content of the editor document (without elements attributes
 * used in Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_html_editor_view_get_text_html_for_drafts (EHTMLEditorView *view)
{
	return process_document (view, "DOMProcessContentForDraft");
}

/**
 * e_html_editor_view_get_text_plain:
 * @view: an #EHTMLEditorView
 *
 * Returns plain text content of the @view. The algorithm removes any
 * formatting or styles from the document and keeps only the text and line
 * breaks.
 *
 * Returns: A newly allocated string with plain text content of the document.
 */
gchar *
e_html_editor_view_get_text_plain (EHTMLEditorView *view)
{
	return process_document (view, "DOMProcessContentForPlainText");
}

void
e_html_editor_view_convert_and_insert_plain_text (EHTMLEditorView *view,
                                                  const gchar *text)
{
	insert_and_convert_html_into_selection (view, text, FALSE);
}

void
e_html_editor_view_convert_and_insert_html_to_plain_text (EHTMLEditorView *view,
                                                          const gchar *html)
{
	insert_and_convert_html_into_selection (view, html, TRUE);
}

/**
 * e_html_editor_selection_insert_text:
 * @selection: an #EHTMLEditorSelection
 * @plain_text: text to insert
 *
 * Inserts @plain_text at current cursor position. When a text range is selected,
 * it will be replaced by @plain_text.
 */
void
e_html_editor_view_insert_text (EHTMLEditorView *view,
                                const gchar *plain_text)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (plain_text != NULL);

	e_html_editor_view_convert_and_insert_plain_text (view, plain_text);
}

void
e_html_editor_view_insert_html (EHTMLEditorView *view,
                                const gchar *html_text)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (html_text != NULL);

	if (!html_text || !*html_text)
		return;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"DOMInsertHTML",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			html_text),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

/**
 * e_html_editor_view_set_text_html:
 * @view: an #EHTMLEditorView
 * @text: HTML code to load into the editor
 *
 * Loads given @text into the editor, destroying any content already present.
 */
void
e_html_editor_view_set_text_html (EHTMLEditorView *view,
                                  const gchar *text)
{
#if 0 /* FIXME WK2 */
	WebKitLoadStatus status;

	/* It can happen that the view is not ready yet (it is in the middle of
	 * another load operation) so we have to queue the current operation and
	 * redo it again when the view is ready. This was happening when loading
	 * the stuff in EMailSignatureEditor. */
	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (view));
	if (status != WEBKIT_LOAD_FINISHED) {
		html_editor_view_queue_post_reload_operation (
			view,
			(PostReloadOperationFunc) e_html_editor_view_set_text_html,
			g_strdup (text),
			g_free);
		return;
	}
#endif
	if (view->priv->reload_in_progress) {
		html_editor_view_queue_post_reload_operation (
			view,
			(PostReloadOperationFunc) e_html_editor_view_set_text_html,
			g_strdup (text),
			g_free);
		return;
	}

	view->priv->reload_in_progress = TRUE;

	if (view->priv->is_message_from_draft) {
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), text, "file://");
		return;
	}

	if (view->priv->is_message_from_selection && !view->priv->html_mode) {
		if (text && *text)
			view->priv->convert_in_situ = TRUE;
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), text, "file://");
		return;
	}

	/* Only convert messages that are in HTML */
	if (!view->priv->html_mode) {
		if (strstr (text, "<!-- text/html -->")) {
			if (!show_lose_formatting_dialog (view)) {
				e_html_editor_view_set_html_mode (view, TRUE);
				webkit_web_view_load_html (
					WEBKIT_WEB_VIEW (view), text, "file://");
				return;
			}
		}
		if (text && *text)
			view->priv->convert_in_situ = TRUE;
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), text, "file://");
	} else
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), text, "file://");
}

/**
 * e_html_editor_view_set_text_plain:
 * @view: an #EHTMLEditorView
 * @text: A plain text to load into the editor
 *
 * Loads given @text into the editor, destryoing any content already present.
 */
void
e_html_editor_view_set_text_plain (EHTMLEditorView *view,
                                   const gchar *text)
{
	GDBusProxy *web_extension;
#if 0 /* FIXME WK2 */
	WebKitLoadStatus status;

	/* It can happen that the view is not ready yet (it is in the middle of
	 * another load operation) so we have to queue the current operation and
	 * redo it again when the view is ready. This was happening when loading
	 * the stuff in EMailSignatureEditor. */
	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (view));
	if (status != WEBKIT_LOAD_FINISHED) {
		html_editor_view_queue_post_reload_operation (
			view,
			(PostReloadOperationFunc) e_html_editor_view_set_text_plain,
			g_strdup (text),
			g_free);
		return;
	}
#endif
	if (view->priv->reload_in_progress) {
		html_editor_view_queue_post_reload_operation (
			view,
			(PostReloadOperationFunc) e_html_editor_view_set_text_html,
			g_strdup (text),
			g_free);
		return;
	}
	view->priv->reload_in_progress = TRUE;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"DOMConvertContent",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			text),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

/**
 * e_html_editor_view_paste_as_text:
 * @view: an #EHTMLEditorView
 *
 * Pastes current content of clipboard into the editor without formatting
 */
void
e_html_editor_view_paste_as_text (EHTMLEditorView *view)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	html_editor_view_paste_as_text (view);
}

/**
 * e_html_editor_view_paste_clipboard_quoted:
 * @view: an #EHTMLEditorView
 *
 * Pastes current content of clipboard into the editor as quoted text
 */
void
e_html_editor_view_paste_clipboard_quoted (EHTMLEditorView *view)
{
	EHTMLEditorViewClass *class;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	class = E_HTML_EDITOR_VIEW_GET_CLASS (view);
	g_return_if_fail (class->paste_clipboard_quoted != NULL);

	class->paste_clipboard_quoted (view);
}

void
e_html_editor_view_embed_styles (EHTMLEditorView *view)
{
	GDBusProxy *web_extension;
	gchar *stylesheet_uri;
	gchar *stylesheet_content;
	const gchar *stylesheet;
	gsize length;
	WebKitSettings *settings;

	g_return_if_fail (view != NULL);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));

	g_object_get (
		G_OBJECT (settings),
		"user-stylesheet-uri", &stylesheet_uri,
		NULL);

	stylesheet = strstr (stylesheet_uri, ",");
	stylesheet_content = (gchar *) g_base64_decode (stylesheet, &length);
	g_free (stylesheet_uri);

	if (length == 0) {
		g_free (stylesheet_content);
		return;
	}

	g_dbus_proxy_call (
		web_extension,
		"DOMEmbedStyleSheet",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			stylesheet_content),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_free (stylesheet_content);
}

void
e_html_editor_view_remove_embed_styles (EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function (view, "DOMRemoveEmbedStyleSheet");
}

/**
 * e_html_editor_view_update_fonts:
 * @view: an #EHTMLEditorView
 *
 * Forces the editor to reload font settings from WebKitSettings and apply
 * it on the content of the editor document.
 */
void
e_html_editor_view_update_fonts (EHTMLEditorView *view)
{
	gboolean mark_citations, use_custom_font;
	GdkColor *visited = NULL;
	gchar *font, *aa = NULL, *citation_color;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GString *stylesheet;
	GtkStyleContext *context;
	PangoFontDescription *ms, *vw;
	WebKitSettings *settings;
	WebKitUserContentManager *manager;
	WebKitUserStyleSheet *style_sheet;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	use_custom_font = g_settings_get_boolean (
		view->priv->mail_settings, "use-custom-font");

	if (use_custom_font) {
		font = g_settings_get_string (
			view->priv->mail_settings, "monospace-font");
		ms = pango_font_description_from_string (font ? font : "monospace 10");
		g_free (font);
	} else {
		font = g_settings_get_string (
			view->priv->font_settings, "monospace-font-name");
		ms = pango_font_description_from_string (font ? font : "monospace 10");
		g_free (font);
	}

	if (view->priv->html_mode) {
		if (use_custom_font) {
			font = g_settings_get_string (
				view->priv->mail_settings, "variable-width-font");
			vw = pango_font_description_from_string (font ? font : "serif 10");
			g_free (font);
		} else {
			font = g_settings_get_string (
				view->priv->font_settings, "font-name");
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

	if (view->priv->aliasing_settings != NULL)
		aa = g_settings_get_string (
			view->priv->aliasing_settings, "antialiasing");

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

	context = gtk_widget_get_style_context (GTK_WIDGET (view));
	gtk_style_context_get_style (
		context, "visited-link-color", &visited, NULL);

	if (visited == NULL) {
		visited = g_slice_new0 (GdkColor);
		visited->red = G_MAXINT16;
	}

	g_string_append_printf (
		stylesheet,
		"a:visited {\n"
		"  color: #%06x;\n"
		"}\n",
		e_color_to_value (visited));

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
		g_settings_get_int (view->priv->mail_settings, "composer-word-wrap-length"));

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

	g_string_append (
		stylesheet,
		"ul,ol "
		"{\n"
		"  -webkit-padding-start: 7ch; \n"
		"}\n");

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
		".-x-evo-list-item-align-left "
		"{\n"
		"  text-align: left; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-list-item-align-center "
		"{\n"
		"  text-align: center; \n"
		"  -webkit-padding-start: 0ch; \n"
		"  margin-left: -3ch; \n"
		"  margin-right: 1ch; \n"
		"  list-style-position: inside; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-list-item-align-right "
		"{\n"
		"  text-align: right; \n"
		"  -webkit-padding-start: 0ch; \n"
		"  margin-left: -3ch; \n"
		"  margin-right: 1ch; \n"
		"  list-style-position: inside; \n"
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

	citation_color = g_settings_get_string (
		view->priv->mail_settings, "citation-color");
	mark_citations = g_settings_get_boolean (
		view->priv->mail_settings, "mark-citations");

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

	gdk_color_free (visited);

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));
	g_object_set (
		G_OBJECT (settings),
		"default-font-size",
		e_util_normalize_font_size (
			GTK_WIDGET (view), pango_font_description_get_size (vw) / PANGO_SCALE),
		"default-font-family",
		pango_font_description_get_family (vw),
		"monospace-font-family",
		pango_font_description_get_family (ms),
		"default-monospace-font-size",
		e_util_normalize_font_size (
			GTK_WIDGET (view), pango_font_description_get_size (ms) / PANGO_SCALE),
		NULL);

	manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (view));
	webkit_user_content_manager_remove_all_style_sheets (manager);

	style_sheet = webkit_user_style_sheet_new (
		stylesheet->str,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
		WEBKIT_USER_STYLE_LEVEL_USER,
		NULL,
		NULL);

	webkit_user_content_manager_add_style_sheet (manager, style_sheet);

	webkit_user_style_sheet_unref (style_sheet);

	pango_font_description_free (ms);
	pango_font_description_free (vw);
}

/**
 * e_html_editor_view_add_inline_image_from_mime_part:
 * @composer: a composer object
 * @part: a CamelMimePart containing image data
 *
 * This adds the mime part @part to @composer as an inline image.
 **/
void
e_html_editor_view_add_inline_image_from_mime_part (EHTMLEditorView *view,
                                                    CamelMimePart *part)
{
	CamelDataWrapper *dw;
	CamelStream *stream;
	GDBusProxy *web_extension;
	GByteArray *byte_array;
	gchar *src, *base64_encoded, *mime_type, *cid_uri;
	const gchar *cid, *name;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
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
		web_extension,
		"DOMAddNewInlineImageIntoList",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			name,
			cid_uri,
			src),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_free (base64_encoded);
	g_free (mime_type);
	g_object_unref (stream);
}

void
e_html_editor_view_set_is_message_from_draft (EHTMLEditorView *view,
                                              gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->is_message_from_draft = value;
}

gboolean
e_html_editor_view_is_message_from_draft (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return get_cached_boolean_property (view, "IsMessageFromDraft");
}

void
e_html_editor_view_set_is_message_from_selection (EHTMLEditorView *view,
                                                  gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->is_message_from_selection = value;
}

gboolean
e_html_editor_view_is_message_from_edit_as_new (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return get_cached_boolean_property (view, "IsMessageFromEditAsNew");
}
void
e_html_editor_view_set_is_message_from_edit_as_new (EHTMLEditorView *view,
                                                    gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->is_message_from_edit_as_new = value;
}

void
e_html_editor_view_set_remove_initial_input_line (EHTMLEditorView *view,
                                                  gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->remove_initial_input_line = value;
}

void
e_html_editor_view_scroll_to_caret (EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function (view, "DOMScrollToCaret");
}

/************************* image_load_and_insert_async() *************************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	EHTMLEditorView *view;
	GInputStream *input_stream;
	GOutputStream *output_stream;
	GFile *file;
	GFileInfo *file_info;
	goffset total_num_bytes;
	gssize bytes_read;
	const gchar *content_type;
	const gchar *filename;
	const gchar *selector;
	gchar buffer[4096];
};

/* Forward Declaration */
static void
image_load_stream_read_cb (GInputStream *input_stream,
                           GAsyncResult *result,
                           LoadContext *load_context);

static LoadContext *
image_load_context_new (EHTMLEditorView *view)
{
	LoadContext *load_context;

	load_context = g_slice_new0 (LoadContext);
	load_context->view = view;

	return load_context;
}

static void
image_load_context_free (LoadContext *load_context)
{
	if (load_context->input_stream != NULL)
		g_object_unref (load_context->input_stream);

	if (load_context->output_stream != NULL)
		g_object_unref (load_context->output_stream);

	if (load_context->file_info != NULL)
		g_object_unref (load_context->file_info);

	if (load_context->file != NULL)
		g_object_unref (load_context->file);

	g_slice_free (LoadContext, load_context);
}

static void
replace_base64_image_src (EHTMLEditorView *view,
                          const gchar *selector,
                          const gchar *base64_content,
                          const gchar *filename,
                          const gchar *uri)
{
	GDBusProxy *web_extension;

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"DOMReplaceBase64ImageSrc",
		g_variant_new (
			"(tssss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			selector,
			base64_content,
			filename,
			uri),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
insert_base64_image (EHTMLEditorView *view,
                     const gchar *base64_content,
                     const gchar *filename,
                     const gchar *uri)
{
	GDBusProxy *web_extension;

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"EHTMLEditorSelectionInsertBase64Image",
		g_variant_new (
			"(tssss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			base64_content,
			filename,
			uri),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
image_load_finish (LoadContext *load_context)
{
	EHTMLEditorView *view;
	GMemoryOutputStream *output_stream;
	const gchar *selector;
	gchar *base64_encoded, *mime_type, *output, *uri;
	gsize size;
	gpointer data;

	output_stream = G_MEMORY_OUTPUT_STREAM (load_context->output_stream);

	view = load_context->view;

	mime_type = g_content_type_get_mime_type (load_context->content_type);

	data = g_memory_output_stream_get_data (output_stream);
	size = g_memory_output_stream_get_data_size (output_stream);
	uri = g_file_get_uri (load_context->file);

	base64_encoded = g_base64_encode ((const guchar *) data, size);
	output = g_strconcat ("data:", mime_type, ";base64,", base64_encoded, NULL);
	selector = load_context->selector;
	if (selector && *selector)
		replace_base64_image_src (
			view, selector, output, load_context->filename, uri);
	else
		insert_base64_image (view, output, load_context->filename, uri);

	g_free (base64_encoded);
	g_free (output);
	g_free (mime_type);
	g_free (uri);

	image_load_context_free (load_context);
}

static void
image_load_write_cb (GOutputStream *output_stream,
                     GAsyncResult *result,
                     LoadContext *load_context)
{
	GInputStream *input_stream;
	gssize bytes_written;
	GError *error = NULL;

	bytes_written = g_output_stream_write_finish (
		output_stream, result, &error);

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	input_stream = load_context->input_stream;

	if (bytes_written < load_context->bytes_read) {
		g_memmove (
			load_context->buffer,
			load_context->buffer + bytes_written,
			load_context->bytes_read - bytes_written);
		load_context->bytes_read -= bytes_written;

		g_output_stream_write_async (
			output_stream,
			load_context->buffer,
			load_context->bytes_read,
			G_PRIORITY_DEFAULT, NULL,
			(GAsyncReadyCallback) image_load_write_cb,
			load_context);
	} else
		g_input_stream_read_async (
			input_stream,
			load_context->buffer,
			sizeof (load_context->buffer),
			G_PRIORITY_DEFAULT, NULL,
			(GAsyncReadyCallback) image_load_stream_read_cb,
			load_context);
}

static void
image_load_stream_read_cb (GInputStream *input_stream,
                           GAsyncResult *result,
                           LoadContext *load_context)
{
	GOutputStream *output_stream;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (
		input_stream, result, &error);

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	if (bytes_read == 0) {
		image_load_finish (load_context);
		return;
	}

	output_stream = load_context->output_stream;
	load_context->bytes_read = bytes_read;

	g_output_stream_write_async (
		output_stream,
		load_context->buffer,
		load_context->bytes_read,
		G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) image_load_write_cb,
		load_context);
}

static void
image_load_file_read_cb (GFile *file,
                         GAsyncResult *result,
                         LoadContext *load_context)
{
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	GError *error = NULL;

	/* Input stream might be NULL, so don't use cast macro. */
	input_stream = g_file_read_finish (file, result, &error);
	load_context->input_stream = (GInputStream *) input_stream;

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	/* Load the contents into a GMemoryOutputStream. */
	output_stream = g_memory_output_stream_new (
		NULL, 0, g_realloc, g_free);

	load_context->output_stream = output_stream;

	g_input_stream_read_async (
		load_context->input_stream,
		load_context->buffer,
		sizeof (load_context->buffer),
		G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) image_load_stream_read_cb,
		load_context);
}

static void
image_load_query_info_cb (GFile *file,
                          GAsyncResult *result,
                          LoadContext *load_context)
{
	GFileInfo *file_info;
	GError *error = NULL;

	file_info = g_file_query_info_finish (file, result, &error);
	if (error) {
		image_load_context_free (load_context);
		return;
	}

	load_context->content_type = g_file_info_get_content_type (file_info);
	load_context->total_num_bytes = g_file_info_get_size (file_info);
	load_context->filename = g_file_info_get_name (file_info);

	g_file_read_async (
		file, G_PRIORITY_DEFAULT,
		NULL, (GAsyncReadyCallback)
		image_load_file_read_cb, load_context);
}

static void
image_load_and_insert_async (EHTMLEditorView *view,
                             const gchar *selector,
                             const gchar *uri)
{
	LoadContext *load_context;
	GFile *file;

	g_return_if_fail (uri && *uri);

	file = g_file_new_for_uri (uri);
	g_return_if_fail (file != NULL);

	load_context = image_load_context_new (view);
	load_context->file = file;
	if (selector && *selector)
		load_context->selector = g_strdup (selector);

	g_file_query_info_async (
		file, "standard::*",
		G_FILE_QUERY_INFO_NONE,G_PRIORITY_DEFAULT,
		NULL, (GAsyncReadyCallback)
		image_load_query_info_cb, load_context);
}

/**
 * e_html_editor_selection_insert_image:
 * @selection: an #EHTMLEditorSelection
 * @image_uri: an URI of the source image
 *
 * Inserts image at current cursor position using @image_uri as source. When a
 * text range is selected, it will be replaced by the image.
 */
void
e_html_editor_view_insert_image (EHTMLEditorView *view,
                                 const gchar *image_uri)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (image_uri != NULL);

	if (e_html_editor_view_get_html_mode (view)) {
		if (strstr (image_uri, ";base64,")) {
			if (g_str_has_prefix (image_uri, "data:"))
				insert_base64_image (view, image_uri, "", "");
			if (strstr (image_uri, ";data")) {
				const gchar *base64_data = strstr (image_uri, ";") + 1;
				gchar *filename;
				glong filename_length;

				filename_length =
					g_utf8_strlen (image_uri, -1) -
					g_utf8_strlen (base64_data, -1) - 1;
				filename = g_strndup (image_uri, filename_length);

				insert_base64_image (view, base64_data, filename, "");
				g_free (filename);
			}
		} else
			image_load_and_insert_async (view, NULL, image_uri);
	}
}

/**
 * e_html_editor_selection_replace_image_src:
 * @selection: an #EHTMLEditorSelection
 * @selector: CSS selector that describes the element that we want to change
 * @image_uri: an URI of the source image
 *
 * If element described by given selector is image, we will replace the src
 * attribute of it with base64 data from given @image_uri. Otherwise we will
 * set the base64 data to the background attribute of given element.
 */
void
e_html_editor_view_replace_image_src (EHTMLEditorView *view,
                                      const gchar *selector,
                                      const gchar *image_uri)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (image_uri != NULL);
	g_return_if_fail (selector && *selector);

	if (strstr (image_uri, ";base64,")) {
		if (g_str_has_prefix (image_uri, "data:"))
			replace_base64_image_src (
				view, selector, image_uri, "", "");
		if (strstr (image_uri, ";data")) {
			const gchar *base64_data = strstr (image_uri, ";") + 1;
			gchar *filename;
			glong filename_length;

			filename_length =
				g_utf8_strlen (image_uri, -1) -
				g_utf8_strlen (base64_data, -1) - 1;
			filename = g_strndup (image_uri, filename_length);

			replace_base64_image_src (
				view, selector, base64_data, filename, "");
			g_free (filename);
		}
	} else
		image_load_and_insert_async (view, selector, image_uri);
}

void
e_html_editor_view_check_magic_links (EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function (view, "DOMCheckMagicLinks");
}

void
e_html_editor_view_restore_selection (EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function (view, "DOMRestoreSelection");
}

void
e_html_editor_view_save_selection (EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function (view, "DOMSaveSelection");
}

static void
set_link_color (EHTMLEditorView *view)
{
	GdkColor *color = NULL;
	GdkRGBA rgba;
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (GTK_WIDGET (view));
	gtk_style_context_get_style (
		context, "link-color", &color, NULL);

	if (color == NULL) {
		rgba.alpha = 1;
		rgba.red = 0;
		rgba.green = 0;
		rgba.blue = 1;
	} else {
		rgba.alpha = 1;
		rgba.red = ((gdouble) color->red) / G_MAXUINT16;
		rgba.green = ((gdouble) color->green) / G_MAXUINT16;
		rgba.blue = ((gdouble) color->blue) / G_MAXUINT16;
	}

	/* FIXME WK2
	 * This set_link_color needs to be called when the document is loaded
	 * (so we will probably emit the signal from WebProcess to Evo when this
	 * happens).
	e_html_editor_view_set_link_color (view, &rgba); */

	gdk_color_free (color);
}
