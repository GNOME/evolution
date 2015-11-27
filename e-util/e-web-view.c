/*
 * e-web-view.c
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

#include "e-web-view.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <pango/pango.h>

#include <camel/camel.h>
#include <libebackend/libebackend.h>

#include <libsoup/soup.h>

#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-file-request.h"
#include "e-misc-utils.h"
#include "e-plugin-ui.h"
#include "e-popup-action.h"
#include "e-selectable.h"
#include "e-stock-request.h"

#define E_WEB_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEB_VIEW, EWebViewPrivate))

typedef enum {
	E_WEB_VIEW_ZOOM_HACK_STATE_NONE,
	E_WEB_VIEW_ZOOM_HACK_STATE_ZOOMED_IN,
	E_WEB_VIEW_ZOOM_HACK_STATE_ZOOMED_OUT
} EWebViewZoomHackState;

typedef struct _AsyncContext AsyncContext;

struct _EWebViewPrivate {
	GtkUIManager *ui_manager;
	gchar *selected_uri;
	gchar *cursor_image_src;

	GQueue highlights;

	GtkAction *open_proxy;
	GtkAction *print_proxy;
	GtkAction *save_as_proxy;

	/* Lockdown Options */
	gboolean disable_printing;
	gboolean disable_save_to_disk;

	gboolean caret_mode;

	GSettings *font_settings;
	gulong font_name_changed_handler_id;
	gulong monospace_font_name_changed_handler_id;

	GSettings *aliasing_settings;
	gulong antialiasing_changed_handler_id;

	GHashTable *old_settings;

	/* To workaround webkit bug:
	 * https://bugs.webkit.org/show_bug.cgi?id=89553 */
	EWebViewZoomHackState zoom_hack_state;

	gboolean has_hover_link;
};

struct _AsyncContext {
	EActivity *activity;
	GFile *destination;
	GInputStream *input_stream;
};

enum {
	PROP_0,
	PROP_CARET_MODE,
	PROP_COPY_TARGET_LIST,
	PROP_CURSOR_IMAGE_SRC,
	PROP_DISABLE_PRINTING,
	PROP_DISABLE_SAVE_TO_DISK,
	PROP_OPEN_PROXY,
	PROP_PRINT_PROXY,
	PROP_SAVE_AS_PROXY,
	PROP_SELECTED_URI
};

enum {
	NEW_ACTIVITY,
	POPUP_EVENT,
	STATUS_MESSAGE,
	STOP_LOADING,
	UPDATE_ACTIONS,
	PROCESS_MAILTO,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GOnce disable_webkit_3rd_party_plugins_once = G_ONCE_INIT;

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
"      <menuitem action='mailto-copy-raw'/>"
"      <menuitem action='image-copy'/>"
"      <menuitem action='image-save'/>"
"    </placeholder>"
"    <placeholder name='custom-actions-3'/>"
"    <separator/>"
"    <menuitem action='select-all'/>"
"    <placeholder name='inspect-menu' />"
"  </popup>"
"</ui>";

/* Forward Declarations */
static void e_web_view_alert_sink_init (EAlertSinkInterface *iface);
static void e_web_view_selectable_init (ESelectableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EWebView,
	e_web_view,
	WEBKIT_TYPE_WEB_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK,
		e_web_view_alert_sink_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_SELECTABLE,
		e_web_view_selectable_init))

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->activity);
	g_clear_object (&async_context->destination);
	g_clear_object (&async_context->input_stream);

	g_slice_free (AsyncContext, async_context);
}

static void
action_copy_clipboard_cb (GtkAction *action,
                          EWebView *web_view)
{
	e_web_view_copy_clipboard (web_view);
}

static void
action_http_open_cb (GtkAction *action,
                     EWebView *web_view)
{
	const gchar *uri;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	e_show_uri (parent, uri);
}

static void
webview_mailto_copy (EWebView *web_view,
		     gboolean only_email_address)
{
	CamelURL *curl;
	CamelInternetAddress *inet_addr;
	GtkClipboard *clipboard;
	const gchar *uri, *name = NULL, *email = NULL;
	gchar *text;

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	/* This should work because we checked it in update_actions(). */
	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	inet_addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
	if (only_email_address &&
	    camel_internet_address_get (inet_addr, 0, &name, &email) &&
	    email && *email) {
		text = g_strdup (email);
	} else {
		text = camel_address_format (CAMEL_ADDRESS (inet_addr));
		if (text == NULL || *text == '\0')
			text = g_strdup (uri + strlen ("mailto:"));
	}

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
action_mailto_copy_cb (GtkAction *action,
                       EWebView *web_view)
{
	webview_mailto_copy (web_view, FALSE);
}

static void
action_mailto_copy_raw_cb (GtkAction *action,
			   EWebView *web_view)
{
	webview_mailto_copy (web_view, TRUE);
}

static void
action_select_all_cb (GtkAction *action,
                      EWebView *web_view)
{
	e_web_view_select_all (web_view);
}

static void
action_send_message_cb (GtkAction *action,
                        EWebView *web_view)
{
	const gchar *uri;
	gpointer parent;
	gboolean handled;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	handled = FALSE;
	g_signal_emit (web_view, signals[PROCESS_MAILTO], 0, uri, &handled);

	if (!handled)
		e_show_uri (parent, uri);
}

static void
action_uri_copy_cb (GtkAction *action,
                    EWebView *web_view)
{
	GtkClipboard *clipboard;
	const gchar *uri;

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clipboard, uri, -1);
	gtk_clipboard_store (clipboard);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, uri, -1);
	gtk_clipboard_store (clipboard);
}

static void
action_image_copy_cb (GtkAction *action,
                      EWebView *web_view)
{
	e_web_view_cursor_image_copy (web_view);
}

static void
action_image_save_cb (GtkAction *action,
                      EWebView *web_view)
{
	e_web_view_cursor_image_save (web_view);
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

	{ "mailto-copy-raw",
	  "edit-copy",
	  N_("Copy _Raw Email Address"),
	  NULL,
	  N_("Copy the raw email address to the clipboard"),
	  G_CALLBACK (action_mailto_copy_raw_cb) },

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
	  G_CALLBACK (action_image_copy_cb) },

	{ "image-save",
	  "document-save",
	  N_("Save _Image..."),
	  "<Control>s",
	  N_("Save the image to a file"),
	  G_CALLBACK (action_image_save_cb) }
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

static void
web_view_init_web_settings (WebKitWebView *web_view)
{
	WebKitWebSettings *web_settings;

	web_settings = webkit_web_settings_new ();

	g_object_set (
		G_OBJECT (web_settings),
		"default-encoding", "UTF-8",
		"enable-dns-prefetching", FALSE,
		"enable-frame-flattening", TRUE,
		"enable-java-applet", FALSE,
		"enable-html5-database", FALSE,
		"enable-html5-local-storage", FALSE,
		"enable-offline-web-application-cache", FALSE,
		"enable-site-specific-quirks", TRUE,
		"enable-scripts", FALSE,
		"respect-image-orientation", TRUE,
		NULL);

	e_binding_bind_property (
		web_settings, "enable-caret-browsing",
		web_view, "caret-mode",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	webkit_web_view_set_settings (web_view, web_settings);

	g_object_unref (web_settings);
}

static void
web_view_menu_item_select_cb (EWebView *web_view,
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

	e_web_view_status_message (web_view, tooltip);
}

static void
web_view_update_document_highlights (EWebView *web_view)
{
	WebKitWebView *webkit_web_view;
	GList *head, *link;

	webkit_web_view = WEBKIT_WEB_VIEW (web_view);

	head = g_queue_peek_head_link (&web_view->priv->highlights);

	for (link = head; link != NULL; link = g_list_next (link))
		webkit_web_view_mark_text_matches (
			webkit_web_view, link->data, FALSE, 0);

	webkit_web_view_set_highlight_text_matches (webkit_web_view, TRUE);
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

static gboolean
web_view_context_menu_cb (WebKitWebView *webkit_web_view,
                          GtkWidget *default_menu,
                          WebKitHitTestResult *hit_test_result,
                          gboolean triggered_with_keyboard)
{
	WebKitHitTestResultContext context;
	EWebView *web_view;
	gboolean event_handled = FALSE;
	gchar *link_uri = NULL;

	web_view = E_WEB_VIEW (webkit_web_view);

	g_free (web_view->priv->cursor_image_src);
	web_view->priv->cursor_image_src = NULL;

	if (hit_test_result == NULL)
		return FALSE;

	g_object_get (hit_test_result, "context", &context, NULL);

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE) {
		gchar *image_uri = NULL;

		g_object_get (hit_test_result, "image-uri", &image_uri, NULL);

		if (image_uri != NULL) {
			g_free (web_view->priv->cursor_image_src);
			web_view->priv->cursor_image_src = image_uri;
		}
	}

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
		g_object_get (hit_test_result, "link-uri", &link_uri, NULL);

	g_signal_emit (
		web_view,
		signals[POPUP_EVENT], 0,
		link_uri, &event_handled);

	g_free (link_uri);

	return event_handled;
}

static GtkWidget *
web_view_create_plugin_widget_cb (EWebView *web_view,
                                  const gchar *mime_type,
                                  const gchar *uri,
                                  GHashTable *param)
{
	EWebViewClass *class;

	/* XXX WebKitWebView does not provide a class method for
	 *     this signal, so we do so we can override the default
	 *     behavior from subclasses for special URI types. */

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_val_if_fail (class->create_plugin_widget != NULL, NULL);

	return class->create_plugin_widget (web_view, mime_type, uri, param);
}

static void
web_view_hovering_over_link_cb (EWebView *web_view,
                                const gchar *title,
                                const gchar *uri)
{
	EWebViewClass *class;

	web_view->priv->has_hover_link = uri && *uri;

	/* XXX WebKitWebView does not provide a class method for
	 *     this signal, so we do so we can override the default
	 *     behavior from subclasses for special URI types. */

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_if_fail (class->hovering_over_link != NULL);

	class->hovering_over_link (web_view, title, uri);
}

static gboolean
web_view_navigation_policy_decision_requested_cb (EWebView *web_view,
                                                  WebKitWebFrame *frame,
                                                  WebKitNetworkRequest *request,
                                                  WebKitWebNavigationAction *navigation_action,
                                                  WebKitWebPolicyDecision *policy_decision)
{
	EWebViewClass *class;
	WebKitWebNavigationReason reason;
	const gchar *uri, *frame_uri;

	reason = webkit_web_navigation_action_get_reason (navigation_action);
	if (reason != WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED)
		return FALSE;

	uri = webkit_network_request_get_uri (request);
	frame_uri = webkit_web_frame_get_uri (frame);

	/* Allow navigation through fragments in page */
	if (uri && *uri && frame_uri && *frame_uri) {
		SoupURI *uri_link, *uri_frame;

		uri_link = soup_uri_new (uri);
		uri_frame = soup_uri_new (frame_uri);
		if (uri_link && uri_frame) {
			const gchar *tmp1, *tmp2;

			tmp1 = soup_uri_get_scheme (uri_link);
			tmp2 = soup_uri_get_scheme (uri_frame);

			/* The scheme on both URIs should be the same */
			if (tmp1 && tmp2) {
				if (g_ascii_strcasecmp (tmp1, tmp2) != 0)
					goto free_uris;
			}

			tmp1 = soup_uri_get_host (uri_link);
			tmp2 = soup_uri_get_host (uri_frame);

			/* The host on both URIs should be the same */
			if (tmp1 && tmp2) {
				if (g_ascii_strcasecmp (tmp1, tmp2) != 0)
					goto free_uris;
			}

			/* URI from link should have fragment set - could be empty */
			if (soup_uri_get_fragment (uri_link)) {
				soup_uri_free (uri_link);
				soup_uri_free (uri_frame);
				webkit_web_policy_decision_use (policy_decision);
				return TRUE;
			}
		}

 free_uris:
		if (uri_link)
			soup_uri_free (uri_link);
		if (uri_frame)
			soup_uri_free (uri_frame);
	}

	/* XXX WebKitWebView does not provide a class method for
	 *     this signal, so we do so we can override the default
	 *     behavior from subclasses for special URI types. */

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_val_if_fail (class->link_clicked != NULL, FALSE);

	webkit_web_policy_decision_ignore (policy_decision);

	class->link_clicked (web_view, uri);

	return TRUE;
}

static void
style_updated_cb (EWebView *web_view)
{
	GdkRGBA color;
	gchar *color_value;
	gchar *style;
	GtkStateFlags state_flags;
	GtkStyleContext *style_context;
	gboolean backdrop;

	state_flags = gtk_widget_get_state_flags (GTK_WIDGET (web_view));
	style_context = gtk_widget_get_style_context (GTK_WIDGET (web_view));
	backdrop = (state_flags & GTK_STATE_FLAG_BACKDROP) != 0;

	if (gtk_style_context_lookup_color (
			style_context,
			backdrop ? "theme_unfocused_base_color" : "theme_base_color",
			&color))
		color_value = g_strdup_printf ("#%06x", e_rgba_to_value (&color));
	else
		color_value = g_strdup (E_UTILS_DEFAULT_THEME_BASE_COLOR);

	style = g_strconcat ("background-color: ", color_value, ";", NULL);

	e_web_view_add_css_rule_into_style_sheet (
		web_view,
		"-e-web-view-css-sheet",
		".-e-web-view-background-color",
		style);

	g_free (color_value);
	g_free (style);

	if (gtk_style_context_lookup_color (
			style_context,
			backdrop ? "theme_unfocused_fg_color" : "theme_fg_color",
			&color))
		color_value = g_strdup_printf ("#%06x", e_rgba_to_value (&color));
	else
		color_value = g_strdup (E_UTILS_DEFAULT_THEME_FG_COLOR);

	style = g_strconcat ("color: ", color_value, ";", NULL);

	e_web_view_add_css_rule_into_style_sheet (
		web_view,
		"-e-web-view-css-sheet",
		".-e-web-view-text-color",
		style);

	g_free (color_value);
	g_free (style);
}

static void
web_view_load_status_changed_cb (WebKitWebView *webkit_web_view,
                                 GParamSpec *pspec,
                                 gpointer user_data)
{
	WebKitLoadStatus status;
	EWebView *web_view;

	web_view = E_WEB_VIEW (webkit_web_view);

	status = webkit_web_view_get_load_status (webkit_web_view);

	if (web_view->priv->zoom_hack_state == E_WEB_VIEW_ZOOM_HACK_STATE_NONE &&
	    status == WEBKIT_LOAD_COMMITTED) {
		if (webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (web_view)) > 0.9999) {
			e_web_view_zoom_out (web_view);
			web_view->priv->zoom_hack_state = E_WEB_VIEW_ZOOM_HACK_STATE_ZOOMED_OUT;
		} else {
			e_web_view_zoom_in (web_view);
			web_view->priv->zoom_hack_state = E_WEB_VIEW_ZOOM_HACK_STATE_ZOOMED_IN;
		}
	} else if (web_view->priv->zoom_hack_state != E_WEB_VIEW_ZOOM_HACK_STATE_NONE &&
		   status == WEBKIT_LOAD_FAILED) {
		if (web_view->priv->zoom_hack_state == E_WEB_VIEW_ZOOM_HACK_STATE_ZOOMED_IN)
			e_web_view_zoom_out (web_view);
		else
			e_web_view_zoom_in (web_view);

		web_view->priv->zoom_hack_state = E_WEB_VIEW_ZOOM_HACK_STATE_NONE;
	}

	if (status != WEBKIT_LOAD_FINISHED)
		return;

	style_updated_cb (web_view);

	web_view_update_document_highlights (web_view);

	if (web_view->priv->zoom_hack_state == E_WEB_VIEW_ZOOM_HACK_STATE_NONE) {
		/* This may not happen, but just in case keep it here. */
		if (webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (web_view)) > 0.9999) {
			e_web_view_zoom_out (web_view);
			e_web_view_zoom_in (web_view);
		} else {
			e_web_view_zoom_in (web_view);
			e_web_view_zoom_out (web_view);
		}
	} else {
		if (web_view->priv->zoom_hack_state == E_WEB_VIEW_ZOOM_HACK_STATE_ZOOMED_IN)
			e_web_view_zoom_out (web_view);
		else
			e_web_view_zoom_in (web_view);

		web_view->priv->zoom_hack_state = E_WEB_VIEW_ZOOM_HACK_STATE_NONE;
	}
}

static void
web_view_set_property (GObject *object,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CARET_MODE:
			e_web_view_set_caret_mode (
				E_WEB_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_CURSOR_IMAGE_SRC:
			e_web_view_set_cursor_image_src (
				E_WEB_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_DISABLE_PRINTING:
			e_web_view_set_disable_printing (
				E_WEB_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_DISABLE_SAVE_TO_DISK:
			e_web_view_set_disable_save_to_disk (
				E_WEB_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_OPEN_PROXY:
			e_web_view_set_open_proxy (
				E_WEB_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_PRINT_PROXY:
			e_web_view_set_print_proxy (
				E_WEB_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SAVE_AS_PROXY:
			e_web_view_set_save_as_proxy (
				E_WEB_VIEW (object),
				g_value_get_object (value));
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
		case PROP_CARET_MODE:
			g_value_set_boolean (
				value, e_web_view_get_caret_mode (
				E_WEB_VIEW (object)));
			return;

		case PROP_CURSOR_IMAGE_SRC:
			g_value_set_string (
				value, e_web_view_get_cursor_image_src (
				E_WEB_VIEW (object)));
			return;

		case PROP_DISABLE_PRINTING:
			g_value_set_boolean (
				value, e_web_view_get_disable_printing (
				E_WEB_VIEW (object)));
			return;

		case PROP_DISABLE_SAVE_TO_DISK:
			g_value_set_boolean (
				value, e_web_view_get_disable_save_to_disk (
				E_WEB_VIEW (object)));
			return;

		case PROP_OPEN_PROXY:
			g_value_set_object (
				value, e_web_view_get_open_proxy (
				E_WEB_VIEW (object)));
			return;

		case PROP_PRINT_PROXY:
			g_value_set_object (
				value, e_web_view_get_print_proxy (
				E_WEB_VIEW (object)));
			return;

		case PROP_SAVE_AS_PROXY:
			g_value_set_object (
				value, e_web_view_get_save_as_proxy (
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

	if (priv->font_name_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->font_settings,
			priv->font_name_changed_handler_id);
		priv->font_name_changed_handler_id = 0;
	}

	if (priv->monospace_font_name_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->font_settings,
			priv->monospace_font_name_changed_handler_id);
		priv->monospace_font_name_changed_handler_id = 0;
	}

	if (priv->antialiasing_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->aliasing_settings,
			priv->antialiasing_changed_handler_id);
		priv->antialiasing_changed_handler_id = 0;
	}

	g_clear_object (&priv->ui_manager);
	g_clear_object (&priv->open_proxy);
	g_clear_object (&priv->print_proxy);
	g_clear_object (&priv->save_as_proxy);
	g_clear_object (&priv->aliasing_settings);
	g_clear_object (&priv->font_settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_web_view_parent_class)->dispose (object);
}

static void
web_view_finalize (GObject *object)
{
	EWebViewPrivate *priv;

	priv = E_WEB_VIEW_GET_PRIVATE (object);

	g_free (priv->selected_uri);
	g_free (priv->cursor_image_src);

	while (!g_queue_is_empty (&priv->highlights))
		g_free (g_queue_pop_head (&priv->highlights));

	if (priv->old_settings) {
		g_hash_table_destroy (priv->old_settings);
		priv->old_settings = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_web_view_parent_class)->finalize (object);
}

static void
web_view_constructed (GObject *object)
{
#ifndef G_OS_WIN32
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.desktop.lockdown");

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

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_web_view_parent_class)->constructed (object);
}

static gboolean
web_view_scroll_event (GtkWidget *widget,
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
			} else if (event->delta_y >= 1e-9 || event->delta_y <= -1e-9) {
				return TRUE;
			} else {
				return FALSE;
			}
		}

		switch (direction) {
			case GDK_SCROLL_UP:
				e_web_view_zoom_in (E_WEB_VIEW (widget));
				return TRUE;
			case GDK_SCROLL_DOWN:
				e_web_view_zoom_out (E_WEB_VIEW (widget));
				return TRUE;
			default:
				break;
		}
	}

	return FALSE;
}

static gboolean
web_view_drag_motion (GtkWidget *widget,
                      GdkDragContext *context,
                      gint x,
                      gint y,
                      guint time_)
{
	return FALSE;
}

static GtkWidget *
web_view_create_plugin_widget (EWebView *web_view,
                               const gchar *mime_type,
                               const gchar *uri,
                               GHashTable *param)
{
	GtkWidget *widget = NULL;

	if (g_strcmp0 (mime_type, "image/x-themed-icon") == 0) {
		GtkIconTheme *icon_theme;
		GdkPixbuf *pixbuf;
		gpointer data;
		glong size = 0;
		GError *error = NULL;

		icon_theme = gtk_icon_theme_get_default ();

		if (size == 0) {
			data = g_hash_table_lookup (param, "width");
			if (data != NULL)
				size = MAX (size, strtol (data, NULL, 10));
		}

		if (size == 0) {
			data = g_hash_table_lookup (param, "height");
			if (data != NULL)
				size = MAX (size, strtol (data, NULL, 10));
		}

		if (size == 0)
			size = 32;  /* arbitrary default */

		pixbuf = gtk_icon_theme_load_icon (
			icon_theme, uri, size, GTK_ICON_LOOKUP_FORCE_SIZE, &error);
		if (pixbuf != NULL) {
			widget = gtk_image_new_from_pixbuf (pixbuf);
			g_object_unref (pixbuf);
		} else if (error != NULL) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
	}

	return widget;
}

static void
web_view_hovering_over_link (EWebView *web_view,
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
	else if (g_str_has_prefix (uri, "callto:") ||
		 g_str_has_prefix (uri, "h323:") ||
		 g_str_has_prefix (uri, "sip:") ||
		 g_str_has_prefix (uri, "tel:"))
		format = _("Click to call %s");
	else if (g_str_has_prefix (uri, "##"))
		message = g_strdup (_("Click to hide/unhide addresses"));
	else if (g_str_has_prefix (uri, "mail:")) {
		const gchar *fragment;
		SoupURI *soup_uri;

		soup_uri = soup_uri_new (uri);
		if (!soup_uri)
			goto exit;

		fragment = soup_uri_get_fragment (soup_uri);
		if (fragment && *fragment)
			message = g_strdup_printf (_("Go to the section %s of the message"), fragment);
		else
			message = g_strdup (_("Go to the beginning of the message"));

		soup_uri_free (soup_uri);
	} else
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
	e_web_view_status_message (web_view, message);

	g_free (message);
}

static void
web_view_link_clicked (EWebView *web_view,
                       const gchar *uri)
{
	gpointer parent;

	if (uri && g_ascii_strncasecmp (uri, "mailto:", 7) == 0) {
		gboolean handled = FALSE;

		g_signal_emit (
			web_view, signals[PROCESS_MAILTO], 0, uri, &handled);

		if (handled)
			return;
	}

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	e_show_uri (parent, uri);
}

static void
web_view_load_string (EWebView *web_view,
                      const gchar *string)
{
	if (string == NULL)
		string = "";

	webkit_web_view_load_string (
		WEBKIT_WEB_VIEW (web_view),
		string, "text/html", "UTF-8", "evo-file:///");
}

static void
web_view_load_uri (EWebView *web_view,
                   const gchar *uri)
{
	if (uri == NULL)
		uri = "about:blank";

	webkit_web_view_load_uri (WEBKIT_WEB_VIEW (web_view), uri);
}

static gchar *
web_view_redirect_uri (EWebView *web_view,
                       const gchar *uri)
{
	return g_strdup (uri);
}

static gchar *
web_view_suggest_filename (EWebView *web_view,
                           const gchar *uri)
{
	const gchar *cp;

	/* Try to derive a filename from the last path segment. */

	cp = strrchr (uri, '/');
	if (cp != NULL) {
		if (strchr (cp, '?') == NULL)
			cp++;
		else
			cp = NULL;
	}

	return g_strdup (cp);
}

static gboolean
web_view_popup_event (EWebView *web_view,
                      const gchar *uri)
{
	e_web_view_set_selected_uri (web_view, uri);
	e_web_view_show_popup_menu (web_view);

	return TRUE;
}

static void
web_view_stop_loading (EWebView *web_view)
{
	webkit_web_view_stop_loading (WEBKIT_WEB_VIEW (web_view));
}

static void
web_view_update_actions (EWebView *web_view)
{
	GtkActionGroup *action_group;
	gboolean can_copy;
	gboolean scheme_is_http = FALSE;
	gboolean scheme_is_mailto = FALSE;
	gboolean uri_is_valid = FALSE;
	gboolean visible;
	const gchar *cursor_image_src;
	const gchar *group_name;
	const gchar *uri;

	uri = e_web_view_get_selected_uri (web_view);
	can_copy = webkit_web_view_can_copy_clipboard (WEBKIT_WEB_VIEW (web_view));
	cursor_image_src = e_web_view_get_cursor_image_src (web_view);

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
	action_group = e_web_view_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "http";
	visible = uri_is_valid && scheme_is_http;
	action_group = e_web_view_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "mailto";
	visible = uri_is_valid && scheme_is_mailto;
	action_group = e_web_view_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	if (visible) {
		CamelURL *curl;

		curl = camel_url_new (uri, NULL);
		if (curl) {
			CamelInternetAddress *inet_addr;
			const gchar *name = NULL, *email = NULL;
			GtkAction *action;

			inet_addr = camel_internet_address_new ();
			camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);

			action = gtk_action_group_get_action (action_group, "mailto-copy-raw");
			gtk_action_set_visible (action,
				camel_internet_address_get (inet_addr, 0, &name, &email) &&
				name && *name && email && *email);

			g_object_unref (inet_addr);
			camel_url_free (curl);
		}
	}

	group_name = "image";
	visible = (cursor_image_src != NULL);
	action_group = e_web_view_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "selection";
	visible = can_copy;
	action_group = e_web_view_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "standard";
	visible = (uri == NULL);
	action_group = e_web_view_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "lockdown-printing";
	visible = (uri == NULL) && !web_view->priv->disable_printing;
	action_group = e_web_view_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);

	group_name = "lockdown-save-to-disk";
	visible = (uri == NULL) && !web_view->priv->disable_save_to_disk;
	action_group = e_web_view_get_action_group (web_view, group_name);
	gtk_action_group_set_visible (action_group, visible);
}

static void
web_view_submit_alert (EAlertSink *alert_sink,
                       EAlert *alert)
{
	EWebView *web_view;
	GtkWidget *dialog;
	GString *buffer;
	const gchar *icon_name = NULL;
	const gchar *primary_text;
	const gchar *secondary_text;
	gpointer parent;

	web_view = E_WEB_VIEW (alert_sink);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

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

	/* Primary text is required. */
	primary_text = e_alert_get_primary_text (alert);
	g_return_if_fail (primary_text != NULL);

	/* Secondary text is optional. */
	secondary_text = e_alert_get_secondary_text (alert);
	if (secondary_text == NULL)
		secondary_text = "";

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
		"<img src='gtk-stock://%s/?size=%d'/>"
		"</td>"
		"<td align='left' width='100%%'>"
		"<h3>%s</h3>"
		"%s"
		"</td>"
		"</tr>",
		icon_name,
		GTK_ICON_SIZE_DIALOG,
		primary_text,
		secondary_text);

	g_string_append (
		buffer,
		"</table>"
		"</td>"
		"</tr>"
		"</table>"
		"</body>"
		"</html>");

	e_web_view_load_string (web_view, buffer->str);

	g_string_free (buffer, TRUE);
}

static void
web_view_selectable_update_actions (ESelectable *selectable,
                                    EFocusTracker *focus_tracker,
                                    GdkAtom *clipboard_targets,
                                    gint n_clipboard_targets)
{
	WebKitWebView *web_view;
	GtkAction *action;
	gboolean sensitive;
	const gchar *tooltip;

	web_view = WEBKIT_WEB_VIEW (selectable);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = webkit_web_view_can_cut_clipboard (web_view);
	tooltip = _("Cut the selection");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = webkit_web_view_can_copy_clipboard (web_view);
	tooltip = _("Copy the selection");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = webkit_web_view_can_paste_clipboard (web_view);
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
web_view_selectable_cut_clipboard (ESelectable *selectable)
{
	e_web_view_cut_clipboard (E_WEB_VIEW (selectable));
}

static void
web_view_selectable_copy_clipboard (ESelectable *selectable)
{
	e_web_view_copy_clipboard (E_WEB_VIEW (selectable));
}

static void
web_view_selectable_paste_clipboard (ESelectable *selectable)
{
	e_web_view_paste_clipboard (E_WEB_VIEW (selectable));
}

static void
web_view_selectable_select_all (ESelectable *selectable)
{
	e_web_view_select_all (E_WEB_VIEW (selectable));
}

static void
e_web_view_test_change_and_update_fonts_cb (EWebView *web_view,
					    const gchar *key,
					    GSettings *settings)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (web_view->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (web_view->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (web_view->priv->old_settings, key);

		e_web_view_update_fonts (web_view);
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static gpointer
web_view_disable_webkit_3rd_party_plugins (gpointer unused)
{
	WebKitWebPluginDatabase *database;
	GSList *installed_plugins, *iterator;

	database = webkit_get_web_plugin_database ();

	if (!database)
		return NULL;

	installed_plugins = webkit_web_plugin_database_get_plugins (database);

	if (!installed_plugins)
		return NULL;

	for (iterator = installed_plugins; iterator; iterator = iterator->next)
		webkit_web_plugin_set_enabled (iterator->data, FALSE);

	webkit_web_plugin_database_plugins_list_free (installed_plugins);

	return NULL;
}

static void
web_view_toplevel_event_after_cb (GtkWidget *widget,
				  GdkEvent *event,
				  EWebView *web_view)
{
	if (event && event->type == GDK_MOTION_NOTIFY && web_view->priv->has_hover_link) {
		GdkEventMotion *motion_event = (GdkEventMotion *) event;

		if (gdk_event_get_window (event) != gtk_widget_get_window (GTK_WIDGET (web_view))) {
			GdkEventMotion fake_motion_event;
			gboolean result = FALSE;

			fake_motion_event = *motion_event;
			fake_motion_event.x = -1.0;
			fake_motion_event.y = -1.0;
			fake_motion_event.window = gtk_widget_get_window (GTK_WIDGET (web_view));

			/* Use a fake event instead of the call to unset the status message, because
			   WebKit caches which link it stays on and doesn't emit the signal when still
			   moving about the same link, thus this will unset the link also for the WebKit. */
			g_signal_emit_by_name (web_view, "motion-notify-event", &fake_motion_event, &result);

			web_view->priv->has_hover_link = FALSE;
		}
	}
}

static void
web_view_map (GtkWidget *widget)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (widget);

	g_signal_connect (toplevel, "event-after", G_CALLBACK (web_view_toplevel_event_after_cb), widget);

	GTK_WIDGET_CLASS (e_web_view_parent_class)->map (widget);
}

static void
web_view_unmap (GtkWidget *widget)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (widget);

	g_signal_handlers_disconnect_by_func (toplevel, G_CALLBACK (web_view_toplevel_event_after_cb), widget);

	GTK_WIDGET_CLASS (e_web_view_parent_class)->unmap (widget);
}

static void
e_web_view_class_init (EWebViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EWebViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = web_view_set_property;
	object_class->get_property = web_view_get_property;
	object_class->dispose = web_view_dispose;
	object_class->finalize = web_view_finalize;
	object_class->constructed = web_view_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->scroll_event = web_view_scroll_event;
	widget_class->drag_motion = web_view_drag_motion;
	widget_class->map = web_view_map;
	widget_class->unmap = web_view_unmap;

	class->create_plugin_widget = web_view_create_plugin_widget;
	class->hovering_over_link = web_view_hovering_over_link;
	class->link_clicked = web_view_link_clicked;
	class->load_string = web_view_load_string;
	class->load_uri = web_view_load_uri;
	class->redirect_uri = web_view_redirect_uri;
	class->suggest_filename = web_view_suggest_filename;
	class->popup_event = web_view_popup_event;
	class->stop_loading = web_view_stop_loading;
	class->update_actions = web_view_update_actions;

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
		PROP_CURSOR_IMAGE_SRC,
		g_param_spec_string (
			"cursor-image-src",
			"Image source uri at the mouse cursor",
			NULL,
			NULL,
			G_PARAM_READWRITE));

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
		PROP_OPEN_PROXY,
		g_param_spec_object (
			"open-proxy",
			"Open Proxy",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

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

	signals[NEW_ACTIVITY] = g_signal_new (
		"new-activity",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, new_activity),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, popup_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__STRING,
		G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

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

	/* return TRUE when a signal handler processed the mailto URI */
	signals[PROCESS_MAILTO] = g_signal_new (
		"process-mailto",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, process_mailto),
		NULL, NULL,
		e_marshal_BOOLEAN__STRING,
		G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

	webkit_set_cache_model (WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
	webkit_set_default_web_database_quota (0);
	webkit_application_cache_set_maximum_size (0);
}

static void
e_web_view_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = web_view_submit_alert;
}

static void
e_web_view_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = web_view_selectable_update_actions;
	iface->cut_clipboard = web_view_selectable_cut_clipboard;
	iface->copy_clipboard = web_view_selectable_copy_clipboard;
	iface->paste_clipboard = web_view_selectable_paste_clipboard;
	iface->select_all = web_view_selectable_select_all;
}

static void
e_web_view_init (EWebView *web_view)
{
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	EPopupAction *popup_action;
	GSettingsSchema *settings_schema;
	GSettings *settings;
	const gchar *domain = GETTEXT_PACKAGE;
	const gchar *id;
	gulong handler_id;
	GError *error = NULL;

	g_once (
		&disable_webkit_3rd_party_plugins_once,
		web_view_disable_webkit_3rd_party_plugins, NULL);

	web_view->priv = E_WEB_VIEW_GET_PRIVATE (web_view);

	web_view->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
	web_view->priv->zoom_hack_state = E_WEB_VIEW_ZOOM_HACK_STATE_NONE;

	/* XXX No WebKitWebView class method pointers to
	 *     override so we have to use signal handlers. */

	g_signal_connect (
		web_view, "create-plugin-widget",
		G_CALLBACK (web_view_create_plugin_widget_cb), NULL);

	g_signal_connect (
		web_view, "context-menu",
		G_CALLBACK (web_view_context_menu_cb), NULL);

	g_signal_connect (
		web_view, "hovering-over-link",
		G_CALLBACK (web_view_hovering_over_link_cb), NULL);

	g_signal_connect (
		web_view, "navigation-policy-decision-requested",
		G_CALLBACK (web_view_navigation_policy_decision_requested_cb),
		NULL);

	g_signal_connect (
		web_view, "new-window-policy-decision-requested",
		G_CALLBACK (web_view_navigation_policy_decision_requested_cb),
		NULL);

	g_signal_connect (
		web_view, "document-load-finished",
		G_CALLBACK (style_updated_cb), NULL);

	e_signal_connect_notify (
		web_view, "notify::load-status",
		G_CALLBACK (web_view_load_status_changed_cb), NULL);

	g_signal_connect (
		web_view, "style-updated",
		G_CALLBACK (style_updated_cb), NULL);

	g_signal_connect (
		web_view, "state-flags-changed",
		G_CALLBACK (style_updated_cb), NULL);

	ui_manager = gtk_ui_manager_new ();
	web_view->priv->ui_manager = ui_manager;

	g_signal_connect_swapped (
		ui_manager, "connect-proxy",
		G_CALLBACK (web_view_connect_proxy_cb), web_view);

	web_view_init_web_settings (WEBKIT_WEB_VIEW (web_view));

	e_web_view_install_request_handler (web_view, E_TYPE_FILE_REQUEST);
	e_web_view_install_request_handler (web_view, E_TYPE_STOCK_REQUEST);

	settings = e_util_ref_settings ("org.gnome.desktop.interface");
	web_view->priv->font_settings = g_object_ref (settings);
	handler_id = g_signal_connect_swapped (
		settings, "changed::font-name",
		G_CALLBACK (e_web_view_test_change_and_update_fonts_cb), web_view);
	web_view->priv->font_name_changed_handler_id = handler_id;
	handler_id = g_signal_connect_swapped (
		settings, "changed::monospace-font-name",
		G_CALLBACK (e_web_view_test_change_and_update_fonts_cb), web_view);
	web_view->priv->monospace_font_name_changed_handler_id = handler_id;
	g_object_unref (settings);

	/* This schema is optional.  Use if available. */
	id = "org.gnome.settings-daemon.plugins.xsettings";
	settings_schema = g_settings_schema_source_lookup (
		g_settings_schema_source_get_default (), id, FALSE);
	if (settings_schema != NULL) {
		settings = e_util_ref_settings (id);
		web_view->priv->aliasing_settings = g_object_ref (settings);
		handler_id = g_signal_connect_swapped (
			settings, "changed::antialiasing",
			G_CALLBACK (e_web_view_test_change_and_update_fonts_cb), web_view);
		web_view->priv->antialiasing_changed_handler_id = handler_id;
		g_object_unref (settings);
		g_settings_schema_unref (settings_schema);
	}

	e_web_view_update_fonts (web_view);

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

	e_binding_bind_property (
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

	e_binding_bind_property (
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

	e_binding_bind_property (
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

	e_web_view_clear (E_WEB_VIEW (web_view));
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

	webkit_web_view_load_html_string (
		WEBKIT_WEB_VIEW (web_view),
		"<html>"
		"<head></head>"
		"<body class=\"-e-web-view-background-color -e-web-view-text-color\"></body>"
		"</html>",
		NULL);
}

void
e_web_view_load_string (EWebView *web_view,
                        const gchar *string)
{
	EWebViewClass *class;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_if_fail (class->load_string != NULL);

	class->load_string (web_view, string);
}

void
e_web_view_load_uri (EWebView *web_view,
                     const gchar *uri)
{
	EWebViewClass *class;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_if_fail (class->load_uri != NULL);

	class->load_uri (web_view, uri);
}

/**
 * e_web_view_redirect_uri:
 * @web_view: an #EWebView
 * @uri: the requested URI
 *
 * Replaces @uri with a redirected URI as necessary, primarily for use
 * with custom #SoupRequest handlers.  Typically this function would be
 * called just prior to handing a request off to a #SoupSession, such as
 * from a #WebKitWebView #WebKitWebView::resource-request-starting signal
 * handler.
 *
 * A newly-allocated URI string is always returned, whether the @uri was
 * redirected or not.  Free the returned string with g_free().
 *
 * Returns: the redirected URI or a copy of @uri
 **/
gchar *
e_web_view_redirect_uri (EWebView *web_view,
                         const gchar *uri)
{
	EWebViewClass *class;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_val_if_fail (class->redirect_uri != NULL, NULL);

	return class->redirect_uri (web_view, uri);
}

/**
 * e_web_view_suggest_filename:
 * @web_view: an #EWebView
 * @uri: a URI string
 *
 * Attempts to derive a suggested filename from the @uri for use in a
 * "Save As" dialog.
 *
 * By default the suggested filename is the last path segment of the @uri
 * (unless @uri looks like a query), but subclasses can use other mechanisms
 * for custom URI schemes.  For example, "cid:" URIs in an email message may
 * refer to a MIME part with a suggested filename in its Content-Disposition
 * header.
 *
 * The returned string should be freed with g_free() when finished with it,
 * but callers should also be prepared for the function to return %NULL if
 * a filename cannot be determined.
 *
 * Returns: a suggested filename, or %NULL
 **/
gchar *
e_web_view_suggest_filename (EWebView *web_view,
                             const gchar *uri)
{
	EWebViewClass *class;
	gchar *filename;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_val_if_fail (class->suggest_filename != NULL, NULL);

	filename = class->suggest_filename (web_view, uri);

	if (filename != NULL)
		e_filename_make_safe (filename);

	return filename;
}

void
e_web_view_reload (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_reload (WEBKIT_WEB_VIEW (web_view));
}

gchar *
e_web_view_get_html (EWebView *web_view)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (web_view));
	element = webkit_dom_document_get_document_element (document);

	return webkit_dom_html_element_get_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (element));
}

gboolean
e_web_view_get_caret_mode (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return web_view->priv->caret_mode;
}

void
e_web_view_set_caret_mode (EWebView *web_view,
                           gboolean caret_mode)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->caret_mode == caret_mode)
		return;

	web_view->priv->caret_mode = caret_mode;

	g_object_notify (G_OBJECT (web_view), "caret-mode");
}

GtkTargetList *
e_web_view_get_copy_target_list (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return webkit_web_view_get_copy_target_list (
		WEBKIT_WEB_VIEW (web_view));
}

gboolean
e_web_view_get_disable_printing (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return web_view->priv->disable_printing;
}

void
e_web_view_set_disable_printing (EWebView *web_view,
                                 gboolean disable_printing)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->disable_printing == disable_printing)
		return;

	web_view->priv->disable_printing = disable_printing;

	g_object_notify (G_OBJECT (web_view), "disable-printing");
}

gboolean
e_web_view_get_disable_save_to_disk (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return web_view->priv->disable_save_to_disk;
}

void
e_web_view_set_disable_save_to_disk (EWebView *web_view,
                                     gboolean disable_save_to_disk)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->disable_save_to_disk == disable_save_to_disk)
		return;

	web_view->priv->disable_save_to_disk = disable_save_to_disk;

	g_object_notify (G_OBJECT (web_view), "disable-save-to-disk");
}

gboolean
e_web_view_get_editable (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return webkit_web_view_get_editable (WEBKIT_WEB_VIEW (web_view));
}

void
e_web_view_set_editable (EWebView *web_view,
                         gboolean editable)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (web_view), editable);
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

	if (g_strcmp0 (web_view->priv->selected_uri, selected_uri) == 0)
		return;

	g_free (web_view->priv->selected_uri);
	web_view->priv->selected_uri = g_strdup (selected_uri);

	g_object_notify (G_OBJECT (web_view), "selected-uri");
}

const gchar *
e_web_view_get_cursor_image_src (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->cursor_image_src;
}

void
e_web_view_set_cursor_image_src (EWebView *web_view,
                                 const gchar *src_uri)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (g_strcmp0 (web_view->priv->cursor_image_src, src_uri) == 0)
		return;

	g_free (web_view->priv->cursor_image_src);
	web_view->priv->cursor_image_src = g_strdup (src_uri);

	g_object_notify (G_OBJECT (web_view), "cursor-image-src");
}

GtkAction *
e_web_view_get_open_proxy (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->open_proxy;
}

void
e_web_view_set_open_proxy (EWebView *web_view,
                           GtkAction *open_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

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
e_web_view_get_paste_target_list (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return webkit_web_view_get_paste_target_list (
		WEBKIT_WEB_VIEW (web_view));
}

GtkAction *
e_web_view_get_print_proxy (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->print_proxy;
}

void
e_web_view_set_print_proxy (EWebView *web_view,
                            GtkAction *print_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

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
e_web_view_get_save_as_proxy (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->save_as_proxy;
}

void
e_web_view_set_save_as_proxy (EWebView *web_view,
                              GtkAction *save_as_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

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

void
e_web_view_add_highlight (EWebView *web_view,
                          const gchar *highlight)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (highlight && *highlight);

	g_queue_push_tail (
		&web_view->priv->highlights,
		g_strdup (highlight));

	webkit_web_view_mark_text_matches (
		WEBKIT_WEB_VIEW (web_view), highlight, FALSE, 0);

	webkit_web_view_set_highlight_text_matches (
		WEBKIT_WEB_VIEW (web_view), TRUE);
}

void
e_web_view_clear_highlights (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_unmark_text_matches (WEBKIT_WEB_VIEW (web_view));

	while (!g_queue_is_empty (&web_view->priv->highlights))
		g_free (g_queue_pop_head (&web_view->priv->highlights));
}

void
e_web_view_update_highlights (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	web_view_update_document_highlights (web_view);
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

void
e_web_view_copy_clipboard (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_copy_clipboard (WEBKIT_WEB_VIEW (web_view));
}

void
e_web_view_cut_clipboard (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_cut_clipboard (WEBKIT_WEB_VIEW (web_view));
}

gboolean
e_web_view_is_selection_active (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return webkit_web_view_has_selection (WEBKIT_WEB_VIEW (web_view));
}

void
e_web_view_paste_clipboard (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_paste_clipboard (WEBKIT_WEB_VIEW (web_view));
}

gboolean
e_web_view_scroll_forward (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	webkit_web_view_move_cursor (
		WEBKIT_WEB_VIEW (web_view), GTK_MOVEMENT_PAGES, 1);

	return TRUE;  /* XXX This means nothing. */
}

gboolean
e_web_view_scroll_backward (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	webkit_web_view_move_cursor (
		WEBKIT_WEB_VIEW (web_view), GTK_MOVEMENT_PAGES, -1);

	return TRUE;  /* XXX This means nothing. */
}

void
e_web_view_select_all (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_select_all (WEBKIT_WEB_VIEW (web_view));
}

void
e_web_view_unselect_all (EWebView *web_view)
{
#if 0  /* WEBKIT */
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	gtk_html_command (GTK_HTML (web_view), "unselect-all");
#endif
}

void
e_web_view_zoom_100 (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (web_view), 1.0);
}

void
e_web_view_zoom_in (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (web_view)) < 4.9999)
		webkit_web_view_zoom_in (WEBKIT_WEB_VIEW (web_view));
}

void
e_web_view_zoom_out (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (web_view)) > 0.7999)
		webkit_web_view_zoom_out (WEBKIT_WEB_VIEW (web_view));
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

	if (!gtk_menu_get_attach_widget (GTK_MENU (menu)))
		gtk_menu_attach_to_widget (GTK_MENU (menu),
					   GTK_WIDGET (web_view),
					   NULL);

	return menu;
}

void
e_web_view_show_popup_menu (EWebView *web_view)
{
	GtkWidget *menu;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_update_actions (web_view);

	menu = e_web_view_get_popup_menu (web_view);

	gtk_menu_popup (
		GTK_MENU (menu), NULL, NULL, NULL, NULL,
		0, gtk_get_current_event_time ());
}

/**
 * e_web_view_new_activity:
 * @web_view: an #EWebView
 *
 * Returns a new #EActivity for an #EWebView-related asynchronous operation,
 * and emits the #EWebView::new-activity signal.  By default the #EActivity
 * comes loaded with a #GCancellable and sets the @web_view itself as the
 * #EActivity:alert-sink (which means alerts are displayed directly in the
 * content area).  The signal emission allows the #EActivity to be further
 * customized and/or tracked by the application.
 *
 * Returns: an #EActivity
 **/
EActivity *
e_web_view_new_activity (EWebView *web_view)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	activity = e_activity_new ();

	alert_sink = E_ALERT_SINK (web_view);
	e_activity_set_alert_sink (activity, alert_sink);

	cancellable = g_cancellable_new ();
	e_activity_set_cancellable (activity, cancellable);
	g_object_unref (cancellable);

	g_signal_emit (web_view, signals[NEW_ACTIVITY], 0, activity);

	return activity;
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

static gboolean
element_is_in_pre_tag (WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	if (!node)
		return FALSE;

	while (element = webkit_dom_node_get_parent_element (node), element) {
		node = WEBKIT_DOM_NODE (element);

		if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (element)) {
			return TRUE;
		} else if (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element)) {
			break;
		}
	}

	return FALSE;
}

static gchar *
web_view_get_frame_selection_html (WebKitDOMElement *iframe)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMNodeList *frames;
	gulong ii, length;

	document = webkit_dom_html_iframe_element_get_content_document (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);
	if (dom_selection && (webkit_dom_dom_selection_get_range_count (dom_selection) > 0)) {
		WebKitDOMRange *range;
		WebKitDOMElement *element;
		WebKitDOMDocumentFragment *fragment;

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		if (range != NULL) {
			gchar *inner_html;
			WebKitDOMNode *node;

			fragment = webkit_dom_range_clone_contents (
				range, NULL);

			element = webkit_dom_document_create_element (
				document, "DIV", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element),
				WEBKIT_DOM_NODE (fragment), NULL);

			inner_html = webkit_dom_html_element_get_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (element));
			node = webkit_dom_range_get_start_container (range, NULL);
			if (element_is_in_pre_tag (node)) {
				gchar *tmp = inner_html;
				inner_html = g_strconcat ("<pre>", tmp, "</pre>", NULL);
				g_free (tmp);
			}

			g_object_unref (range);
			g_object_unref (dom_selection);
			return inner_html;
		}
	}

	g_object_unref (dom_selection);

	frames = webkit_dom_document_get_elements_by_tag_name (
		document, "IFRAME");
	length = webkit_dom_node_list_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		gchar *text;

		node = webkit_dom_node_list_item (frames, ii);

		text = web_view_get_frame_selection_html (
			WEBKIT_DOM_ELEMENT (node));

		g_object_unref (node);
		if (text != NULL) {
			g_object_unref (frames);
			return text;
		}
	}
	g_object_unref (frames);

	return NULL;
}

gchar *
e_web_view_get_selection_html (EWebView *web_view)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *frames;
	gulong ii, length;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	if (!webkit_web_view_has_selection (WEBKIT_WEB_VIEW (web_view)))
		return NULL;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (web_view));
	frames = webkit_dom_document_get_elements_by_tag_name (document, "IFRAME");
	length = webkit_dom_node_list_get_length (frames);

	for (ii = 0; ii < length; ii++) {
		gchar *text;
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (frames, ii);

		text = web_view_get_frame_selection_html (
			WEBKIT_DOM_ELEMENT (node));

		g_object_unref (node);
		if (text != NULL) {
			g_object_unref (frames);
			return text;
		}
	}
	g_object_unref (frames);

	return NULL;
}

const gchar *
e_web_view_get_citation_color_for_level (gint level)
{
	/* Block quote border colors are borrowed from Thunderbird. */
	static const gchar *citation_color_levels[5] = {
		"rgb(233,185,110)",	/* level 5 - Chocolate 1 */
		"rgb(114,159,207)",	/* level 1 - Sky Blue 1 */
		"rgb(173,127,168)",	/* level 2 - Plum 1 */
		"rgb(138,226,52)",	/* level 3 - Chameleon 1 */
		"rgb(252,175,62)",	/* level 4 - Orange 1 */
	};

	g_return_val_if_fail (level > 0, citation_color_levels[1]);

	return citation_color_levels[level % 5];
}

void
e_web_view_update_fonts (EWebView *web_view)
{
	EWebViewClass *class;
	GString *stylesheet;
	gchar *base64;
	gchar *aa = NULL;
	WebKitWebSettings *settings;
	PangoFontDescription *min_size, *ms, *vw;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GtkStyleContext *context;
	GdkColor *link = NULL;
	GdkColor *visited = NULL;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	ms = NULL;
	vw = NULL;

	class = E_WEB_VIEW_GET_CLASS (web_view);
	if (class->set_fonts != NULL)
		class->set_fonts (web_view, &ms, &vw);

	if (ms == NULL) {
		gchar *font;

		font = g_settings_get_string (
			web_view->priv->font_settings,
			"monospace-font-name");

		ms = pango_font_description_from_string (
			(font != NULL) ? font : "monospace 10");

		g_free (font);
	}

	if (vw == NULL) {
		gchar *font;

		font = g_settings_get_string (
			web_view->priv->font_settings,
			"font-name");

		vw = pango_font_description_from_string (
			(font != NULL) ? font : "serif 10");

		g_free (font);
	}

	if (pango_font_description_get_size (ms) < pango_font_description_get_size (vw)) {
		min_size = ms;
	} else {
		min_size = vw;
	}

	stylesheet = g_string_new ("");
	g_string_append_printf (
		stylesheet,
		"body {\n"
		"  font-family: '%s';\n"
		"  font-size: %dpt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n",
		pango_font_description_get_family (vw),
		pango_font_description_get_size (vw) / PANGO_SCALE,
		pango_font_description_get_weight (vw),
		styles[pango_font_description_get_style (vw)]);

	if (web_view->priv->aliasing_settings != NULL)
		aa = g_settings_get_string (
			web_view->priv->aliasing_settings, "antialiasing");

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
		"  margin: 0px;\n"
		"}",
		pango_font_description_get_family (ms),
		pango_font_description_get_size (ms) / PANGO_SCALE,
		pango_font_description_get_weight (ms),
		styles[pango_font_description_get_style (ms)]);

	context = gtk_widget_get_style_context (GTK_WIDGET (web_view));
	gtk_style_context_get_style (
		context,
		"link-color", &link,
		"visited-link-color", &visited,
		NULL);

	if (link == NULL) {
		#if GTK_CHECK_VERSION(3,12,0)
		GdkRGBA rgba;
		#endif

		link = g_slice_new0 (GdkColor);
		link->blue = G_MAXINT16;

		#if GTK_CHECK_VERSION(3,12,0)
		rgba.alpha = 1;
		rgba.red = 0;
		rgba.green = 0;
		rgba.blue = 1;

		gtk_style_context_get_color (context, GTK_STATE_FLAG_LINK, &rgba);

		e_rgba_to_color (&rgba, link);
		#endif
	}

	if (visited == NULL) {
		#if GTK_CHECK_VERSION(3,12,0)
		GdkRGBA rgba;
		#endif

		visited = g_slice_new0 (GdkColor);
		visited->red = G_MAXINT16;

		#if GTK_CHECK_VERSION(3,12,0)
		rgba.alpha = 1;
		rgba.red = 1;
		rgba.green = 0;
		rgba.blue = 0;

		gtk_style_context_get_color (context, GTK_STATE_FLAG_VISITED, &rgba);

		e_rgba_to_color (&rgba, visited);
		#endif
	}

	g_string_append_printf (
		stylesheet,
		"a {\n"
		"  color: #%06x;\n"
		"}\n"
		"a:visited {\n"
		"  color: #%06x;\n"
		"}\n",
		e_color_to_value (link),
		e_color_to_value (visited));

	gdk_color_free (link);
	gdk_color_free (visited);

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

	base64 = g_base64_encode ((guchar *) stylesheet->str, stylesheet->len);
	g_string_free (stylesheet, TRUE);

	stylesheet = g_string_new ("data:text/css;charset=utf-8;base64,");
	g_string_append (stylesheet, base64);
	g_free (base64);

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (web_view));
	g_object_set (
		G_OBJECT (settings),
		"default-font-size",
		pango_font_description_get_size (vw) / PANGO_SCALE,
		"default-font-family",
		pango_font_description_get_family (vw),
		"monospace-font-family",
		pango_font_description_get_family (ms),
		"default-monospace-font-size",
		pango_font_description_get_size (ms) / PANGO_SCALE,
		"minimum-font-size",
		pango_font_description_get_size (min_size) / PANGO_SCALE,
		"user-stylesheet-uri",
		stylesheet->str,
		NULL);

	g_string_free (stylesheet, TRUE);

	pango_font_description_free (ms);
	pango_font_description_free (vw);
}

/* Helper for e_web_view_cursor_image_copy() */
static void
web_view_cursor_image_copy_pixbuf_cb (GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	GdkPixbuf *pixbuf;
	GError *local_error = NULL;

	activity = E_ACTIVITY (user_data);
	alert_sink = e_activity_get_alert_sink (activity);

	pixbuf = gdk_pixbuf_new_from_stream_finish (result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((pixbuf != NULL) && (local_error == NULL)) ||
		((pixbuf == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"widgets:no-image-copy",
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		GtkClipboard *clipboard;

		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
		gtk_clipboard_set_image (clipboard, pixbuf);
		gtk_clipboard_store (clipboard);

		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	g_clear_object (&activity);
	g_clear_object (&pixbuf);
}

/* Helper for e_web_view_cursor_image_copy() */
static void
web_view_cursor_image_copy_request_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	GInputStream *input_stream;
	GError *local_error = NULL;

	activity = E_ACTIVITY (user_data);
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	input_stream = e_web_view_request_finish (
		E_WEB_VIEW (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((input_stream != NULL) && (local_error == NULL)) ||
		((input_stream == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"widgets:no-image-copy",
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		gdk_pixbuf_new_from_stream_async (
			input_stream, cancellable,
			web_view_cursor_image_copy_pixbuf_cb,
			g_object_ref (activity));
	}

	g_clear_object (&activity);
	g_clear_object (&input_stream);
}

/**
 * e_web_view_cursor_image_copy:
 * @web_view: an #EWebView
 *
 * Asynchronously copies the image under the cursor to the clipboard.
 *
 * This function triggers a #EWebView::new-activity signal emission so
 * the asynchronous operation can be tracked and/or cancelled.
 **/
void
e_web_view_cursor_image_copy (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->cursor_image_src != NULL) {
		EActivity *activity;
		GCancellable *cancellable;
		const gchar *text;

		activity = e_web_view_new_activity (web_view);
		cancellable = e_activity_get_cancellable (activity);

		text = _("Copying image to clipboard");
		e_activity_set_text (activity, text);

		e_web_view_request (
			web_view,
			web_view->priv->cursor_image_src,
			cancellable,
			web_view_cursor_image_copy_request_cb,
			g_object_ref (activity));

		g_object_unref (activity);
	}
}

/* Helper for e_web_view_cursor_image_save() */
static void
web_view_cursor_image_save_splice_cb (GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	g_output_stream_splice_finish (
		G_OUTPUT_STREAM (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"widgets:no-image-save",
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (async_context);
}

/* Helper for e_web_view_cursor_image_save() */
static void
web_view_cursor_image_save_replace_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	GFileOutputStream *output_stream;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	output_stream = g_file_replace_finish (
		G_FILE (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((output_stream != NULL) && (local_error == NULL)) ||
		((output_stream == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);
		async_context_free (async_context);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"widgets:no-image-save",
			local_error->message, NULL);
		g_error_free (local_error);
		async_context_free (async_context);

	} else {
		g_output_stream_splice_async (
			G_OUTPUT_STREAM (output_stream),
			async_context->input_stream,
			G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
			G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
			G_PRIORITY_DEFAULT,
			cancellable,
			web_view_cursor_image_save_splice_cb,
			async_context);
	}

	g_clear_object (&output_stream);
}

/* Helper for e_web_view_cursor_image_save() */
static void
web_view_cursor_image_save_request_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	GInputStream *input_stream;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	input_stream = e_web_view_request_finish (
		E_WEB_VIEW (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((input_stream != NULL) && (local_error == NULL)) ||
		((input_stream == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);
		async_context_free (async_context);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"widgets:no-image-save",
			local_error->message, NULL);
		g_error_free (local_error);
		async_context_free (async_context);

	} else {
		async_context->input_stream = g_object_ref (input_stream);

		/* Open an output stream to the destination file. */
		g_file_replace_async (
			async_context->destination,
			NULL, FALSE,
			G_FILE_CREATE_REPLACE_DESTINATION,
			G_PRIORITY_DEFAULT,
			cancellable,
			web_view_cursor_image_save_replace_cb,
			async_context);
	}

	g_clear_object (&input_stream);
}

/**
 * e_web_view_cursor_image_save:
 * @web_view: an #EWebView
 *
 * Prompts the user to choose a destination file and then asynchronously
 * saves the image under the cursor to the destination file.
 *
 * This function triggers a #EWebView::new-activity signal emission so
 * the asynchronous operation can be tracked and/or cancelled.
 **/
void
e_web_view_cursor_image_save (EWebView *web_view)
{
	GtkFileChooser *file_chooser;
	GFile *destination = NULL;
	GtkWidget *dialog;
	gchar *suggestion;
	gpointer toplevel;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->cursor_image_src == NULL)
		return;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	toplevel = gtk_widget_is_toplevel (toplevel) ? toplevel : NULL;

	dialog = gtk_file_chooser_dialog_new (
		_("Save Image"), toplevel,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Save"), GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	file_chooser = GTK_FILE_CHOOSER (dialog);
	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (file_chooser, TRUE);

	suggestion = e_web_view_suggest_filename (
		web_view, web_view->priv->cursor_image_src);

	if (suggestion != NULL) {
		gtk_file_chooser_set_current_name (file_chooser, suggestion);
		g_free (suggestion);
	}

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		destination = gtk_file_chooser_get_file (file_chooser);

	gtk_widget_destroy (dialog);

	if (destination != NULL) {
		EActivity *activity;
		GCancellable *cancellable;
		AsyncContext *async_context;
		gchar *text;
		gchar *uri;

		activity = e_web_view_new_activity (web_view);
		cancellable = e_activity_get_cancellable (activity);

		uri = g_file_get_uri (destination);
		text = g_strdup_printf (_("Saving image to '%s'"), uri);
		e_activity_set_text (activity, text);
		g_free (text);
		g_free (uri);

		async_context = g_slice_new0 (AsyncContext);
		async_context->activity = g_object_ref (activity);
		async_context->destination = g_object_ref (destination);

		e_web_view_request (
			web_view,
			web_view->priv->cursor_image_src,
			cancellable,
			web_view_cursor_image_save_request_cb,
			async_context);

		g_object_unref (activity);
		g_object_unref (destination);
	}
}

/* Helper for e_web_view_request() */
static void
web_view_request_send_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	GError *local_error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	async_context->input_stream = soup_request_send_finish (
		SOUP_REQUEST (source_object), result, &local_error);

	if (local_error != NULL)
		g_simple_async_result_take_error (simple, local_error);

	g_simple_async_result_complete (simple);
}

/**
 * e_web_view_request:
 * @web_view: an #EWebView
 * @uri: the URI to load
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously requests data at @uri by way of a #SoupRequest to WebKit's
 * default #SoupSession, incorporating both e_web_view_redirect_uri() and the
 * custom request handlers installed via e_web_view_install_request_handler().
 *
 * When the operation is finished, @callback will be called.  You can then
 * call e_web_view_request_finish() to get the result of the operation.
 **/
void
e_web_view_request (EWebView *web_view,
                    const gchar *uri,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
	SoupSession *session;
	SoupRequest *request;
	gchar *real_uri;
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (uri != NULL);

	session = webkit_get_default_session ();

	async_context = g_slice_new0 (AsyncContext);

	simple = g_simple_async_result_new (
		G_OBJECT (web_view), callback,
		user_data, e_web_view_request);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	real_uri = e_web_view_redirect_uri (web_view, uri);
	request = soup_session_request (session, real_uri, &local_error);
	g_free (real_uri);

	/* Sanity check. */
	g_return_if_fail (
		((request != NULL) && (local_error == NULL)) ||
		((request == NULL) && (local_error != NULL)));

	if (request != NULL) {
		soup_request_send_async (
			request, cancellable,
			web_view_request_send_cb,
			g_object_ref (simple));

		g_object_unref (request);

	} else {
		g_simple_async_result_take_error (simple, local_error);
		g_simple_async_result_complete_in_idle (simple);
	}

	g_object_unref (simple);
}

/**
 * e_web_view_request_finish:
 * @web_view: an #EWebView
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finishes the operation started with e_web_view_request().
 *
 * Unreference the returned #GInputStream with g_object_unref() when finished
 * with it.  If an error occurred, the function will set @error and return
 * %NULL.
 *
 * Returns: a #GInputStream, or %NULL
 **/
GInputStream *
e_web_view_request_finish (EWebView *web_view,
                           GAsyncResult *result,
                           GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (web_view), e_web_view_request), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	g_return_val_if_fail (async_context->input_stream != NULL, NULL);

	return g_object_ref (async_context->input_stream);
}

void
e_web_view_install_request_handler (EWebView *web_view,
                                    GType handler_type)
{
	SoupSession *session;

	session = webkit_get_default_session ();
	soup_session_add_feature_by_type (session, handler_type);
}

void
e_web_view_create_and_add_css_style_sheet (WebKitDOMDocument *document,
                                           const gchar *style_sheet_id)
{
	WebKitDOMElement *style_element;

	style_element = webkit_dom_document_get_element_by_id (document, style_sheet_id);

	if (!style_element) {
		WebKitDOMText *dom_text;
		WebKitDOMHTMLHeadElement *head;

		dom_text = webkit_dom_document_create_text_node (document, "");

		/* Create new <style> element */
		style_element = webkit_dom_document_create_element (document, "style", NULL);
		webkit_dom_element_set_id (
			WEBKIT_DOM_ELEMENT (style_element),
			style_sheet_id);
		webkit_dom_html_style_element_set_media (
			WEBKIT_DOM_HTML_STYLE_ELEMENT (style_element),
			"screen");
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (style_element),
			/* WebKit hack - we have to insert empty TextNode into style element */
			WEBKIT_DOM_NODE (dom_text),
			NULL);

		head = webkit_dom_document_get_head (document);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head),
			WEBKIT_DOM_NODE (style_element),
			NULL);

		g_object_unref (head);
		g_object_unref (dom_text);
		g_object_unref (style_element);
	}
}

static void
add_css_rule_into_style_sheet (WebKitDOMDocument *document,
                               const gchar *style_sheet_id,
                               const gchar *selector,
                               const gchar *style)
{
	WebKitDOMElement *style_element;
	WebKitDOMStyleSheet *sheet;
	WebKitDOMCSSRuleList *rules_list;
	gint length, ii, selector_length;
	gboolean removed = FALSE;

	g_return_if_fail (selector != NULL);

	selector_length = strlen (selector);
	style_element = webkit_dom_document_get_element_by_id (document, style_sheet_id);

	if (!style_element) {
		e_web_view_create_and_add_css_style_sheet (document, style_sheet_id);
		style_element = webkit_dom_document_get_element_by_id (document, style_sheet_id);
	}

	/* Get sheet that is associated with style element */
	sheet = webkit_dom_html_style_element_get_sheet (WEBKIT_DOM_HTML_STYLE_ELEMENT (style_element));

	rules_list = webkit_dom_css_style_sheet_get_css_rules (WEBKIT_DOM_CSS_STYLE_SHEET (sheet));
	length = webkit_dom_css_rule_list_get_length (rules_list);

	/* Check if rule exists */
	for (ii = 0; ii < length && !removed; ii++) {
		WebKitDOMCSSRule *rule;
		gchar *rule_text = NULL;

		rule = webkit_dom_css_rule_list_item (rules_list, ii);

		g_return_if_fail (WEBKIT_DOM_IS_CSS_RULE (rule));

		rule_text = webkit_dom_css_rule_get_css_text (rule);

		/* Find the start of the style => end of the selector */
		if (rule_text && selector && g_str_has_prefix (rule_text, selector) &&
		    rule_text[selector_length] == ' ' && rule_text[selector_length + 1] == '{') {
			/* If exists remove it */
			webkit_dom_css_style_sheet_remove_rule (
				WEBKIT_DOM_CSS_STYLE_SHEET (sheet),
				ii, NULL);
			length--;
			removed = TRUE;
		}

		g_free (rule_text);
		g_object_unref (rule);
	}

	g_object_unref (rules_list);

	/* Insert the rule at the end, so it will override previously inserted */
	webkit_dom_css_style_sheet_add_rule (
		WEBKIT_DOM_CSS_STYLE_SHEET (sheet), selector, style, length, NULL);

	g_object_unref (sheet);
	g_object_unref (style_element);
}

static void
add_css_rule_into_style_sheet_recursive (WebKitDOMDocument *document,
                                         const gchar *style_sheet_id,
                                         const gchar *selector,
                                         const gchar *style)
{
	WebKitDOMNodeList *frames;
	gint ii, length;

	/* Add rule to document */
	add_css_rule_into_style_sheet (
		document,
		style_sheet_id,
		selector,
		style);

	frames = webkit_dom_document_query_selector_all (document, "iframe", NULL);
	length = webkit_dom_node_list_get_length (frames);

	/* Add rules to every sub document */
	for (ii = 0; ii < length; ii++) {
		WebKitDOMDocument *iframe_document;
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (frames, ii);
		iframe_document = webkit_dom_html_iframe_element_get_content_document (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (node));

		add_css_rule_into_style_sheet_recursive (
			iframe_document,
			style_sheet_id,
			selector,
			style);
		g_object_unref (node);
	}
	g_object_unref (frames);
}

/**
 * e_web_view_add_css_rule_into_style_sheet:
 * @web_view: an #EWebView
 * @style_sheet_id: CSS style sheet's id
 * @selector: CSS selector
 * @style: style for given selector
 *
 * Insert new CSS rule (defined with @selector and @style) into CSS style sheet
 * with given @style_sheet_id. If style sheet doesn't exist, it's created.
 *
 * The rule is inserted to every DOM document that is in page. That means also
 * into DOM documents inside iframe elements.
 **/
void
e_web_view_add_css_rule_into_style_sheet (EWebView *view,
                                          const gchar *style_sheet_id,
                                          const gchar *selector,
                                          const gchar *style)
{
	g_return_if_fail (E_IS_WEB_VIEW (view));
	g_return_if_fail (style_sheet_id && *style_sheet_id);
	g_return_if_fail (selector && *selector);
	g_return_if_fail (style && *style);

	add_css_rule_into_style_sheet_recursive (
		webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view)),
		style_sheet_id,
		selector,
		style);
}
