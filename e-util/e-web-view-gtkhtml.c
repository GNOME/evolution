/*
 * e-web-view-gtkhtml.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-web-view-gtkhtml.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <libebackend/libebackend.h>

#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-misc-utils.h"
#include "e-plugin-ui.h"
#include "e-popup-action.h"
#include "e-selectable.h"

#define E_WEB_VIEW_GTKHTML_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEB_VIEW_GTKHTML, EWebViewGtkHTMLPrivate))

typedef struct _EWebViewGtkHTMLRequest EWebViewGtkHTMLRequest;

struct _EWebViewGtkHTMLPrivate {
	GList *requests;
	GtkUIManager *ui_manager;
	gchar *selected_uri;
	GdkPixbufAnimation *cursor_image;

	GtkAction *open_proxy;
	GtkAction *print_proxy;
	GtkAction *save_as_proxy;

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;

	/* Lockdown Options */
	guint disable_printing : 1;
	guint disable_save_to_disk : 1;
};

struct _EWebViewGtkHTMLRequest {
	GFile *file;
	EWebViewGtkHTML *web_view;
	GCancellable *cancellable;
	GInputStream *input_stream;
	GtkHTMLStream *output_stream;
	gchar buffer[4096];
};

enum {
	PROP_0,
	PROP_ANIMATE,
	PROP_CARET_MODE,
	PROP_COPY_TARGET_LIST,
	PROP_DISABLE_PRINTING,
	PROP_DISABLE_SAVE_TO_DISK,
	PROP_EDITABLE,
	PROP_INLINE_SPELLING,
	PROP_MAGIC_LINKS,
	PROP_MAGIC_SMILEYS,
	PROP_OPEN_PROXY,
	PROP_PASTE_TARGET_LIST,
	PROP_PRINT_PROXY,
	PROP_SAVE_AS_PROXY,
	PROP_SELECTED_URI,
	PROP_CURSOR_IMAGE
};

enum {
	COPY_CLIPBOARD,
	CUT_CLIPBOARD,
	PASTE_CLIPBOARD,
	POPUP_EVENT,
	STATUS_MESSAGE,
	STOP_LOADING,
	UPDATE_ACTIONS,
	PROCESS_MAILTO,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <menuitem action='copy-clipboard'/>"
"    <separator/>"
"    <placeholder name='custom-actions-1'>"
"      <menuitem action='open'/>"
"      <menuitem action='save-as'/>"
"      <menuitem action='http-open'/>"
"      <menuitem action='send-message'/>"
"      <menuitem action='print'/>"
"    </placeholder>"
"    <placeholder name='custom-actions-2'>"
"      <menuitem action='uri-copy'/>"
"      <menuitem action='mailto-copy'/>"
"      <menuitem action='image-copy'/>"
"    </placeholder>"
"    <placeholder name='custom-actions-3'/>"
"    <separator/>"
"    <menuitem action='select-all'/>"
"  </popup>"
"</ui>";

/* Forward Declarations */
static void e_web_view_gtkhtml_alert_sink_init (EAlertSinkInterface *iface);
static void e_web_view_gtkhtml_selectable_init (ESelectableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EWebViewGtkHTML,
	e_web_view_gtkhtml,
	GTK_TYPE_HTML,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK,
		e_web_view_gtkhtml_alert_sink_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_SELECTABLE,
		e_web_view_gtkhtml_selectable_init))

static EWebViewGtkHTMLRequest *
web_view_gtkhtml_request_new (EWebViewGtkHTML *web_view,
                              const gchar *uri,
                              GtkHTMLStream *stream)
{
	EWebViewGtkHTMLRequest *request;
	GList *list;

	request = g_slice_new (EWebViewGtkHTMLRequest);

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
web_view_gtkhtml_request_free (EWebViewGtkHTMLRequest *request)
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

	g_slice_free (EWebViewGtkHTMLRequest, request);
}

static void
web_view_gtkhtml_request_cancel (EWebViewGtkHTMLRequest *request)
{
	g_cancellable_cancel (request->cancellable);
}

static gboolean
web_view_gtkhtml_request_check_for_error (EWebViewGtkHTMLRequest *request,
                                          GError *error)
{
	GtkHTML *html;
	GtkHTMLStream *stream;

	if (error == NULL)
		return FALSE;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED)) {
		/* use this error, but do not close the stream */
		g_error_free (error);
		return TRUE;
	}

	/* XXX Should we log errors that are not cancellations? */

	html = GTK_HTML (request->web_view);
	stream = request->output_stream;

	gtk_html_end (html, stream, GTK_HTML_STREAM_ERROR);
	web_view_gtkhtml_request_free (request);
	g_error_free (error);

	return TRUE;
}

static void
web_view_gtkhtml_request_stream_read_cb (GInputStream *input_stream,
                                         GAsyncResult *result,
                                         EWebViewGtkHTMLRequest *request)
{
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (input_stream, result, &error);

	if (web_view_gtkhtml_request_check_for_error (request, error))
		return;

	if (bytes_read == 0) {
		gtk_html_end (
			GTK_HTML (request->web_view),
			request->output_stream, GTK_HTML_STREAM_OK);
		web_view_gtkhtml_request_free (request);
		return;
	}

	gtk_html_write (
		GTK_HTML (request->web_view),
		request->output_stream, request->buffer, bytes_read);

	g_input_stream_read_async (
		request->input_stream, request->buffer,
		sizeof (request->buffer), G_PRIORITY_DEFAULT,
		request->cancellable, (GAsyncReadyCallback)
		web_view_gtkhtml_request_stream_read_cb, request);
}

static void
web_view_gtkhtml_request_read_cb (GFile *file,
                                  GAsyncResult *result,
                                  EWebViewGtkHTMLRequest *request)
{
	GFileInputStream *input_stream;
	GError *error = NULL;

	/* Input stream might be NULL, so don't use cast macro. */
	input_stream = g_file_read_finish (file, result, &error);
	request->input_stream = (GInputStream *) input_stream;

	if (web_view_gtkhtml_request_check_for_error (request, error))
		return;

	g_input_stream_read_async (
		request->input_stream, request->buffer,
		sizeof (request->buffer), G_PRIORITY_DEFAULT,
		request->cancellable, (GAsyncReadyCallback)
		web_view_gtkhtml_request_stream_read_cb, request);
}

static void
action_copy_clipboard_cb (GtkAction *action,
                          EWebViewGtkHTML *web_view)
{
	e_web_view_gtkhtml_copy_clipboard (web_view);
}

static void
action_http_open_cb (GtkAction *action,
                     EWebViewGtkHTML *web_view)
{
	const gchar *uri;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	uri = e_web_view_gtkhtml_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	e_show_uri (parent, uri);
}

static void
action_mailto_copy_cb (GtkAction *action,
                       EWebViewGtkHTML *web_view)
{
	CamelURL *curl;
	CamelInternetAddress *inet_addr;
	GtkClipboard *clipboard;
	const gchar *uri;
	gchar *text;

	uri = e_web_view_gtkhtml_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	/* This should work because we checked it in update_actions(). */
	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	inet_addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
	text = camel_address_format (CAMEL_ADDRESS (inet_addr));
	if (text == NULL || *text == '\0')
		text = g_strdup (uri + strlen ("mailto:"));

	g_object_unref (inet_addr);
	camel_url_free (curl);

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clipboard, text, -1);
	gtk_clipboard_store (clipboard);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, text, -1);
	gtk_clipboard_store (clipboard);

	g_free (text);
}

static void
action_select_all_cb (GtkAction *action,
                      EWebViewGtkHTML *web_view)
{
	e_web_view_gtkhtml_select_all (web_view);
}

static void
action_send_message_cb (GtkAction *action,
                        EWebViewGtkHTML *web_view)
{
	const gchar *uri;
	gpointer parent;
	gboolean handled;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	uri = e_web_view_gtkhtml_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	handled = FALSE;
	g_signal_emit (web_view, signals[PROCESS_MAILTO], 0, uri, &handled);

	if (!handled)
		e_show_uri (parent, uri);
}

static void
action_uri_copy_cb (GtkAction *action,
                    EWebViewGtkHTML *web_view)
{
	GtkClipboard *clipboard;
	const gchar *uri;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	uri = e_web_view_gtkhtml_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	gtk_clipboard_set_text (clipboard, uri, -1);
	gtk_clipboard_store (clipboard);
}

static void
action_image_copy_cb (GtkAction *action,
                      EWebViewGtkHTML *web_view)
{
	GtkClipboard *clipboard;
	GdkPixbufAnimation *animation;
	GdkPixbuf *pixbuf;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	animation = e_web_view_gtkhtml_get_cursor_image (web_view);
	g_return_if_fail (animation != NULL);

	pixbuf = gdk_pixbuf_animation_get_static_image (animation);
	if (!pixbuf)
		return;

	gtk_clipboard_set_image (clipboard, pixbuf);
	gtk_clipboard_store (clipboard);
}

static GtkActionEntry uri_entries[] = {

	{ "uri-copy",
	  "edit-copy",
	  N_("_Copy Link Location"),
	  "<Control>c",
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
	  "edit-copy",
	  N_("_Copy Email Address"),
	  "<Control>c",
	  N_("Copy the email address to the clipboard"),
	  G_CALLBACK (action_mailto_copy_cb) },

	{ "send-message",
	  "mail-message-new",
	  N_("_Send New Message To..."),
	  NULL,
	  N_("Send a mail message to this address"),
	  G_CALLBACK (action_send_message_cb) }
};

static GtkActionEntry image_entries[] = {

	{ "image-copy",
	  "edit-copy",
	  N_("_Copy Image"),
	  "<Control>c",
	  N_("Copy the image to the clipboard"),
	  G_CALLBACK (action_image_copy_cb) }
};

static GtkActionEntry selection_entries[] = {

	{ "copy-clipboard",
	  "edit-copy",
	  N_("_Copy"),
	  "<Control>c",
	  N_("Copy the selection"),
	  G_CALLBACK (action_copy_clipboard_cb) },
};

static GtkActionEntry standard_entries[] = {

	{ "select-all",
	  "edit-select-all",
	  N_("Select _All"),
	  NULL,
	  N_("Select all text and images"),
	  G_CALLBACK (action_select_all_cb) }
};

static gboolean
web_view_gtkhtml_button_press_event_cb (EWebViewGtkHTML *web_view,
                                        GdkEventButton *event,
                                        GtkHTML *frame)
{
	gboolean event_handled = FALSE;
	gchar *uri = NULL;

	if (event) {
		GdkPixbufAnimation *anim;

		if (frame == NULL)
			frame = GTK_HTML (web_view);

		anim = gtk_html_get_image_at (frame, event->x, event->y);
		e_web_view_gtkhtml_set_cursor_image (web_view, anim);
		if (anim != NULL)
			g_object_unref (anim);
	}

	if (event != NULL && event->button != 3)
		return FALSE;

	/* Only extract a URI if no selection is active.  Selected text
	 * implies the user is more likely to want to copy the selection
	 * to the clipboard than open a link within the selection. */
	if (!e_web_view_gtkhtml_is_selection_active (web_view))
		uri = e_web_view_gtkhtml_extract_uri (web_view, event, frame);

	if (uri != NULL && g_str_has_prefix (uri, "##")) {
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
web_view_gtkhtml_menu_item_select_cb (EWebViewGtkHTML *web_view,
                                      GtkWidget *widget)
{
	GtkAction *action;
	GtkActivatable *activatable;
	const gchar *tooltip;

	activatable = GTK_ACTIVATABLE (widget);
	action = gtk_activatable_get_related_action (activatable);
	tooltip = gtk_action_get_tooltip (action);

	if (tooltip == NULL)
		return;

	e_web_view_gtkhtml_status_message (web_view, tooltip);
}

static void
web_view_gtkhtml_menu_item_deselect_cb (EWebViewGtkHTML *web_view)
{
	e_web_view_gtkhtml_status_message (web_view, NULL);
}

static void
web_view_gtkhtml_connect_proxy_cb (EWebViewGtkHTML *web_view,
                                   GtkAction *action,
                                   GtkWidget *proxy)
{
	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	g_signal_connect_swapped (
		proxy, "select",
		G_CALLBACK (web_view_gtkhtml_menu_item_select_cb), web_view);

	g_signal_connect_swapped (
		proxy, "deselect",
		G_CALLBACK (web_view_gtkhtml_menu_item_deselect_cb), web_view);
}

static void
web_view_gtkhtml_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ANIMATE:
			e_web_view_gtkhtml_set_animate (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_CARET_MODE:
			e_web_view_gtkhtml_set_caret_mode (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_DISABLE_PRINTING:
			e_web_view_gtkhtml_set_disable_printing (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_DISABLE_SAVE_TO_DISK:
			e_web_view_gtkhtml_set_disable_save_to_disk (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_EDITABLE:
			e_web_view_gtkhtml_set_editable (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_INLINE_SPELLING:
			e_web_view_gtkhtml_set_inline_spelling (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_LINKS:
			e_web_view_gtkhtml_set_magic_links (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_SMILEYS:
			e_web_view_gtkhtml_set_magic_smileys (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_OPEN_PROXY:
			e_web_view_gtkhtml_set_open_proxy (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_object (value));
			return;

		case PROP_PRINT_PROXY:
			e_web_view_gtkhtml_set_print_proxy (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_object (value));
			return;

		case PROP_SAVE_AS_PROXY:
			e_web_view_gtkhtml_set_save_as_proxy (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_object (value));
			return;

		case PROP_SELECTED_URI:
			e_web_view_gtkhtml_set_selected_uri (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_string (value));
			return;
		case PROP_CURSOR_IMAGE:
			e_web_view_gtkhtml_set_cursor_image (
				E_WEB_VIEW_GTKHTML (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
web_view_gtkhtml_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ANIMATE:
			g_value_set_boolean (
				value, e_web_view_gtkhtml_get_animate (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_CARET_MODE:
			g_value_set_boolean (
				value, e_web_view_gtkhtml_get_caret_mode (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (
				value, e_web_view_gtkhtml_get_copy_target_list (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_DISABLE_PRINTING:
			g_value_set_boolean (
				value, e_web_view_gtkhtml_get_disable_printing (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_DISABLE_SAVE_TO_DISK:
			g_value_set_boolean (
				value, e_web_view_gtkhtml_get_disable_save_to_disk (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value, e_web_view_gtkhtml_get_editable (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_INLINE_SPELLING:
			g_value_set_boolean (
				value, e_web_view_gtkhtml_get_inline_spelling (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_MAGIC_LINKS:
			g_value_set_boolean (
				value, e_web_view_gtkhtml_get_magic_links (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_MAGIC_SMILEYS:
			g_value_set_boolean (
				value, e_web_view_gtkhtml_get_magic_smileys (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_OPEN_PROXY:
			g_value_set_object (
				value, e_web_view_gtkhtml_get_open_proxy (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (
				value, e_web_view_gtkhtml_get_paste_target_list (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_PRINT_PROXY:
			g_value_set_object (
				value, e_web_view_gtkhtml_get_print_proxy (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_SAVE_AS_PROXY:
			g_value_set_object (
				value, e_web_view_gtkhtml_get_save_as_proxy (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_SELECTED_URI:
			g_value_set_string (
				value, e_web_view_gtkhtml_get_selected_uri (
				E_WEB_VIEW_GTKHTML (object)));
			return;

		case PROP_CURSOR_IMAGE:
			g_value_set_object (
				value, e_web_view_gtkhtml_get_cursor_image (
				E_WEB_VIEW_GTKHTML (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
web_view_gtkhtml_dispose (GObject *object)
{
	EWebViewGtkHTMLPrivate *priv;

	priv = E_WEB_VIEW_GTKHTML_GET_PRIVATE (object);

	if (priv->ui_manager != NULL) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->open_proxy != NULL) {
		g_object_unref (priv->open_proxy);
		priv->open_proxy = NULL;
	}

	if (priv->print_proxy != NULL) {
		g_object_unref (priv->print_proxy);
		priv->print_proxy = NULL;
	}

	if (priv->save_as_proxy != NULL) {
		g_object_unref (priv->save_as_proxy);
		priv->save_as_proxy = NULL;
	}

	if (priv->copy_target_list != NULL) {
		gtk_target_list_unref (priv->copy_target_list);
		priv->copy_target_list = NULL;
	}

	if (priv->paste_target_list != NULL) {
		gtk_target_list_unref (priv->paste_target_list);
		priv->paste_target_list = NULL;
	}

	if (priv->cursor_image != NULL) {
		g_object_unref (priv->cursor_image);
		priv->cursor_image = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_web_view_gtkhtml_parent_class)->dispose (object);
}

static void
web_view_gtkhtml_finalize (GObject *object)
{
	EWebViewGtkHTMLPrivate *priv;

	priv = E_WEB_VIEW_GTKHTML_GET_PRIVATE (object);

	/* All URI requests should be complete or cancelled by now. */
	if (priv->requests != NULL)
		g_warning ("Finalizing EWebViewGtkHTML with active URI requests");

	g_free (priv->selected_uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_web_view_gtkhtml_parent_class)->finalize (object);
}

static void
web_view_gtkhtml_constructed (GObject *object)
{
#ifndef G_OS_WIN32
	GSettings *settings;

	settings = g_settings_new ("org.gnome.desktop.lockdown");

	g_settings_bind (
		settings, "disable-printing",
		object, "disable-printing",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "disable-save-to-disk",
		object, "disable-save-to-disk",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);
#endif

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_web_view_gtkhtml_parent_class)->constructed (object);
}

static gboolean
web_view_gtkhtml_button_press_event (GtkWidget *widget,
                                     GdkEventButton *event)
{
	GtkWidgetClass *widget_class;
	EWebViewGtkHTML *web_view;

	web_view = E_WEB_VIEW_GTKHTML (widget);

	if (web_view_gtkhtml_button_press_event_cb (web_view, event, NULL))
		return TRUE;

	/* Chain up to parent's button_press_event() method. */
	widget_class = GTK_WIDGET_CLASS (e_web_view_gtkhtml_parent_class);
	return widget_class->button_press_event (widget, event);
}

static gboolean
web_view_gtkhtml_scroll_event (GtkWidget *widget,
                               GdkEventScroll *event)
{
	if (event->state & GDK_CONTROL_MASK) {
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
			} else {
				return FALSE;
			}
		}

		switch (direction) {
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
web_view_gtkhtml_url_requested (GtkHTML *html,
                                const gchar *uri,
                                GtkHTMLStream *stream)
{
	EWebViewGtkHTMLRequest *request;

	request = web_view_gtkhtml_request_new (E_WEB_VIEW_GTKHTML (html), uri, stream);

	g_file_read_async (
		request->file, G_PRIORITY_DEFAULT,
		request->cancellable, (GAsyncReadyCallback)
		web_view_gtkhtml_request_read_cb, request);
}

static void
web_view_gtkhtml_gtkhtml_link_clicked (GtkHTML *html,
                                       const gchar *uri)
{
	EWebViewGtkHTMLClass *class;
	EWebViewGtkHTML *web_view;

	web_view = E_WEB_VIEW_GTKHTML (html);

	class = E_WEB_VIEW_GTKHTML_GET_CLASS (web_view);
	g_return_if_fail (class->link_clicked != NULL);

	class->link_clicked (web_view, uri);
}

static void
web_view_gtkhtml_on_url (GtkHTML *html,
                         const gchar *uri)
{
	EWebViewGtkHTMLClass *class;
	EWebViewGtkHTML *web_view;

	web_view = E_WEB_VIEW_GTKHTML (html);

	class = E_WEB_VIEW_GTKHTML_GET_CLASS (web_view);
	g_return_if_fail (class->hovering_over_link != NULL);

	/* XXX WebKit would supply a title here. */
	class->hovering_over_link (web_view, NULL, uri);
}

static void
web_view_gtkhtml_iframe_created (GtkHTML *html,
                                 GtkHTML *iframe)
{
	g_signal_connect_swapped (
		iframe, "button-press-event",
		G_CALLBACK (web_view_gtkhtml_button_press_event_cb), html);
}

static gchar *
web_view_gtkhtml_extract_uri (EWebViewGtkHTML *web_view,
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

static void
web_view_gtkhtml_hovering_over_link (EWebViewGtkHTML *web_view,
                                     const gchar *title,
                                     const gchar *uri)
{
	CamelInternetAddress *address;
	CamelURL *curl;
	const gchar *format = NULL;
	gchar *message = NULL;
	gchar *who;

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
	g_object_unref (address);
	camel_url_free (curl);

	if (who == NULL)
		who = g_strdup (strchr (uri, ':') + 1);

	message = g_strdup_printf (format, who);

	g_free (who);

exit:
	e_web_view_gtkhtml_status_message (web_view, message);

	g_free (message);
}

static void
web_view_gtkhtml_link_clicked (EWebViewGtkHTML *web_view,
                               const gchar *uri)
{
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	e_show_uri (parent, uri);
}

static void
web_view_gtkhtml_load_string (EWebViewGtkHTML *web_view,
                              const gchar *string)
{
	if (string != NULL && *string != '\0')
		gtk_html_load_from_string (GTK_HTML (web_view), string, -1);
	else
		e_web_view_gtkhtml_clear (web_view);
}

static void
web_view_gtkhtml_copy_clipboard (EWebViewGtkHTML *web_view)
{
	gtk_html_command (GTK_HTML (web_view), "copy");
}

static void
web_view_gtkhtml_cut_clipboard (EWebViewGtkHTML *web_view)
{
	if (e_web_view_gtkhtml_get_editable (web_view))
		gtk_html_command (GTK_HTML (web_view), "cut");
}

static void
web_view_gtkhtml_paste_clipboard (EWebViewGtkHTML *web_view)
{
	if (e_web_view_gtkhtml_get_editable (web_view))
		gtk_html_command (GTK_HTML (web_view), "paste");
}

static gboolean
web_view_gtkhtml_popup_event (EWebViewGtkHTML *web_view,
                              GdkEventButton *event,
                              const gchar *uri)
{
	e_web_view_gtkhtml_set_selected_uri (web_view, uri);
	e_web_view_gtkhtml_show_popup_menu (web_view, event, NULL, NULL);

	return TRUE;
}

static void
web_view_gtkhtml_stop_loading (EWebViewGtkHTML *web_view)
{
	g_list_foreach (
		web_view->priv->requests, (GFunc)
		web_view_gtkhtml_request_cancel, NULL);

	gtk_html_stop (GTK_HTML (web_view));
}

static void
web_view_gtkhtml_update_actions (EWebViewGtkHTML *web_view)
{
	GtkActionGroup *action_group;
	gboolean have_selection;
	gboolean scheme_is_http = FALSE;
	gboolean scheme_is_mailto = FALSE;
	gboolean uri_is_valid = FALSE;
	gboolean has_cursor_image;
	gboolean visible;
	const gchar *group_name;
	const gchar *uri;

	uri = e_web_view_gtkhtml_get_selected_uri (web_view);
	have_selection = e_web_view_gtkhtml_is_selection_active (web_view);
	has_cursor_image = e_web_view_gtkhtml_get_cursor_image (web_view) != NULL;

	/* Parse the URI early so we know if the actions will work. */
	if (uri != NULL) {
		CamelURL *curl;

		curl = camel_url_new (uri, NULL);
		uri_is_valid = (curl != NULL);
		camel_url_free (curl);

		scheme_is_http =
			(g_ascii_strncasecmp (uri, "http:", 5) == 0) ||
			(g_ascii_strncasecmp (uri, "https:", 6) == 0);

		scheme_is_mailto =
			(g_ascii_strncasecmp (uri, "mailto:", 7) == 0);
	}

	/* Allow copying the URI even if it's malformed. */
	group_name = "uri";
	visible = (uri != NULL) && !scheme_is_mailto;
	action_group = e_web_view_gtkhtml_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "http";
	visible = uri_is_valid && scheme_is_http;
	action_group = e_web_view_gtkhtml_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "mailto";
	visible = uri_is_valid && scheme_is_mailto;
	action_group = e_web_view_gtkhtml_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "image";
	visible = has_cursor_image;
	action_group = e_web_view_gtkhtml_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "selection";
	visible = have_selection;
	action_group = e_web_view_gtkhtml_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "standard";
	visible = (uri == NULL);
	action_group = e_web_view_gtkhtml_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "lockdown-printing";
	visible = (uri == NULL) && !web_view->priv->disable_printing;
	action_group = e_web_view_gtkhtml_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "lockdown-save-to-disk";
	visible = (uri == NULL) && !web_view->priv->disable_save_to_disk;
	action_group = e_web_view_gtkhtml_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);
}

static void
web_view_gtkhtml_submit_alert (EAlertSink *alert_sink,
                               EAlert *alert)
{
	GtkIconInfo *icon_info;
	EWebViewGtkHTML *web_view;
	GtkWidget *dialog;
	GString *buffer;
	const gchar *icon_name = NULL;
	const gchar *filename;
	gpointer parent;
	gchar *icon_uri;
	gint size = 0;
	GError *error = NULL;

	web_view = E_WEB_VIEW_GTKHTML (alert_sink);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	/* We use equivalent named icons instead of stock IDs,
	 * since it's easier to get the filename of the icon. */
	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
			icon_name = "dialog-information";
			break;

		case GTK_MESSAGE_WARNING:
			icon_name = "dialog-warning";
			break;

		case GTK_MESSAGE_ERROR:
			icon_name = "dialog-error";
			break;

		default:
			dialog = e_alert_dialog_new (parent, alert);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			return;
	}

	gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &size, NULL);

	icon_info = gtk_icon_theme_lookup_icon (
		gtk_icon_theme_get_default (),
		icon_name, size, GTK_ICON_LOOKUP_NO_SVG);
	g_return_if_fail (icon_info != NULL);

	filename = gtk_icon_info_get_filename (icon_info);
	icon_uri = g_filename_to_uri (filename, NULL, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	buffer = g_string_sized_new (512);

	g_string_append (
		buffer,
		"<html>"
		"<head>"
		"<meta http-equiv=\"content-type\""
		" content=\"text/html; charset=utf-8\">"
		"</head>"
		"<body>");

	g_string_append (
		buffer,
		"<table bgcolor='#000000' width='100%'"
		" cellpadding='1' cellspacing='0'>"
		"<tr>"
		"<td>"
		"<table bgcolor='#dddddd' width='100%' cellpadding='6'>"
		"<tr>");

	g_string_append_printf (
		buffer,
		"<tr>"
		"<td valign='top'>"
		"<img src='%s'/>"
		"</td>"
		"<td align='left' width='100%%'>"
		"<h3>%s</h3>"
		"%s"
		"</td>"
		"</tr>",
		icon_uri,
		e_alert_get_primary_text (alert),
		e_alert_get_secondary_text (alert));

	g_string_append (
		buffer,
		"</table>"
		"</td>"
		"</tr>"
		"</table>"
		"</body>"
		"</html>");

	e_web_view_gtkhtml_load_string (web_view, buffer->str);

	g_string_free (buffer, TRUE);

	gtk_icon_info_free (icon_info);
	g_free (icon_uri);
}

static void
web_view_gtkhtml_selectable_update_actions (ESelectable *selectable,
                                            EFocusTracker *focus_tracker,
                                            GdkAtom *clipboard_targets,
                                            gint n_clipboard_targets)
{
	EWebViewGtkHTML *web_view;
	GtkAction *action;
	/*GtkTargetList *target_list;*/
	gboolean can_paste = FALSE;
	gboolean editable;
	gboolean have_selection;
	gboolean sensitive;
	const gchar *tooltip;
	/*gint ii;*/

	web_view = E_WEB_VIEW_GTKHTML (selectable);
	editable = e_web_view_gtkhtml_get_editable (web_view);
	have_selection = e_web_view_gtkhtml_is_selection_active (web_view);

	/* XXX GtkHtml implements its own clipboard instead of using
	 *     GDK_SELECTION_CLIPBOARD, so we don't get notifications
	 *     when the clipboard contents change.  The logic below
	 *     is what we would do if GtkHtml worked properly.
	 *     Instead, we need to keep the Paste action sensitive so
	 *     its accelerator overrides GtkHtml's key binding. */
#if 0
	target_list = e_selectable_get_paste_target_list (selectable);
	for (ii = 0; ii < n_clipboard_targets && !can_paste; ii++)
		can_paste = gtk_target_list_find (
			target_list, clipboard_targets[ii], NULL);
#endif
	can_paste = TRUE;

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = editable && have_selection;
	tooltip = _("Cut the selection");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = have_selection;
	tooltip = _("Copy the selection");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = editable && can_paste;
	tooltip = _("Paste the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	sensitive = TRUE;
	tooltip = _("Select all text and images");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);
}

static void
web_view_gtkhtml_selectable_cut_clipboard (ESelectable *selectable)
{
	e_web_view_gtkhtml_cut_clipboard (E_WEB_VIEW_GTKHTML (selectable));
}

static void
web_view_gtkhtml_selectable_copy_clipboard (ESelectable *selectable)
{
	e_web_view_gtkhtml_copy_clipboard (E_WEB_VIEW_GTKHTML (selectable));
}

static void
web_view_gtkhtml_selectable_paste_clipboard (ESelectable *selectable)
{
	e_web_view_gtkhtml_paste_clipboard (E_WEB_VIEW_GTKHTML (selectable));
}

static void
web_view_gtkhtml_selectable_select_all (ESelectable *selectable)
{
	e_web_view_gtkhtml_select_all (E_WEB_VIEW_GTKHTML (selectable));
}

static void
e_web_view_gtkhtml_class_init (EWebViewGtkHTMLClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkHTMLClass *html_class;

	g_type_class_add_private (class, sizeof (EWebViewGtkHTMLPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = web_view_gtkhtml_set_property;
	object_class->get_property = web_view_gtkhtml_get_property;
	object_class->dispose = web_view_gtkhtml_dispose;
	object_class->finalize = web_view_gtkhtml_finalize;
	object_class->constructed = web_view_gtkhtml_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = web_view_gtkhtml_button_press_event;
	widget_class->scroll_event = web_view_gtkhtml_scroll_event;

	html_class = GTK_HTML_CLASS (class);
	html_class->url_requested = web_view_gtkhtml_url_requested;
	html_class->link_clicked = web_view_gtkhtml_gtkhtml_link_clicked;
	html_class->on_url = web_view_gtkhtml_on_url;
	html_class->iframe_created = web_view_gtkhtml_iframe_created;

	class->extract_uri = web_view_gtkhtml_extract_uri;
	class->hovering_over_link = web_view_gtkhtml_hovering_over_link;
	class->link_clicked = web_view_gtkhtml_link_clicked;
	class->load_string = web_view_gtkhtml_load_string;
	class->copy_clipboard = web_view_gtkhtml_copy_clipboard;
	class->cut_clipboard = web_view_gtkhtml_cut_clipboard;
	class->paste_clipboard = web_view_gtkhtml_paste_clipboard;
	class->popup_event = web_view_gtkhtml_popup_event;
	class->stop_loading = web_view_gtkhtml_stop_loading;
	class->update_actions = web_view_gtkhtml_update_actions;

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

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	g_object_class_install_property (
		object_class,
		PROP_DISABLE_PRINTING,
		g_param_spec_boolean (
			"disable-printing",
			"Disable Printing",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_DISABLE_SAVE_TO_DISK,
		g_param_spec_boolean (
			"disable-save-to-disk",
			"Disable Save-to-Disk",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_INLINE_SPELLING,
		g_param_spec_boolean (
			"inline-spelling",
			"Inline Spelling",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_LINKS,
		g_param_spec_boolean (
			"magic-links",
			"Magic Links",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_SMILEYS,
		g_param_spec_boolean (
			"magic-smileys",
			"Magic Smileys",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_OPEN_PROXY,
		g_param_spec_object (
			"open-proxy",
			"Open Proxy",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_PRINT_PROXY,
		g_param_spec_object (
			"print-proxy",
			"Print Proxy",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SAVE_AS_PROXY,
		g_param_spec_object (
			"save-as-proxy",
			"Save As Proxy",
			NULL,
			GTK_TYPE_ACTION,
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

	g_object_class_install_property (
		object_class,
		PROP_CURSOR_IMAGE,
		g_param_spec_object (
			"cursor-image",
			"Image animation at the mouse cursor",
			NULL,
			GDK_TYPE_PIXBUF_ANIMATION,
			G_PARAM_READWRITE));

	signals[COPY_CLIPBOARD] = g_signal_new (
		"copy-clipboard",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EWebViewGtkHTMLClass, copy_clipboard),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CUT_CLIPBOARD] = g_signal_new (
		"cut-clipboard",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EWebViewGtkHTMLClass, cut_clipboard),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[PASTE_CLIPBOARD] = g_signal_new (
		"paste-clipboard",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EWebViewGtkHTMLClass, paste_clipboard),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewGtkHTMLClass, popup_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__BOXED_STRING,
		G_TYPE_BOOLEAN, 2,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_STRING);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewGtkHTMLClass, status_message),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	signals[STOP_LOADING] = g_signal_new (
		"stop-loading",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewGtkHTMLClass, stop_loading),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewGtkHTMLClass, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* return TRUE when a signal handler processed the mailto URI */
	signals[PROCESS_MAILTO] = g_signal_new (
		"process-mailto",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewGtkHTMLClass, process_mailto),
		NULL, NULL,
		e_marshal_BOOLEAN__STRING,
		G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
}

static void
e_web_view_gtkhtml_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = web_view_gtkhtml_submit_alert;
}

static void
e_web_view_gtkhtml_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = web_view_gtkhtml_selectable_update_actions;
	iface->cut_clipboard = web_view_gtkhtml_selectable_cut_clipboard;
	iface->copy_clipboard = web_view_gtkhtml_selectable_copy_clipboard;
	iface->paste_clipboard = web_view_gtkhtml_selectable_paste_clipboard;
	iface->select_all = web_view_gtkhtml_selectable_select_all;
}

static void
e_web_view_gtkhtml_init (EWebViewGtkHTML *web_view)
{
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkTargetList *target_list;
	EPopupAction *popup_action;
	const gchar *domain = GETTEXT_PACKAGE;
	const gchar *id;
	GError *error = NULL;

	web_view->priv = E_WEB_VIEW_GTKHTML_GET_PRIVATE (web_view);

	ui_manager = gtk_ui_manager_new ();
	web_view->priv->ui_manager = ui_manager;

	g_signal_connect_swapped (
		ui_manager, "connect-proxy",
		G_CALLBACK (web_view_gtkhtml_connect_proxy_cb), web_view);

	target_list = gtk_target_list_new (NULL, 0);
	web_view->priv->copy_target_list = target_list;

	target_list = gtk_target_list_new (NULL, 0);
	web_view->priv->paste_target_list = target_list;

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

	action_group = gtk_action_group_new ("image");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	gtk_action_group_add_actions (
		action_group, image_entries,
		G_N_ELEMENTS (image_entries), web_view);

	action_group = gtk_action_group_new ("selection");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	gtk_action_group_add_actions (
		action_group, selection_entries,
		G_N_ELEMENTS (selection_entries), web_view);

	action_group = gtk_action_group_new ("standard");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), web_view);

	popup_action = e_popup_action_new ("open");
	gtk_action_group_add_action (action_group, GTK_ACTION (popup_action));
	g_object_unref (popup_action);

	g_object_bind_property (
		web_view, "open-proxy",
		popup_action, "related-action",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Support lockdown. */

	action_group = gtk_action_group_new ("lockdown-printing");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	popup_action = e_popup_action_new ("print");
	gtk_action_group_add_action (action_group, GTK_ACTION (popup_action));
	g_object_unref (popup_action);

	g_object_bind_property (
		web_view, "print-proxy",
		popup_action, "related-action",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	action_group = gtk_action_group_new ("lockdown-save-to-disk");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	popup_action = e_popup_action_new ("save-as");
	gtk_action_group_add_action (action_group, GTK_ACTION (popup_action));
	g_object_unref (popup_action);

	g_object_bind_property (
		web_view, "save-as-proxy",
		popup_action, "related-action",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);

	id = "org.gnome.evolution.webview";
	e_plugin_ui_register_manager (ui_manager, id, web_view);
	e_plugin_ui_enable_manager (ui_manager, id);

	e_extensible_load_extensions (E_EXTENSIBLE (web_view));
}

GtkWidget *
e_web_view_gtkhtml_new (void)
{
	return g_object_new (E_TYPE_WEB_VIEW_GTKHTML, NULL);
}

void
e_web_view_gtkhtml_clear (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	gtk_html_load_empty (GTK_HTML (web_view));
}

void
e_web_view_gtkhtml_load_string (EWebViewGtkHTML *web_view,
                                const gchar *string)
{
	EWebViewGtkHTMLClass *class;

	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	class = E_WEB_VIEW_GTKHTML_GET_CLASS (web_view);
	g_return_if_fail (class->load_string != NULL);

	class->load_string (web_view, string);
}

gboolean
e_web_view_gtkhtml_get_animate (EWebViewGtkHTML *web_view)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_web_view_set_animate(). */

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_get_animate (GTK_HTML (web_view));
}

void
e_web_view_gtkhtml_set_animate (EWebViewGtkHTML *web_view,
                                gboolean animate)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_animate()
	 *     so we can get a "notify::animate" signal. */

	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (gtk_html_get_animate (GTK_HTML (web_view)) == animate)
		return;

	gtk_html_set_animate (GTK_HTML (web_view), animate);

	g_object_notify (G_OBJECT (web_view), "animate");
}

gboolean
e_web_view_gtkhtml_get_caret_mode (EWebViewGtkHTML *web_view)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_web_view_set_caret_mode(). */

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_get_caret_mode (GTK_HTML (web_view));
}

void
e_web_view_gtkhtml_set_caret_mode (EWebViewGtkHTML *web_view,
                                   gboolean caret_mode)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_caret_mode()
	 *     so we can get a "notify::caret-mode" signal. */

	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (gtk_html_get_caret_mode (GTK_HTML (web_view)) == caret_mode)
		return;

	gtk_html_set_caret_mode (GTK_HTML (web_view), caret_mode);

	g_object_notify (G_OBJECT (web_view), "caret-mode");
}

GtkTargetList *
e_web_view_gtkhtml_get_copy_target_list (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	return web_view->priv->copy_target_list;
}

gboolean
e_web_view_gtkhtml_get_disable_printing (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return web_view->priv->disable_printing;
}

void
e_web_view_gtkhtml_set_disable_printing (EWebViewGtkHTML *web_view,
                                         gboolean disable_printing)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (web_view->priv->disable_printing == disable_printing)
		return;

	web_view->priv->disable_printing = disable_printing;

	g_object_notify (G_OBJECT (web_view), "disable-printing");
}

gboolean
e_web_view_gtkhtml_get_disable_save_to_disk (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return web_view->priv->disable_save_to_disk;
}

void
e_web_view_gtkhtml_set_disable_save_to_disk (EWebViewGtkHTML *web_view,
                                             gboolean disable_save_to_disk)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (web_view->priv->disable_save_to_disk == disable_save_to_disk)
		return;

	web_view->priv->disable_save_to_disk = disable_save_to_disk;

	g_object_notify (G_OBJECT (web_view), "disable-save-to-disk");
}

gboolean
e_web_view_gtkhtml_get_editable (EWebViewGtkHTML *web_view)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_web_view_set_editable(). */

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_get_editable (GTK_HTML (web_view));
}

void
e_web_view_gtkhtml_set_editable (EWebViewGtkHTML *web_view,
                                 gboolean editable)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_editable()
	 *     so we can get a "notify::editable" signal. */

	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (gtk_html_get_editable (GTK_HTML (web_view)) == editable)
		return;

	gtk_html_set_editable (GTK_HTML (web_view), editable);

	g_object_notify (G_OBJECT (web_view), "editable");
}

gboolean
e_web_view_gtkhtml_get_inline_spelling (EWebViewGtkHTML *web_view)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_web_view_set_inline_spelling(). */

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_get_inline_spelling (GTK_HTML (web_view));
}

void
e_web_view_gtkhtml_set_inline_spelling (EWebViewGtkHTML *web_view,
                                        gboolean inline_spelling)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_inline_spelling()
	 *     so we get a "notify::inline-spelling" signal. */

	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (gtk_html_get_inline_spelling (GTK_HTML (web_view)) == inline_spelling)
		return;

	gtk_html_set_inline_spelling (GTK_HTML (web_view), inline_spelling);

	g_object_notify (G_OBJECT (web_view), "inline-spelling");
}

gboolean
e_web_view_gtkhtml_get_magic_links (EWebViewGtkHTML *web_view)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_web_view_set_magic_links(). */

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_get_magic_links (GTK_HTML (web_view));
}

void
e_web_view_gtkhtml_set_magic_links (EWebViewGtkHTML *web_view,
                                    gboolean magic_links)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_magic_links()
	 *     so we can get a "notify::magic-links" signal. */

	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (gtk_html_get_magic_links (GTK_HTML (web_view)) == magic_links)
		return;

	gtk_html_set_magic_links (GTK_HTML (web_view), magic_links);

	g_object_notify (G_OBJECT (web_view), "magic-links");
}

gboolean
e_web_view_gtkhtml_get_magic_smileys (EWebViewGtkHTML *web_view)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_web_view_set_magic_smileys(). */

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_get_magic_smileys (GTK_HTML (web_view));
}

void
e_web_view_gtkhtml_set_magic_smileys (EWebViewGtkHTML *web_view,
                                      gboolean magic_smileys)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_magic_smileys()
	 *     so we can get a "notify::magic-smileys" signal. */

	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (gtk_html_get_magic_smileys (GTK_HTML (web_view)) == magic_smileys)
		return;

	gtk_html_set_magic_smileys (GTK_HTML (web_view), magic_smileys);

	g_object_notify (G_OBJECT (web_view), "magic-smileys");
}

const gchar *
e_web_view_gtkhtml_get_selected_uri (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	return web_view->priv->selected_uri;
}

void
e_web_view_gtkhtml_set_selected_uri (EWebViewGtkHTML *web_view,
                                     const gchar *selected_uri)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (g_strcmp0 (web_view->priv->selected_uri, selected_uri) == 0)
		return;

	g_free (web_view->priv->selected_uri);
	web_view->priv->selected_uri = g_strdup (selected_uri);

	g_object_notify (G_OBJECT (web_view), "selected-uri");
}

GdkPixbufAnimation *
e_web_view_gtkhtml_get_cursor_image (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	return web_view->priv->cursor_image;
}

void
e_web_view_gtkhtml_set_cursor_image (EWebViewGtkHTML *web_view,
                                     GdkPixbufAnimation *image)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (web_view->priv->cursor_image == image)
		return;

	if (image != NULL)
		g_object_ref (image);

	if (web_view->priv->cursor_image != NULL)
		g_object_unref (web_view->priv->cursor_image);

	web_view->priv->cursor_image = image;

	g_object_notify (G_OBJECT (web_view), "cursor-image");
}

GtkAction *
e_web_view_gtkhtml_get_open_proxy (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	return web_view->priv->open_proxy;
}

void
e_web_view_gtkhtml_set_open_proxy (EWebViewGtkHTML *web_view,
                                   GtkAction *open_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (web_view->priv->open_proxy == open_proxy)
		return;

	if (open_proxy != NULL) {
		g_return_if_fail (GTK_IS_ACTION (open_proxy));
		g_object_ref (open_proxy);
	}

	if (web_view->priv->open_proxy != NULL)
		g_object_unref (web_view->priv->open_proxy);

	web_view->priv->open_proxy = open_proxy;

	g_object_notify (G_OBJECT (web_view), "open-proxy");
}

GtkTargetList *
e_web_view_gtkhtml_get_paste_target_list (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	return web_view->priv->paste_target_list;
}

GtkAction *
e_web_view_gtkhtml_get_print_proxy (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	return web_view->priv->print_proxy;
}

void
e_web_view_gtkhtml_set_print_proxy (EWebViewGtkHTML *web_view,
                                    GtkAction *print_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (web_view->priv->print_proxy == print_proxy)
		return;

	if (print_proxy != NULL) {
		g_return_if_fail (GTK_IS_ACTION (print_proxy));
		g_object_ref (print_proxy);
	}

	if (web_view->priv->print_proxy != NULL)
		g_object_unref (web_view->priv->print_proxy);

	web_view->priv->print_proxy = print_proxy;

	g_object_notify (G_OBJECT (web_view), "print-proxy");
}

GtkAction *
e_web_view_gtkhtml_get_save_as_proxy (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	return web_view->priv->save_as_proxy;
}

void
e_web_view_gtkhtml_set_save_as_proxy (EWebViewGtkHTML *web_view,
                                      GtkAction *save_as_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	if (web_view->priv->save_as_proxy == save_as_proxy)
		return;

	if (save_as_proxy != NULL) {
		g_return_if_fail (GTK_IS_ACTION (save_as_proxy));
		g_object_ref (save_as_proxy);
	}

	if (web_view->priv->save_as_proxy != NULL)
		g_object_unref (web_view->priv->save_as_proxy);

	web_view->priv->save_as_proxy = save_as_proxy;

	g_object_notify (G_OBJECT (web_view), "save-as-proxy");
}

GtkAction *
e_web_view_gtkhtml_get_action (EWebViewGtkHTML *web_view,
                               const gchar *action_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_web_view_gtkhtml_get_ui_manager (web_view);

	return e_lookup_action (ui_manager, action_name);
}

GtkActionGroup *
e_web_view_gtkhtml_get_action_group (EWebViewGtkHTML *web_view,
                                     const gchar *group_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_web_view_gtkhtml_get_ui_manager (web_view);

	return e_lookup_action_group (ui_manager, group_name);
}

gchar *
e_web_view_gtkhtml_extract_uri (EWebViewGtkHTML *web_view,
                                GdkEventButton *event,
                                GtkHTML *frame)
{
	EWebViewGtkHTMLClass *class;

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	if (frame == NULL)
		frame = GTK_HTML (web_view);

	class = E_WEB_VIEW_GTKHTML_GET_CLASS (web_view);
	g_return_val_if_fail (class->extract_uri != NULL, NULL);

	return class->extract_uri (web_view, event, frame);
}

void
e_web_view_gtkhtml_copy_clipboard (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	g_signal_emit (web_view, signals[COPY_CLIPBOARD], 0);
}

void
e_web_view_gtkhtml_cut_clipboard (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	g_signal_emit (web_view, signals[CUT_CLIPBOARD], 0);
}

gboolean
e_web_view_gtkhtml_is_selection_active (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_command (GTK_HTML (web_view), "is-selection-active");
}

void
e_web_view_gtkhtml_paste_clipboard (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	g_signal_emit (web_view, signals[PASTE_CLIPBOARD], 0);
}

gboolean
e_web_view_gtkhtml_scroll_forward (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_command (GTK_HTML (web_view), "scroll-forward");
}

gboolean
e_web_view_gtkhtml_scroll_backward (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), FALSE);

	return gtk_html_command (GTK_HTML (web_view), "scroll-backward");
}

void
e_web_view_gtkhtml_select_all (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	gtk_html_command (GTK_HTML (web_view), "select-all");
}

void
e_web_view_gtkhtml_unselect_all (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	gtk_html_command (GTK_HTML (web_view), "unselect-all");
}

void
e_web_view_gtkhtml_zoom_100 (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	gtk_html_command (GTK_HTML (web_view), "zoom-reset");
}

void
e_web_view_gtkhtml_zoom_in (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	gtk_html_command (GTK_HTML (web_view), "zoom-in");
}

void
e_web_view_gtkhtml_zoom_out (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	gtk_html_command (GTK_HTML (web_view), "zoom-out");
}

GtkUIManager *
e_web_view_gtkhtml_get_ui_manager (EWebViewGtkHTML *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	return web_view->priv->ui_manager;
}

GtkWidget *
e_web_view_gtkhtml_get_popup_menu (EWebViewGtkHTML *web_view)
{
	GtkUIManager *ui_manager;
	GtkWidget *menu;

	g_return_val_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view), NULL);

	ui_manager = e_web_view_gtkhtml_get_ui_manager (web_view);
	menu = gtk_ui_manager_get_widget (ui_manager, "/context");
	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	return menu;
}

void
e_web_view_gtkhtml_show_popup_menu (EWebViewGtkHTML *web_view,
                                    GdkEventButton *event,
                                    GtkMenuPositionFunc func,
                                    gpointer user_data)
{
	GtkWidget *menu;

	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	e_web_view_gtkhtml_update_actions (web_view);

	menu = e_web_view_gtkhtml_get_popup_menu (web_view);

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
e_web_view_gtkhtml_status_message (EWebViewGtkHTML *web_view,
                                   const gchar *status_message)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	g_signal_emit (web_view, signals[STATUS_MESSAGE], 0, status_message);
}

void
e_web_view_gtkhtml_stop_loading (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	g_signal_emit (web_view, signals[STOP_LOADING], 0);
}

void
e_web_view_gtkhtml_update_actions (EWebViewGtkHTML *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW_GTKHTML (web_view));

	g_signal_emit (web_view, signals[UPDATE_ACTIONS], 0);
}
