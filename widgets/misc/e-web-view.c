/*
 * e-web-view.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-web-view.h"

#include <config.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include <camel/camel-internet-address.h>
#include <camel/camel-url.h>

#include "e-util/e-util.h"
#include "e-util/e-plugin-ui.h"

#define E_WEB_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEB_VIEW, EWebViewPrivate))

typedef struct _EWebViewRequest EWebViewRequest;

struct _EWebViewPrivate {
	GList *requests;
	GtkUIManager *ui_manager;
	gchar *selected_uri;
};

struct _EWebViewRequest {
	GFile *file;
	EWebView *web_view;
	GCancellable *cancellable;
	GInputStream *input_stream;
	GtkHTMLStream *output_stream;
	gchar buffer[4096];
};

enum {
	PROP_0,
	PROP_ANIMATE,
	PROP_CARET_MODE,
	PROP_SELECTED_URI
};

enum {
	POPUP_EVENT,
	STATUS_MESSAGE,
	STOP_LOADING,
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions-1'>"
"      <menuitem action='http-open'/>"
"      <menuitem action='send-message'/>"
"    </placeholder>"
"    <placeholder name='custom-actions-2'>"
"      <menuitem action='uri-copy'/>"
"      <menuitem action='mailto-copy'/>"
"    </placeholder>"
"    <placeholder name='custom-actions-3'/>"
"  </popup>"
"</ui>";

static EWebViewRequest *
web_view_request_new (EWebView *web_view,
                      const gchar *uri,
                      GtkHTMLStream *stream)
{
	EWebViewRequest *request;
	GList *list;

	request = g_slice_new (EWebViewRequest);

	/* Try to detect file paths posing as URIs. */
	if (*uri == '/')
		request->file = g_file_new_for_path (uri);
	else
		request->file = g_file_new_for_uri (uri);

	request->web_view = g_object_ref (web_view);
	request->cancellable = g_cancellable_new ();
	request->input_stream = NULL;
	request->output_stream = stream;

	list = request->web_view->priv->requests;
	list = g_list_prepend (list, request);
	request->web_view->priv->requests = list;

	return request;
}

static void
web_view_request_free (EWebViewRequest *request)
{
	GList *list;

	list = request->web_view->priv->requests;
	list = g_list_remove (list, request);
	request->web_view->priv->requests = list;

	g_object_unref (request->file);
	g_object_unref (request->web_view);
	g_object_unref (request->cancellable);

	if (request->input_stream != NULL)
		g_object_unref (request->input_stream);

	g_slice_free (EWebViewRequest, request);
}

static void
web_view_request_cancel (EWebViewRequest *request)
{
	g_cancellable_cancel (request->cancellable);
}

static gboolean
web_view_request_check_for_error (EWebViewRequest *request,
                                  GError *error)
{
	GtkHTML *html;
	GtkHTMLStream *stream;

	if (error == NULL)
		return FALSE;

	/* XXX Should we log errors that are not cancellations? */

	html = GTK_HTML (request->web_view);
	stream = request->output_stream;

	gtk_html_end (html, stream, GTK_HTML_STREAM_ERROR);
	web_view_request_free (request);
	g_error_free (error);

	return TRUE;
}

static void
web_view_request_stream_read_cb (GInputStream *input_stream,
                                 GAsyncResult *result,
                                 EWebViewRequest *request)
{
	GtkHTML *html;
	gssize bytes_read;
	GError *error = NULL;

	html = GTK_HTML (request->web_view);
	bytes_read = g_input_stream_read_finish (input_stream, result, &error);

	if (web_view_request_check_for_error (request, error))
		return;

	if (bytes_read == 0) {
		gtk_html_end (
			GTK_HTML (request->web_view),
			request->output_stream, GTK_HTML_STREAM_OK);
		web_view_request_free (request);
		return;
	}

	gtk_html_write (
		GTK_HTML (request->web_view),
		request->output_stream, request->buffer, bytes_read);

	g_input_stream_read_async (
		request->input_stream, request->buffer,
		sizeof (request->buffer), G_PRIORITY_DEFAULT,
		request->cancellable, (GAsyncReadyCallback)
		web_view_request_stream_read_cb, request);
}

static void
web_view_request_read_cb (GFile *file,
                          GAsyncResult *result,
                          EWebViewRequest *request)
{
	GFileInputStream *input_stream;
	GError *error = NULL;

	/* Input stream might be NULL, so don't use cast macro. */
	input_stream = g_file_read_finish (file, result, &error);
	request->input_stream = (GInputStream *) input_stream;

	if (web_view_request_check_for_error (request, error))
		return;

	g_input_stream_read_async (
		request->input_stream, request->buffer,
		sizeof (request->buffer), G_PRIORITY_DEFAULT,
		request->cancellable, (GAsyncReadyCallback)
		web_view_request_stream_read_cb, request);
}

static void
action_http_open_cb (GtkAction *action,
                     EWebView *web_view)
{
	const gchar *uri;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	e_show_uri (parent, uri);
}

static void
action_mailto_copy_cb (GtkAction *action,
                       EWebView *web_view)
{
	CamelURL *curl;
	CamelInternetAddress *inet_addr;
	GtkClipboard *clipboard;
	const gchar *uri;
	gchar *text;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	/* This should work because we checked it in update_actions(). */
	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	inet_addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
	text = camel_address_encode (CAMEL_ADDRESS (inet_addr));
	if (text == NULL || *text == '\0')
		text = g_strdup (uri + strlen ("mailto:"));

	camel_object_unref (inet_addr);
	camel_url_free (curl);

	gtk_clipboard_set_text (clipboard, text, -1);
	gtk_clipboard_store (clipboard);

	g_free (text);
}

static void
action_send_message_cb (GtkAction *action,
                        EWebView *web_view)
{
	const gchar *uri;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	e_show_uri (parent, uri);
}

static void
action_uri_copy_cb (GtkAction *action,
                    EWebView *web_view)
{
	GtkClipboard *clipboard;
	const gchar *uri;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	gtk_clipboard_set_text (clipboard, uri, -1);
	gtk_clipboard_store (clipboard);
}

static GtkActionEntry uri_entries[] = {

	{ "uri-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy Link Location"),
	  NULL,
	  N_("Copy the link to the clipboard"),
	  G_CALLBACK (action_uri_copy_cb) }
};

static GtkActionEntry http_entries[] = {

	{ "http-open",
	  "emblem-web",
	  N_("_Open Link in Browser"),
	  NULL,
	  N_("Open the link in a web browser"),
	  G_CALLBACK (action_http_open_cb) }
};

static GtkActionEntry mailto_entries[] = {

	{ "mailto-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy Email Address"),
	  NULL,
	  N_("Copy the email address to the clipboard"),
	  G_CALLBACK (action_mailto_copy_cb) },

	{ "send-message",
	  "mail-message-new",
	  N_("_Send New Message To..."),
	  NULL,
	  N_("Send a mail message to this address"),
	  G_CALLBACK (action_send_message_cb) }
};

static gboolean
web_view_button_press_event_cb (EWebView *web_view,
                                GdkEventButton *event,
                                GtkHTML *frame)
{
	gboolean event_handled = FALSE;
	gchar *uri;

	if (event != NULL && event->button != 3)
		return FALSE;

	uri = e_web_view_extract_uri (web_view, event, frame);

	if (uri == NULL || g_str_has_prefix (uri, "##")) {
		g_free (uri);
		return FALSE;
	}

	g_signal_emit (
		web_view, signals[POPUP_EVENT], 0,
		event, uri, &event_handled);

	g_free (uri);

	return event_handled;
}

static void
web_view_menu_item_select_cb (EWebView *web_view,
                              GtkWidget *widget)
{
	GtkAction *action;
	const gchar *tooltip;

	action = gtk_widget_get_action (widget);
	tooltip = gtk_action_get_tooltip (action);

	if (tooltip == NULL)
		return;

	e_web_view_status_message (web_view, tooltip);
}

static void
web_view_menu_item_deselect_cb (EWebView *web_view)
{
	e_web_view_status_message (web_view, NULL);
}

static void
web_view_connect_proxy_cb (EWebView *web_view,
                           GtkAction *action,
                           GtkWidget *proxy)
{
	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	g_signal_connect_swapped (
		proxy, "select",
		G_CALLBACK (web_view_menu_item_select_cb), web_view);

	g_signal_connect_swapped (
		proxy, "deselect",
		G_CALLBACK (web_view_menu_item_deselect_cb), web_view);
}

static void
web_view_set_property (GObject *object,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ANIMATE:
			e_web_view_set_animate (
				E_WEB_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_CARET_MODE:
			e_web_view_set_caret_mode (
				E_WEB_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SELECTED_URI:
			e_web_view_set_selected_uri (
				E_WEB_VIEW (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
web_view_get_property (GObject *object,
                       guint property_id,
                       GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ANIMATE:
			g_value_set_boolean (
				value, e_web_view_get_animate (
				E_WEB_VIEW (object)));
			return;

		case PROP_CARET_MODE:
			g_value_set_boolean (
				value, e_web_view_get_caret_mode (
				E_WEB_VIEW (object)));
			return;

		case PROP_SELECTED_URI:
			g_value_set_string (
				value, e_web_view_get_selected_uri (
				E_WEB_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
web_view_dispose (GObject *object)
{
	EWebViewPrivate *priv;

	priv = E_WEB_VIEW_GET_PRIVATE (object);

	if (priv->ui_manager != NULL) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
web_view_finalize (GObject *object)
{
	EWebViewPrivate *priv;

	priv = E_WEB_VIEW_GET_PRIVATE (object);

	/* All URI requests should be complete or cancelled by now. */
	if (priv->requests != NULL)
		g_warning ("Finalizing EWebView with active URI requests");

	g_free (priv->selected_uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
web_view_button_press_event (GtkWidget *widget,
                             GdkEventButton *event)
{
	GtkWidgetClass *widget_class;
	EWebView *web_view;

	web_view = E_WEB_VIEW (widget);

	if (web_view_button_press_event_cb (web_view, event, NULL))
		return TRUE;

	/* Chain up to parent's button_press_event() method. */
	widget_class = GTK_WIDGET_CLASS (parent_class);
	return widget_class->button_press_event (widget, event);
}

static gboolean
web_view_scroll_event (GtkWidget *widget,
                       GdkEventScroll *event)
{
	if (event->state & GDK_CONTROL_MASK) {
		switch (event->direction) {
			case GDK_SCROLL_UP:
				gtk_html_zoom_in (GTK_HTML (widget));
				return TRUE;
			case GDK_SCROLL_DOWN:
				gtk_html_zoom_out (GTK_HTML (widget));
				return TRUE;
			default:
				break;
		}
	}

	return FALSE;
}

static void
web_view_url_requested (GtkHTML *html,
                        const gchar *uri,
                        GtkHTMLStream *stream)
{
	EWebViewRequest *request;

	request = web_view_request_new (E_WEB_VIEW (html), uri, stream);

	g_file_read_async (
		request->file, G_PRIORITY_DEFAULT,
		request->cancellable, (GAsyncReadyCallback)
		web_view_request_read_cb, request);
}

static void
web_view_link_clicked (GtkHTML *html,
                       const gchar *uri)
{
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (html));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	e_show_uri (parent, uri);
}

static void
web_view_on_url (GtkHTML *html,
                 const gchar *uri)
{
	EWebView *web_view;
	CamelInternetAddress *address;
	CamelURL *curl;
	const gchar *format = NULL;
	gchar *message = NULL;
	gchar *who;

	web_view = E_WEB_VIEW (html);

	if (uri == NULL || *uri == '\0')
		goto exit;

	if (g_str_has_prefix (uri, "mailto:"))
		format = _("Click to mail %s");
	else if (g_str_has_prefix (uri, "callto:"))
		format = _("Click to call %s");
	else if (g_str_has_prefix (uri, "h323:"))
		format = _("Click to call %s");
	else if (g_str_has_prefix (uri, "sip:"))
		format = _("Click to call %s");
	else if (g_str_has_prefix (uri, "##"))
		message = g_strdup (_("Click to hide/unhide addresses"));
	else
		message = g_strdup_printf (_("Click to open %s"), uri);

	if (format == NULL)
		goto exit;

	/* XXX Use something other than Camel here.  Surely
	 *     there's other APIs around that can do this. */
	curl = camel_url_new (uri, NULL);
	address = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (address), curl->path);
	who = camel_address_format (CAMEL_ADDRESS (address));
	camel_object_unref (address);
	camel_url_free (curl);

	if (who == NULL)
		who = g_strdup (strchr (uri, ':') + 1);

	message = g_strdup_printf (format, who);

	g_free (who);

exit:
	e_web_view_status_message (web_view, message);

	g_free (message);
}

static void
web_view_iframe_created (GtkHTML *html,
                         GtkHTML *iframe)
{
	g_signal_connect_swapped (
		iframe, "button-press-event",
		G_CALLBACK (web_view_button_press_event_cb), html);
}

static gchar *
web_view_extract_uri (EWebView *web_view,
                      GdkEventButton *event,
                      GtkHTML *html)
{
	gchar *uri;

	if (event != NULL)
		uri = gtk_html_get_url_at (html, event->x, event->y);
	else
		uri = gtk_html_get_cursor_url (html);

	return uri;
}

static gboolean
web_view_popup_event (EWebView *web_view,
                      GdkEventButton *event,
                      const gchar *uri)
{
	e_web_view_set_selected_uri (web_view, uri);
	e_web_view_show_popup_menu (web_view, event, NULL, NULL);

	return TRUE;
}

static void
web_view_stop_loading (EWebView *web_view)
{
	g_list_foreach (
		web_view->priv->requests, (GFunc)
		web_view_request_cancel, NULL);

	gtk_html_stop (GTK_HTML (web_view));
}

static void
web_view_update_actions (EWebView *web_view)
{
	CamelURL *curl;
	GtkActionGroup *action_group;
	gboolean scheme_is_http;
	gboolean scheme_is_mailto;
	gboolean uri_is_valid;
	gboolean visible;
	const gchar *uri;

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	/* Parse the URI early so we know if the actions will work. */
	curl = camel_url_new (uri, NULL);
	uri_is_valid = (curl != NULL);
	camel_url_free (curl);

	scheme_is_http =
		(g_ascii_strncasecmp (uri, "http:", 5) == 0) ||
		(g_ascii_strncasecmp (uri, "https:", 6) == 0);

	scheme_is_mailto =
		(g_ascii_strncasecmp (uri, "mailto:", 7) == 0);

	/* Allow copying the URI even if it's malformed. */
	visible = !scheme_is_mailto;
	action_group = e_web_view_get_action_group (web_view, "uri");
	gtk_action_group_set_visible (action_group, visible);

	visible = uri_is_valid && scheme_is_http;
	action_group = e_web_view_get_action_group (web_view, "http");
	gtk_action_group_set_visible (action_group, visible);

	visible = uri_is_valid && scheme_is_mailto;
	action_group = e_web_view_get_action_group (web_view, "mailto");
	gtk_action_group_set_visible (action_group, visible);
}

static void
web_view_class_init (EWebViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkHTMLClass *html_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EWebViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = web_view_set_property;
	object_class->get_property = web_view_get_property;
	object_class->dispose = web_view_dispose;
	object_class->finalize = web_view_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = web_view_button_press_event;
	widget_class->scroll_event = web_view_scroll_event;

	html_class = GTK_HTML_CLASS (class);
	html_class->url_requested = web_view_url_requested;
	html_class->link_clicked = web_view_link_clicked;
	html_class->on_url = web_view_on_url;
	html_class->iframe_created = web_view_iframe_created;

	class->extract_uri = web_view_extract_uri;
	class->popup_event = web_view_popup_event;
	class->stop_loading = web_view_stop_loading;
	class->update_actions = web_view_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_ANIMATE,
		g_param_spec_boolean (
			"animate",
			"Animate Images",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CARET_MODE,
		g_param_spec_boolean (
			"caret-mode",
			"Caret Mode",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTED_URI,
		g_param_spec_string (
			"selected-uri",
			"Selected URI",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, popup_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__BOXED_STRING,
		G_TYPE_BOOLEAN, 2,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_STRING);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, status_message),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	signals[STOP_LOADING] = g_signal_new (
		"stop-loading",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, stop_loading),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
web_view_init (EWebView *web_view)
{
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	const gchar *domain = GETTEXT_PACKAGE;
	const gchar *id;
	GError *error = NULL;

	web_view->priv = E_WEB_VIEW_GET_PRIVATE (web_view);

	ui_manager = gtk_ui_manager_new ();
	web_view->priv->ui_manager = ui_manager;

	g_signal_connect_swapped (
		ui_manager, "connect-proxy",
		G_CALLBACK (web_view_connect_proxy_cb), web_view);

	action_group = gtk_action_group_new ("uri");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	gtk_action_group_add_actions (
		action_group, uri_entries,
		G_N_ELEMENTS (uri_entries), web_view);

	action_group = gtk_action_group_new ("http");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	gtk_action_group_add_actions (
		action_group, http_entries,
		G_N_ELEMENTS (http_entries), web_view);

	action_group = gtk_action_group_new ("mailto");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	gtk_action_group_add_actions (
		action_group, mailto_entries,
		G_N_ELEMENTS (mailto_entries), web_view);

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);

	id = "org.gnome.evolution.webview";
	e_plugin_ui_register_manager (ui_manager, id, web_view);
	e_plugin_ui_enable_manager (ui_manager, id);
}

GType
e_web_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EWebViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) web_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EWebView),
			0,     /* n_preallocs */
			(GInstanceInitFunc) web_view_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_HTML, "EWebView", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_web_view_new (void)
{
	return g_object_new (E_TYPE_WEB_VIEW, NULL);
}

void
e_web_view_clear (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	gtk_html_load_empty (GTK_HTML (web_view));
}

void
e_web_view_load_string (EWebView *web_view,
                        const gchar *string)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (string != NULL && *string != '\0')
		gtk_html_load_from_string (GTK_HTML (web_view), string, -1);
	else
		e_web_view_clear (web_view);
}

gboolean
e_web_view_get_animate (EWebView *web_view)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_web_view_set_animate(). */

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return gtk_html_get_animate (GTK_HTML (web_view));
}

void
e_web_view_set_animate (EWebView *web_view,
                        gboolean animate)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_animate()
	 *     so we can get a "notify::animate" signal. */

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	gtk_html_set_animate (GTK_HTML (web_view), animate);

	g_object_notify (G_OBJECT (web_view), "animate");
}

gboolean
e_web_view_get_caret_mode (EWebView *web_view)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_web_view_set_caret_mode(). */

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return gtk_html_get_caret_mode (GTK_HTML (web_view));
}

void
e_web_view_set_caret_mode (EWebView *web_view,
                           gboolean caret_mode)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_caret_mode()
	 *     so we can get a "notify::caret-mode" signal. */

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	gtk_html_set_caret_mode (GTK_HTML (web_view), caret_mode);

	g_object_notify (G_OBJECT (web_view), "caret-mode");
}

const gchar *
e_web_view_get_selected_uri (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->selected_uri;
}

void
e_web_view_set_selected_uri (EWebView *web_view,
                             const gchar *selected_uri)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	g_free (web_view->priv->selected_uri);
	web_view->priv->selected_uri = g_strdup (selected_uri);

	g_object_notify (G_OBJECT (web_view), "selected-uri");
}

GtkAction *
e_web_view_get_action (EWebView *web_view,
                       const gchar *action_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_web_view_get_ui_manager (web_view);

	return e_lookup_action (ui_manager, action_name);
}

GtkActionGroup *
e_web_view_get_action_group (EWebView *web_view,
                             const gchar *group_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_web_view_get_ui_manager (web_view);

	return e_lookup_action_group (ui_manager, group_name);
}

gchar *
e_web_view_extract_uri (EWebView *web_view,
                        GdkEventButton *event,
                        GtkHTML *frame)
{
	EWebViewClass *class;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	if (frame == NULL)
		frame = GTK_HTML (web_view);

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_val_if_fail (class->extract_uri != NULL, NULL);

	return class->extract_uri (web_view, event, frame);
}

gboolean
e_web_view_scroll_forward (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return gtk_html_command (GTK_HTML (web_view), "scroll-forward");
}

gboolean
e_web_view_scroll_backward (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return gtk_html_command (GTK_HTML (web_view), "scroll-backward");
}

GtkUIManager *
e_web_view_get_ui_manager (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->ui_manager;
}

GtkWidget *
e_web_view_get_popup_menu (EWebView *web_view)
{
	GtkUIManager *ui_manager;
	GtkWidget *menu;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	ui_manager = e_web_view_get_ui_manager (web_view);
	menu = gtk_ui_manager_get_widget (ui_manager, "/context");
	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	return menu;
}

void
e_web_view_show_popup_menu (EWebView *web_view,
                            GdkEventButton *event,
                            GtkMenuPositionFunc func,
                            gpointer user_data)
{
	GtkWidget *menu;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_update_actions (web_view);

	menu = e_web_view_get_popup_menu (web_view);

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, func,
			user_data, event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, func,
			user_data, 0, gtk_get_current_event_time ());
}

void
e_web_view_status_message (EWebView *web_view,
                           const gchar *status_message)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	g_signal_emit (web_view, signals[STATUS_MESSAGE], 0, status_message);
}

void
e_web_view_stop_loading (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	g_signal_emit (web_view, signals[STOP_LOADING], 0);
}

void
e_web_view_update_actions (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	g_signal_emit (web_view, signals[UPDATE_ACTIONS], 0);
}
