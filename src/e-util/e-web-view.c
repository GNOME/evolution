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

#include "evolution-config.h"

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

#include "web-extensions/e-web-extension-names.h"

#include "e-web-view.h"

#define E_WEB_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEB_VIEW, EWebViewPrivate))

typedef struct _AsyncContext AsyncContext;

typedef struct _ElementClickedData {
	EWebViewElementClickedFunc callback;
	gpointer user_data;
} ElementClickedData;

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

	GDBusProxy *web_extension;
	guint web_extension_watch_name_id;

	WebKitFindController *find_controller;
	gulong found_text_handler_id;
	gulong failed_to_find_text_handler_id;

	gboolean has_hover_link;

	GSList *content_requests; /* EContentRequest * */

	GHashTable *element_clicked_cbs; /* gchar *element_class ~> GPtrArray {ElementClickedData} */
	guint web_extension_element_clicked_signal_id;

	guint32 clipboard_flags;
	guint web_extension_clipboard_flags_changed_signal_id;

	gboolean need_input;
	guint web_extension_need_input_changed_signal_id;

	GCancellable *load_cancellable;
};

struct _AsyncContext {
	EActivity *activity;
	GFile *destination;
	GInputStream *input_stream;
	EContentRequest *content_request;
	gchar *uri;
};

enum {
	PROP_0,
	PROP_CARET_MODE,
	PROP_CLIPBOARD_FLAGS,
	PROP_COPY_TARGET_LIST,
	PROP_CURSOR_IMAGE_SRC,
	PROP_DISABLE_PRINTING,
	PROP_DISABLE_SAVE_TO_DISK,
	PROP_NEED_INPUT,
	PROP_OPEN_PROXY,
	PROP_PASTE_TARGET_LIST,
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
	URI_REQUESTED,
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
async_context_free (gpointer ptr)
{
	AsyncContext *async_context = ptr;

	if (!async_context)
		return;

	g_clear_object (&async_context->activity);
	g_clear_object (&async_context->destination);
	g_clear_object (&async_context->input_stream);
	g_clear_object (&async_context->content_request);
	g_free (async_context->uri);

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
webkit_find_controller_found_text_cb (WebKitFindController *find_controller,
                                      guint match_count,
                                      EWebView *web_view)
{
}

static void
webkit_find_controller_failed_to_found_text_cb (WebKitFindController *find_controller,
                                                EWebView *web_view)
{
}

static void
web_view_set_find_controller (EWebView *web_view)
{
	WebKitFindController *find_controller;

	find_controller =
		webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (web_view));

	web_view->priv->found_text_handler_id = g_signal_connect (
		find_controller, "found-text",
		G_CALLBACK (webkit_find_controller_found_text_cb), web_view);

	web_view->priv->failed_to_find_text_handler_id = g_signal_connect (
		find_controller, "failed-to-find-text",
		G_CALLBACK (webkit_find_controller_failed_to_found_text_cb), web_view);

	web_view->priv->find_controller = find_controller;
}

static void
web_view_update_document_highlights (EWebView *web_view)
{
	GList *head, *link;

	head = g_queue_peek_head_link (&web_view->priv->highlights);

	for (link = head; link != NULL; link = g_list_next (link)) {
		webkit_find_controller_search (
			web_view->priv->find_controller,
			link->data,
			WEBKIT_FIND_OPTIONS_NONE,
			G_MAXUINT);
	}
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
                          WebKitContextMenu *context_menu,
                          GdkEvent *event,
                          WebKitHitTestResult *hit_test_result,
                          gpointer user_data)
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

	context = webkit_hit_test_result_get_context (hit_test_result);

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
		link_uri, event, &event_handled);

	g_free (link_uri);

	return event_handled;
}

static void
web_view_mouse_target_changed_cb (EWebView *web_view,
                                  WebKitHitTestResult *hit_test_result,
                                  guint modifiers,
                                  gpointer user_data)
{
	EWebViewClass *class;
	const gchar *title, *uri;

	title = webkit_hit_test_result_get_link_title (hit_test_result);
	uri = webkit_hit_test_result_get_link_uri (hit_test_result);

	web_view->priv->has_hover_link = uri && *uri;

	/* XXX WebKitWebView does not provide a class method for
	 *     this signal, so we do so we can override the default
	 *     behavior from subclasses for special URI types. */

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->hovering_over_link != NULL);

	class->hovering_over_link (web_view, title, uri);
}

static gboolean
web_view_decide_policy_cb (EWebView *web_view,
                           WebKitPolicyDecision *decision,
                           WebKitPolicyDecisionType type)
{
	EWebViewClass *class;
	WebKitNavigationPolicyDecision *navigation_decision;
	WebKitNavigationAction *navigation_action;
	WebKitNavigationType navigation_type;
	WebKitURIRequest *request;
	const gchar *uri, *view_uri;

	if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
	    type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
		return FALSE;

	navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
	navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
	navigation_type = webkit_navigation_action_get_navigation_type (navigation_action);

	if (navigation_type != WEBKIT_NAVIGATION_TYPE_LINK_CLICKED)
		return FALSE;

	request = webkit_navigation_action_get_request (navigation_action);
	uri = webkit_uri_request_get_uri (request);
	view_uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view));

	/* Allow navigation through fragments in page */
	if (uri && *uri && view_uri && *view_uri) {
		SoupURI *uri_link, *uri_view;

		uri_link = soup_uri_new (uri);
		uri_view = soup_uri_new (view_uri);
		if (uri_link && uri_view) {
			const gchar *tmp1, *tmp2;

			tmp1 = soup_uri_get_scheme (uri_link);
			tmp2 = soup_uri_get_scheme (uri_view);

			/* The scheme on both URIs should be the same */
			if (tmp1 && tmp2 && g_ascii_strcasecmp (tmp1, tmp2) != 0)
				goto free_uris;

			tmp1 = soup_uri_get_host (uri_link);
			tmp2 = soup_uri_get_host (uri_view);

			/* The host on both URIs should be the same */
			if (tmp1 && tmp2 && g_ascii_strcasecmp (tmp1, tmp2) != 0)
				goto free_uris;

			/* URI from link should have fragment set - could be empty */
			if (soup_uri_get_fragment (uri_link)) {
				soup_uri_free (uri_link);
				soup_uri_free (uri_view);
				webkit_policy_decision_use (decision);
				return TRUE;
			}
		}

 free_uris:
		if (uri_link)
			soup_uri_free (uri_link);
		if (uri_view)
			soup_uri_free (uri_view);
	}

	/* XXX WebKitWebView does not provide a class method for
	 *     this signal, so we do so we can override the default
	 *     behavior from subclasses for special URI types. */

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->link_clicked != NULL, FALSE);

	webkit_policy_decision_ignore (decision);

	class->link_clicked (web_view, uri);

	return TRUE;
}

static void
e_web_view_ensure_body_class (EWebView *web_view)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (!web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		web_extension,
		"EWebViewEnsureBodyClass",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)),
			"-e-web-view-background-color -e-web-view-text-color"),
		NULL);
}

static void
style_updated_cb (EWebView *web_view)
{
	GdkRGBA color;
	gchar *color_value;
	gchar *style;
	GtkStyleContext *style_context;

	style_context = gtk_widget_get_style_context (GTK_WIDGET (web_view));

	if (gtk_style_context_lookup_color (style_context, "theme_base_color", &color))
		color_value = g_strdup_printf ("#%06x", e_rgba_to_value (&color));
	else {
		color_value = g_strdup (E_UTILS_DEFAULT_THEME_BASE_COLOR);
		if (!gdk_rgba_parse (&color, color_value)) {
			color.red = 1.0;
			color.green = 1.0;
			color.blue = 1.0;
			color.alpha = 1.0;
		}
	}

	style = g_strconcat ("background-color: ", color_value, ";", NULL);

	webkit_web_view_set_background_color (WEBKIT_WEB_VIEW (web_view), &color);

	e_web_view_add_css_rule_into_style_sheet (
		web_view,
		"-e-web-view-style-sheet",
		".-e-web-view-background-color",
		style);

	g_free (color_value);
	g_free (style);

	if (gtk_style_context_lookup_color (style_context, "theme_fg_color", &color))
		color_value = g_strdup_printf ("#%06x", e_rgba_to_value (&color));
	else
		color_value = g_strdup (E_UTILS_DEFAULT_THEME_FG_COLOR);

	style = g_strconcat ("color: ", color_value, ";", NULL);

	e_web_view_add_css_rule_into_style_sheet (
		web_view,
		"-e-web-view-style-sheet",
		".-e-web-view-text-color",
		style);

	g_free (color_value);
	g_free (style);
}

static void
web_view_load_changed_cb (WebKitWebView *webkit_web_view,
                          WebKitLoadEvent load_event,
                          gpointer user_data)
{
	EWebView *web_view;

	web_view = E_WEB_VIEW (webkit_web_view);

	if (load_event == WEBKIT_LOAD_STARTED)
		g_hash_table_remove_all (web_view->priv->element_clicked_cbs);

	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	e_web_view_ensure_body_class (web_view);
	style_updated_cb (web_view);

	web_view_update_document_highlights (web_view);
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

static GObject*
web_view_constructor (GType type,
                      guint n_construct_properties,
                      GObjectConstructParam *construct_properties)
{
	GObjectClass* object_class;
	GParamSpec* param_spec;
	GObjectConstructParam *param = NULL;

	object_class = G_OBJECT_CLASS (g_type_class_ref(type));
	g_return_val_if_fail (object_class != NULL, NULL);

	if (construct_properties && n_construct_properties != 0) {
		param_spec = g_object_class_find_property (object_class, "settings");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, e_web_view_get_default_webkit_settings ());
		param_spec = g_object_class_find_property(object_class, "user-content-manager");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, webkit_user_content_manager_new ());
	}

	g_type_class_unref (object_class);

	return G_OBJECT_CLASS (e_web_view_parent_class)->constructor(type, n_construct_properties, construct_properties);
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

		case PROP_CLIPBOARD_FLAGS:
			e_web_view_set_clipboard_flags (
				E_WEB_VIEW (object),
				g_value_get_uint (value));
			return;

		case PROP_COPY_TARGET_LIST:
			/* This is a fake property. */
			g_warning ("%s: EWebView::copy-target-list not used", G_STRFUNC);
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

		case PROP_NEED_INPUT:
			e_web_view_set_need_input (
				E_WEB_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_OPEN_PROXY:
			e_web_view_set_open_proxy (
				E_WEB_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_PASTE_TARGET_LIST:
			/* This is a fake property. */
			g_warning ("%s: EWebView::paste-target-list not used", G_STRFUNC);
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

		case PROP_CLIPBOARD_FLAGS:
			g_value_set_uint (
				value, e_web_view_get_clipboard_flags (
				E_WEB_VIEW (object)));
			return;

		case PROP_COPY_TARGET_LIST:
			/* This is a fake property. */
			g_value_set_boxed (value, NULL);
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

		case PROP_NEED_INPUT:
			g_value_set_boolean (
				value, e_web_view_get_need_input (
				E_WEB_VIEW (object)));
			return;

		case PROP_OPEN_PROXY:
			g_value_set_object (
				value, e_web_view_get_open_proxy (
				E_WEB_VIEW (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			/* This is a fake property. */
			g_value_set_boxed (value, NULL);
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

	if (priv->load_cancellable) {
		g_cancellable_cancel (priv->load_cancellable);
		g_clear_object (&priv->load_cancellable);
	}

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

	if (priv->web_extension_watch_name_id > 0) {
		g_bus_unwatch_name (priv->web_extension_watch_name_id);
		priv->web_extension_watch_name_id = 0;
	}

	if (priv->found_text_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->find_controller,
			priv->found_text_handler_id);
		priv->found_text_handler_id = 0;
	}

	if (priv->failed_to_find_text_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->find_controller,
			priv->failed_to_find_text_handler_id);
		priv->failed_to_find_text_handler_id = 0;
	}

	if (priv->web_extension && priv->web_extension_clipboard_flags_changed_signal_id) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (priv->web_extension),
			priv->web_extension_clipboard_flags_changed_signal_id);
		priv->web_extension_clipboard_flags_changed_signal_id = 0;
	}

	if (priv->web_extension && priv->web_extension_need_input_changed_signal_id) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (priv->web_extension),
			priv->web_extension_need_input_changed_signal_id);
		priv->web_extension_need_input_changed_signal_id = 0;
	}

	if (priv->web_extension && priv->web_extension_element_clicked_signal_id) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (priv->web_extension),
			priv->web_extension_element_clicked_signal_id);
		priv->web_extension_element_clicked_signal_id = 0;
	}

	g_hash_table_remove_all (priv->element_clicked_cbs);

	g_slist_free_full (priv->content_requests, g_object_unref);
	priv->content_requests = NULL;

	g_clear_object (&priv->ui_manager);
	g_clear_object (&priv->open_proxy);
	g_clear_object (&priv->print_proxy);
	g_clear_object (&priv->save_as_proxy);
	g_clear_object (&priv->aliasing_settings);
	g_clear_object (&priv->font_settings);
	g_clear_object (&priv->web_extension);

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

	g_hash_table_destroy (priv->element_clicked_cbs);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_web_view_parent_class)->finalize (object);
}


static void
web_view_uri_request_done_cb (GObject *source_object,
			      GAsyncResult *result,
			      gpointer user_data)
{
	WebKitURISchemeRequest *request = user_data;
	GInputStream *stream = NULL;
	gint64 stream_length = -1;
	gchar *mime_type = NULL;
	GError *error = NULL;

	g_return_if_fail (E_IS_CONTENT_REQUEST (source_object));
	g_return_if_fail (WEBKIT_IS_URI_SCHEME_REQUEST (request));

	if (!e_content_request_process_finish (E_CONTENT_REQUEST (source_object),
		result, &stream, &stream_length, &mime_type, &error)) {
		webkit_uri_scheme_request_finish_error (request, error);
		g_clear_error (&error);
	} else {
		webkit_uri_scheme_request_finish (request, stream, stream_length, mime_type);

		g_clear_object (&stream);
		g_free (mime_type);
	}

	g_object_unref (request);
}

static void
web_view_process_uri_request_cb (WebKitURISchemeRequest *request,
				 gpointer user_data)
{
	EWebView *web_view = NULL;
	EContentRequest *content_request = user_data;
	const gchar *uri;
	gchar *redirect_to_uri = NULL;
	GObject *requester;

	g_return_if_fail (WEBKIT_IS_URI_SCHEME_REQUEST (request));
	g_return_if_fail (E_IS_CONTENT_REQUEST (content_request));

	uri = webkit_uri_scheme_request_get_uri (request);
	requester = G_OBJECT (webkit_uri_scheme_request_get_web_view (request));

	if (!requester) {
		GError *error;

		error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
		webkit_uri_scheme_request_finish_error (request, error);
		g_clear_error (&error);

		return;
	}

	g_return_if_fail (e_content_request_can_process_uri (content_request, uri));

	if (E_IS_WEB_VIEW (requester)) {
		/* Expects an empty string to abandon the request,
		   or NULL to keep the passed-in uri,
		   or a new uri to load instead. */
		g_signal_emit (requester, signals[URI_REQUESTED], 0, uri, &redirect_to_uri);

		if (redirect_to_uri && *redirect_to_uri) {
			uri = redirect_to_uri;
		} else if (redirect_to_uri) {
			GError *error;

			g_free (redirect_to_uri);

			error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");

			webkit_uri_scheme_request_finish_error (request, error);
			g_clear_error (&error);

			return;
		}

		web_view = E_WEB_VIEW (requester);
	}

	e_content_request_process (content_request, uri, requester, web_view ? web_view->priv->load_cancellable : NULL,
		web_view_uri_request_done_cb, g_object_ref (request));

	g_free (redirect_to_uri);
}

/* 'scheme' is like "file", not "file:" */
void
e_web_view_register_content_request_for_scheme (EWebView *web_view,
						const gchar *scheme,
						EContentRequest *content_request)
{
	WebKitWebContext *web_context;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (E_IS_CONTENT_REQUEST (content_request));
	g_return_if_fail (scheme != NULL);

	web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (web_view));

	webkit_web_context_register_uri_scheme (web_context, scheme, web_view_process_uri_request_cb,
		g_object_ref (content_request), g_object_unref);

	if (!g_slist_find (web_view->priv->content_requests, content_request)) {
		web_view->priv->content_requests = g_slist_prepend (
			web_view->priv->content_requests,
			g_object_ref (content_request));
	}
}

static void
web_view_initialize (WebKitWebView *web_view)
{
	WebKitWebContext *web_context;
	EContentRequest *content_request;
	const gchar *id = "org.gnome.settings-daemon.plugins.xsettings";
	GSettings *settings = NULL, *font_settings;
	GSettingsSchema *settings_schema;

	web_context = webkit_web_view_get_context (web_view);

	webkit_web_context_set_cache_model (web_context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

	content_request = e_file_request_new ();
	e_web_view_register_content_request_for_scheme (E_WEB_VIEW (web_view), "evo-file", content_request);
	g_object_unref (content_request);

	content_request = e_stock_request_new ();
	e_web_view_register_content_request_for_scheme (E_WEB_VIEW (web_view), "gtk-stock", content_request);
	g_object_unref (content_request);

	/* Optional schema */
	settings_schema = g_settings_schema_source_lookup (
		g_settings_schema_source_get_default (), id, FALSE);

	if (settings_schema)
		settings = e_util_ref_settings (id);

	font_settings = e_util_ref_settings ("org.gnome.desktop.interface");
	e_web_view_update_fonts_settings (
		font_settings, settings, NULL, NULL, GTK_WIDGET (web_view));

	g_object_unref (font_settings);
	if (settings)
		g_object_unref (settings);
}


static void
web_view_constructed (GObject *object)
{
	WebKitSettings *web_settings;
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

	web_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (object));

	g_object_set (
		G_OBJECT (web_settings),
		"default-charset", "UTF-8",
		NULL);

	e_binding_bind_property (
		web_settings, "enable-caret-browsing",
		E_WEB_VIEW (object), "caret-mode",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	web_view_initialize (WEBKIT_WEB_VIEW (object));

	web_view_set_find_controller (E_WEB_VIEW (object));
}

static void
e_web_view_replace_load_cancellable (EWebView *web_view,
				     gboolean create_new)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->load_cancellable) {
		g_cancellable_cancel (web_view->priv->load_cancellable);
		g_clear_object (&web_view->priv->load_cancellable);
	}

	if (create_new)
		web_view->priv->load_cancellable = g_cancellable_new ();
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

	return GTK_WIDGET_CLASS (e_web_view_parent_class)->
		scroll_event (widget, event);
}

static gboolean
web_view_drag_motion (GtkWidget *widget,
                      GdkDragContext *context,
                      gint x,
                      gint y,
                      guint time_)
{
	/* Made this way to not pretend that this is a drop zone,
	   when only FALSE had been returned, even it is supposed
	   to be enough. Remove web_view_drag_leave() once fixed. */
	gdk_drag_status (context, 0, time_);

	return TRUE;
}

static gboolean
web_view_drag_drop (GtkWidget *widget,
		    GdkDragContext *context,
		    gint x,
		    gint y,
		    guint time_)
{
	/* Defined to avoid crash in WebKit */
	return FALSE;
}

static void
web_view_drag_leave (GtkWidget *widget,
		     GdkDragContext *context,
		     guint time_)
{
	/* Defined to avoid crash in WebKit, when the web_view_drag_motion()
	   uses the other way around. */
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
	if (!string || !*string) {
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (web_view), "", "evo-file://");
	} else {
		GBytes *bytes;

		bytes = g_bytes_new (string, strlen (string));
		webkit_web_view_load_bytes (WEBKIT_WEB_VIEW (web_view), bytes, NULL, NULL, "evo-file://");
		g_bytes_unref (bytes);
	}
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
                      const gchar *uri,
		      GdkEvent *event)
{
	e_web_view_set_selected_uri (web_view, uri);
	e_web_view_show_popup_menu (web_view, event);

	return TRUE;
}

static void
web_view_stop_loading (EWebView *web_view)
{
	e_web_view_replace_load_cancellable (web_view, FALSE);
	webkit_web_view_stop_loading (WEBKIT_WEB_VIEW (web_view));
}

static void
web_view_register_element_clicked_hfunc (gpointer key,
					 gpointer value,
					 gpointer user_data)
{
	const gchar *elem_class = key;
	EWebView *web_view = user_data;

	g_return_if_fail (elem_class && *elem_class);
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (!web_view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		web_view->priv->web_extension,
		"RegisterElementClicked",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)),
			elem_class),
		NULL);
}

static void
web_view_need_input_changed_signal_cb (GDBusConnection *connection,
				       const gchar *sender_name,
				       const gchar *object_path,
				       const gchar *interface_name,
				       const gchar *signal_name,
				       GVariant *parameters,
				       gpointer user_data)
{
	EWebView *web_view = user_data;
	guint64 page_id = 0;
	gboolean need_input = FALSE;

	if (g_strcmp0 (signal_name, "NeedInputChanged") != 0)
		return;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (!parameters)
		return;

	g_variant_get (parameters, "(tb)", &page_id, &need_input);

	if (page_id == webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)))
		e_web_view_set_need_input (web_view, need_input);
}

static void
web_view_clipboard_flags_changed_signal_cb (GDBusConnection *connection,
					    const gchar *sender_name,
					    const gchar *object_path,
					    const gchar *interface_name,
					    const gchar *signal_name,
					    GVariant *parameters,
					    gpointer user_data)
{
	EWebView *web_view = user_data;
	guint64 page_id = 0;
	guint32 clipboard_flags = 0;

	if (g_strcmp0 (signal_name, "ClipboardFlagsChanged") != 0)
		return;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (!parameters)
		return;

	g_variant_get (parameters, "(tu)", &page_id, &clipboard_flags);

	if (page_id == webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)))
		e_web_view_set_clipboard_flags (web_view, clipboard_flags);
}

static void
web_view_element_clicked_signal_cb (GDBusConnection *connection,
				    const gchar *sender_name,
				    const gchar *object_path,
				    const gchar *interface_name,
				    const gchar *signal_name,
				    GVariant *parameters,
				    gpointer user_data)
{
	EWebView *web_view = user_data;
	const gchar *elem_class = NULL, *elem_value = NULL;
	GtkAllocation elem_position;
	guint64 page_id = 0;
	gint position_left = 0, position_top = 0, position_width = 0, position_height = 0;
	GPtrArray *listeners;

	if (g_strcmp0 (signal_name, "ElementClicked") != 0)
		return;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (!parameters)
		return;

	g_variant_get (parameters, "(t&s&siiii)", &page_id, &elem_class, &elem_value, &position_left, &position_top, &position_width, &position_height);

	if (!elem_class || !*elem_class || page_id != webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)))
		return;

	elem_position.x = position_left;
	elem_position.y = position_top;
	elem_position.width = position_width;
	elem_position.height = position_height;

	listeners = g_hash_table_lookup (web_view->priv->element_clicked_cbs, elem_class);
	if (listeners) {
		guint ii;

		for (ii = 0; ii <listeners->len; ii++) {
			ElementClickedData *ecd = g_ptr_array_index (listeners, ii);

			if (ecd && ecd->callback)
				ecd->callback (web_view, elem_class, elem_value, &elem_position, ecd->user_data);
		}
	}
}

static void
web_extension_proxy_created_cb (GDBusProxy *proxy,
                                GAsyncResult *result,
                                GWeakRef *web_view_ref)
{
	EWebView *web_view;
	GError *error = NULL;

	g_return_if_fail (web_view_ref != NULL);

	web_view = g_weak_ref_get (web_view_ref);

	if (!web_view) {
		e_weak_ref_free (web_view_ref);
		return;
	}

	web_view->priv->web_extension = g_dbus_proxy_new_finish (result, &error);
	if (!web_view->priv->web_extension) {
		g_warning ("Error creating web extension proxy: %s\n", error->message);
		g_error_free (error);
	} else {
		web_view->priv->web_extension_clipboard_flags_changed_signal_id =
			g_dbus_connection_signal_subscribe (
				g_dbus_proxy_get_connection (web_view->priv->web_extension),
				g_dbus_proxy_get_name (web_view->priv->web_extension),
				E_WEB_EXTENSION_INTERFACE,
				"ClipboardFlagsChanged",
				E_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				web_view_clipboard_flags_changed_signal_cb,
				web_view,
				NULL);

		web_view->priv->web_extension_need_input_changed_signal_id =
			g_dbus_connection_signal_subscribe (
				g_dbus_proxy_get_connection (web_view->priv->web_extension),
				g_dbus_proxy_get_name (web_view->priv->web_extension),
				E_WEB_EXTENSION_INTERFACE,
				"NeedInputChanged",
				E_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				web_view_need_input_changed_signal_cb,
				web_view,
				NULL);

		web_view->priv->web_extension_element_clicked_signal_id =
			g_dbus_connection_signal_subscribe (
				g_dbus_proxy_get_connection (web_view->priv->web_extension),
				g_dbus_proxy_get_name (web_view->priv->web_extension),
				E_WEB_EXTENSION_INTERFACE,
				"ElementClicked",
				E_WEB_EXTENSION_OBJECT_PATH,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				web_view_element_clicked_signal_cb,
				web_view,
				NULL);

		g_hash_table_foreach (web_view->priv->element_clicked_cbs, web_view_register_element_clicked_hfunc, web_view);

		e_web_view_ensure_body_class (web_view);
		style_updated_cb (web_view);
	}

	g_clear_object (&web_view);
	e_weak_ref_free (web_view_ref);
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           GWeakRef *web_view_ref)
{
	EWebView *web_view;

	g_return_if_fail (web_view_ref != NULL);

	web_view = g_weak_ref_get (web_view_ref);

	if (!web_view)
		return;

	g_dbus_proxy_new (
		connection,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL,
		name,
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
		NULL,
		(GAsyncReadyCallback) web_extension_proxy_created_cb,
		e_weak_ref_new (web_view));

	g_clear_object (&web_view);
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           GWeakRef *web_view_ref)
{
	EWebView *web_view;

	g_return_if_fail (web_view_ref != NULL);

	web_view = g_weak_ref_get (web_view_ref);

	if (!web_view)
		return;

	g_clear_object (&web_view->priv->web_extension);
	g_clear_object (&web_view);
}

static void
web_view_watch_web_extension (EWebView *web_view)
{
	web_view->priv->web_extension_watch_name_id =
		g_bus_watch_name (
			G_BUS_TYPE_SESSION,
			E_WEB_EXTENSION_SERVICE_NAME,
			G_BUS_NAME_WATCHER_FLAGS_NONE,
			(GBusNameAppearedCallback) web_extension_appeared_cb,
			(GBusNameVanishedCallback) web_extension_vanished_cb,
			e_weak_ref_new (web_view),
			(GDestroyNotify) e_weak_ref_free);
}

GDBusProxy *
e_web_view_get_web_extension_proxy (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->web_extension;
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

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	uri = e_web_view_get_selected_uri (web_view);
	can_copy = (e_web_view_get_clipboard_flags (web_view) & E_CLIPBOARD_CAN_COPY) != 0;
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
	gint icon_width, icon_height;
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

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &icon_width, &icon_height)) {
		icon_width = 48;
		icon_height = 48;
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
		"<img src='gtk-stock://%s/?size=%d' width=\"%dpx\" height=\"%dpx\"/>"
		"</td>"
		"<td align='left' width='100%%'>"
		"<h3>%s</h3>"
		"%s"
		"</td>"
		"</tr>",
		icon_name,
		GTK_ICON_SIZE_DIALOG,
		icon_width,
		icon_height,
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
web_view_can_execute_editing_command_cb (WebKitWebView *webkit_web_view,
                                         GAsyncResult *result,
                                         GtkAction *action)
{
	gboolean can_do_command;

	can_do_command = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	gtk_action_set_sensitive (action, can_do_command);
	g_object_unref (action);
}

static void
web_view_selectable_update_actions (ESelectable *selectable,
                                    EFocusTracker *focus_tracker,
                                    GdkAtom *clipboard_targets,
                                    gint n_clipboard_targets)
{
	EWebView *web_view;
	GtkAction *action;
	gboolean can_copy = FALSE;

	web_view = E_WEB_VIEW (selectable);

	can_copy = (e_web_view_get_clipboard_flags (web_view) & E_CLIPBOARD_CAN_COPY) != 0;

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	gtk_action_set_sensitive (action, can_copy);
	gtk_action_set_tooltip (action, _("Copy the selection"));

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_CUT,
		NULL, /* cancellable */
		(GAsyncReadyCallback) web_view_can_execute_editing_command_cb,
		g_object_ref (action));
	gtk_action_set_tooltip (action, _("Cut the selection"));

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_PASTE,
		NULL, /* cancellable */
		(GAsyncReadyCallback) web_view_can_execute_editing_command_cb,
		g_object_ref (action));
	gtk_action_set_tooltip (action, _("Paste the clipboard"));

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_set_tooltip (action, _("Select all text and images"));
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
	object_class->constructor = web_view_constructor;
	object_class->set_property = web_view_set_property;
	object_class->get_property = web_view_get_property;
	object_class->dispose = web_view_dispose;
	object_class->finalize = web_view_finalize;
	object_class->constructed = web_view_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->scroll_event = web_view_scroll_event;
	widget_class->drag_motion = web_view_drag_motion;
	widget_class->drag_drop = web_view_drag_drop;
	widget_class->drag_leave = web_view_drag_leave;
	widget_class->map = web_view_map;
	widget_class->unmap = web_view_unmap;

	class->hovering_over_link = web_view_hovering_over_link;
	class->link_clicked = web_view_link_clicked;
	class->load_string = web_view_load_string;
	class->load_uri = web_view_load_uri;
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
		PROP_CLIPBOARD_FLAGS,
		g_param_spec_uint (
			"clipboard-flags",
			"Clipboard Flags",
			NULL,
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/* Inherited from ESelectableInterface; just a fake property here */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	/* Inherited from ESelectableInterface; just a fake property here */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

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
		PROP_NEED_INPUT,
		g_param_spec_boolean (
			"need-input",
			"Need Input",
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
		NULL,
		G_TYPE_BOOLEAN, 2, G_TYPE_STRING, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

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

	/* Expects an empty string to abandon the request,
	   or NULL to keep the passed-in uri,
	   or a new uri to load instead. */
	signals[URI_REQUESTED] = g_signal_new (
		"uri-requested",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, uri_requested),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_POINTER);
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
initialize_web_extensions_cb (WebKitWebContext *web_context)
{
	/* Set the web extensions dir before the process is launched */
	webkit_web_context_set_web_extensions_directory (
		web_context, EVOLUTION_WEB_EXTENSIONS_DIR);
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

	web_view->priv = E_WEB_VIEW_GET_PRIVATE (web_view);

	web_view->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	g_signal_connect (
		web_view, "context-menu",
		G_CALLBACK (web_view_context_menu_cb), NULL);

	g_signal_connect (
		web_view, "mouse-target-changed",
		G_CALLBACK (web_view_mouse_target_changed_cb), NULL);

	g_signal_connect (
		web_view, "decide-policy",
		G_CALLBACK (web_view_decide_policy_cb),
		NULL);

	g_signal_connect (
		webkit_web_context_get_default (), "initialize-web-extensions",
		G_CALLBACK (initialize_web_extensions_cb), NULL);

	g_signal_connect (
		web_view, "load-changed",
		G_CALLBACK (web_view_load_changed_cb), NULL);

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

	web_view_watch_web_extension (web_view);

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

	web_view->priv->element_clicked_cbs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);

	web_view->priv->load_cancellable = NULL;
}

GtkWidget *
e_web_view_new (void)
{
	return g_object_new (
		E_TYPE_WEB_VIEW,
		NULL);
}

void
e_web_view_clear (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_replace_load_cancellable (web_view, FALSE);

	e_web_view_load_string (web_view,
		"<html>"
		"<head></head>"
		"<body class=\"-e-web-view-background-color -e-web-view-text-color\"></body>"
		"</html>");
}

void
e_web_view_load_string (EWebView *web_view,
                        const gchar *string)
{
	EWebViewClass *class;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->load_string != NULL);

	e_web_view_replace_load_cancellable (web_view, TRUE);

	class->load_string (web_view, string);
}

void
e_web_view_load_uri (EWebView *web_view,
                     const gchar *uri)
{
	EWebViewClass *class;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->load_uri != NULL);

	e_web_view_replace_load_cancellable (web_view, TRUE);

	class->load_uri (web_view, uri);
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
	g_return_val_if_fail (class != NULL, NULL);
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

	e_web_view_replace_load_cancellable (web_view, TRUE);

	webkit_web_view_reload (WEBKIT_WEB_VIEW (web_view));
}

static void
get_document_content_html_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	GDBusProxy *web_extension;
	GTask *task = user_data;
	GVariant *result_variant;
	gchar *html_content = NULL;
	GError *error = NULL;

	g_return_if_fail (G_IS_DBUS_PROXY (source_object));
	g_return_if_fail (G_IS_TASK (task));

	web_extension = G_DBUS_PROXY (source_object);

	result_variant = g_dbus_proxy_call_finish (web_extension, result, &error);
	if (result_variant)
		g_variant_get (result_variant, "(s)", &html_content);
	g_variant_unref (result_variant);

	g_task_return_pointer (task, html_content, g_free);
	g_object_unref (task);

	if (error)
		g_dbus_error_strip_remote_error (error);

	e_util_claim_dbus_proxy_call_error (web_extension, "GetDocumentContentHTML", error);
	g_clear_error (&error);
}

void
e_web_view_get_content_html (EWebView *web_view,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	GDBusProxy *web_extension;
	GTask *task;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	task = g_task_new (web_view, cancellable, callback, user_data);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (web_extension) {
		g_dbus_proxy_call (
			web_extension,
			"GetDocumentContentHTML",
			g_variant_new (
				"(t)",
				webkit_web_view_get_page_id (
					WEBKIT_WEB_VIEW (web_view))),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			cancellable,
			get_document_content_html_cb,
			g_object_ref (task));
	} else
		g_task_return_pointer (task, NULL, NULL);
}

gchar *
e_web_view_get_content_html_finish (EWebView *web_view,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);
	g_return_val_if_fail (g_task_is_valid (result, web_view), FALSE);

	return g_task_propagate_pointer (G_TASK (result), error);
}

gchar *
e_web_view_get_content_html_sync (EWebView *web_view,
                                  GCancellable *cancellable,
                                  GError **error)
{
	GDBusProxy *web_extension;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (web_extension) {
		GVariant *result;

		result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_full (
				web_extension,
				"GetDocumentContentHTML",
				g_variant_new (
					"(t)",
					webkit_web_view_get_page_id (
						WEBKIT_WEB_VIEW (web_view))),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				cancellable,
				error);

		if (result) {
			gchar *html_content = NULL;

			g_variant_get (result, "(s)", &html_content);
			g_variant_unref (result);

			return html_content;
		}
	}

	return NULL;
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

	return NULL;
	/* FIXME WK2 */
	/*return webkit_web_view_get_copy_target_list (
		WEBKIT_WEB_VIEW (web_view));*/
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

	return webkit_web_view_is_editable (WEBKIT_WEB_VIEW (web_view));
}

void
e_web_view_set_editable (EWebView *web_view,
                         gboolean editable)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (web_view), editable);
}

guint32
e_web_view_get_clipboard_flags (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), 0);

	return web_view->priv->clipboard_flags;
}

void
e_web_view_set_clipboard_flags (EWebView *web_view,
				guint32 clipboard_flags)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->clipboard_flags == clipboard_flags)
		return;

	web_view->priv->clipboard_flags = clipboard_flags;

	g_object_notify (G_OBJECT (web_view), "clipboard-flags");
}

gboolean
e_web_view_get_need_input (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return web_view->priv->need_input;
}

void
e_web_view_set_need_input (EWebView *web_view,
			   gboolean need_input)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if ((!web_view->priv->need_input) == (!need_input))
		return;

	web_view->priv->need_input = need_input;

	g_object_notify (G_OBJECT (web_view), "need-input");
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

	/* FIXME WK2
	return webkit_web_view_get_paste_target_list (
		WEBKIT_WEB_VIEW (web_view)); */
	return NULL;
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

	webkit_find_controller_search (
		web_view->priv->find_controller,
		highlight,
		WEBKIT_FIND_OPTIONS_NONE,
		G_MAXUINT);
}

void
e_web_view_clear_highlights (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_find_controller_search_finish (web_view->priv->find_controller);

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

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view), WEBKIT_EDITING_COMMAND_COPY);
}

void
e_web_view_cut_clipboard (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view), WEBKIT_EDITING_COMMAND_CUT);
}

gboolean
e_web_view_is_selection_active (EWebView *web_view)
{
	GDBusProxy *web_extension;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (web_extension) {
		GVariant *result;

		result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
				web_extension,
				"DocumentHasSelection",
				g_variant_new (
					"(t)",
					webkit_web_view_get_page_id (
						WEBKIT_WEB_VIEW (web_view))),
				NULL);

		if (result) {
			gboolean value = FALSE;

			g_variant_get (result, "(b)", &value);
			g_variant_unref (result);
			return value;
		}
	}

	return FALSE;
}

void
e_web_view_paste_clipboard (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view), WEBKIT_EDITING_COMMAND_PASTE);
}

gboolean
e_web_view_scroll_forward (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);
/* FIXME WK2
	webkit_web_view_move_cursor (
		WEBKIT_WEB_VIEW (web_view), GTK_MOVEMENT_PAGES, 1);
*/
	return TRUE;  /* XXX This means nothing. */
}

gboolean
e_web_view_scroll_backward (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);
/* FIXME WK2
	webkit_web_view_move_cursor (
		WEBKIT_WEB_VIEW (web_view), GTK_MOVEMENT_PAGES, -1);
*/
	return TRUE;  /* XXX This means nothing. */
}

void
e_web_view_select_all (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view), WEBKIT_EDITING_COMMAND_SELECT_ALL);
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
	gdouble zoom_level;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	/* There is no webkit_web_view_zoom_in function in WK2, so emulate it */
	zoom_level = webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (web_view));
	/* zoom-step in WK1 was 0.1 */
	zoom_level += 0.1;
	if (zoom_level < 4.9999)
		webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (web_view), zoom_level);
}

void
e_web_view_zoom_out (EWebView *web_view)
{
	gdouble zoom_level;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	/* There is no webkit_web_view_zoom_out function in WK2, so emulate it */
	zoom_level = webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (web_view));
	/* zoom-step in WK1 was 0.1 */
	zoom_level -= 0.1;
	if (zoom_level > 0.7999)
		webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (web_view), zoom_level);
}

GtkUIManager *
e_web_view_get_ui_manager (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->ui_manager;
}

static void
e_web_view_popup_menu_deactivate_cb (GtkMenu *popup_menu,
				     GtkWidget *web_view)
{
	g_return_if_fail (GTK_IS_MENU (popup_menu));

	g_signal_handlers_disconnect_by_func (popup_menu, e_web_view_popup_menu_deactivate_cb, web_view);
	gtk_menu_detach (popup_menu);
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

	g_warn_if_fail (!gtk_menu_get_attach_widget (GTK_MENU (menu)));

	gtk_menu_attach_to_widget (GTK_MENU (menu),
				   GTK_WIDGET (web_view),
				   NULL);

	g_signal_connect (
		menu, "deactivate",
		G_CALLBACK (e_web_view_popup_menu_deactivate_cb), web_view);

	return menu;
}

void
e_web_view_show_popup_menu (EWebView *web_view,
			    GdkEvent *event)
{
	GtkWidget *menu;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_update_actions (web_view);

	menu = e_web_view_get_popup_menu (web_view);

	gtk_menu_popup_at_pointer (GTK_MENU (menu), event);
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

static void
get_selection_content_html_cb (GObject *source_object,
			       GAsyncResult *result,
			       gpointer user_data)
{
	GDBusProxy *web_extension;
	GTask *task = user_data;
	GVariant *result_variant;
	gchar *html_content = NULL;
	GError *error = NULL;

	g_return_if_fail (G_IS_DBUS_PROXY (source_object));
	g_return_if_fail (G_IS_TASK (task));

	web_extension = G_DBUS_PROXY (source_object);

	result_variant = g_dbus_proxy_call_finish (web_extension, result, &error);
	if (result_variant)
		g_variant_get (result_variant, "(s)", &html_content);
	g_variant_unref (result_variant);

	g_task_return_pointer (task, html_content, g_free);
	g_object_unref (task);

	if (error)
		g_dbus_error_strip_remote_error (error);

	e_util_claim_dbus_proxy_call_error (web_extension, "GetSelectionContentHTML", error);
	g_clear_error (&error);
}

void
e_web_view_get_selection_content_html (EWebView *web_view,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
	GDBusProxy *web_extension;
	GTask *task;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	task = g_task_new (web_view, cancellable, callback, user_data);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (web_extension) {
		g_dbus_proxy_call (
			web_extension,
			"GetSelectionContentHTML",
			g_variant_new (
				"(t)",
				webkit_web_view_get_page_id (
					WEBKIT_WEB_VIEW (web_view))),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			cancellable,
			get_selection_content_html_cb,
			g_object_ref (task));
	} else
		g_task_return_pointer (task, NULL, NULL);
}

gchar *
e_web_view_get_selection_content_html_finish (EWebView *web_view,
                                              GAsyncResult *result,
                                              GError **error)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);
	g_return_val_if_fail (g_task_is_valid (result, web_view), FALSE);

	return g_task_propagate_pointer (G_TASK (result), error);
}

gchar *
e_web_view_get_selection_content_html_sync (EWebView *web_view,
                                            GCancellable *cancellable,
                                            GError **error)
{
	GDBusProxy *web_extension;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (web_extension) {
		GVariant *result;

		result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_full (
				web_extension,
				"GetSelectionContentHTML",
				g_variant_new (
					"(t)",
					webkit_web_view_get_page_id (
						WEBKIT_WEB_VIEW (web_view))),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				cancellable,
				error);

		if (result) {
			gchar *html_content = NULL;

			g_variant_get (result, "(s)", &html_content);
			g_variant_unref (result);
			return html_content;
		}
	}

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
e_web_view_update_fonts_settings (GSettings *font_settings,
                                  GSettings *aliasing_settings,
                                  PangoFontDescription *ms_font,
                                  PangoFontDescription *vw_font,
                                  GtkWidget *view_widget)
{
	gboolean clean_ms = FALSE, clean_vw = FALSE;
	gchar *aa = NULL;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GdkColor *link = NULL;
	GdkColor *visited = NULL;
	GString *stylesheet;
	GtkStyleContext *context;
	PangoFontDescription *min_size, *ms, *vw;
	WebKitSettings *wk_settings;
	WebKitUserContentManager *manager;
	WebKitUserStyleSheet *style_sheet;

	if (!ms_font) {
		gchar *font;

		font = g_settings_get_string (
			font_settings,
			"monospace-font-name");

		ms = pango_font_description_from_string (
			(font != NULL) ? font : "monospace 10");

		clean_ms = TRUE;

		g_free (font);
	} else
		ms = ms_font;

	if (!vw_font) {
		gchar *font;

		font = g_settings_get_string (
			font_settings,
			"font-name");

		vw = pango_font_description_from_string (
			(font != NULL) ? font : "serif 10");

		clean_vw = TRUE;

		g_free (font);
	} else
		vw = vw_font;

	if (pango_font_description_get_size (ms) < pango_font_description_get_size (vw))
		min_size = ms;
	else
		min_size = vw;

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

	if (aliasing_settings != NULL)
		aa = g_settings_get_string (
			aliasing_settings, "antialiasing");

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
		"}\n",
		pango_font_description_get_family (ms),
		pango_font_description_get_size (ms) / PANGO_SCALE,
		pango_font_description_get_weight (ms),
		styles[pango_font_description_get_style (ms)]);

	if (view_widget) {
		context = gtk_widget_get_style_context (view_widget);
		gtk_style_context_get_style (
			context,
			"link-color", &link,
			"visited-link-color", &visited,
			NULL);

		if (link == NULL) {
			GdkRGBA rgba;
			GtkStateFlags state;

			link = g_slice_new0 (GdkColor);
			link->blue = G_MAXINT16;

			rgba.alpha = 1;
			rgba.red = 0;
			rgba.green = 0;
			rgba.blue = 1;

			state = gtk_style_context_get_state (context);
			state = state & (~(GTK_STATE_FLAG_VISITED | GTK_STATE_FLAG_LINK));
			state = state | GTK_STATE_FLAG_LINK;

			gtk_style_context_save (context);
			gtk_style_context_set_state (context, state);
			gtk_style_context_get_color (context, state, &rgba);
			gtk_style_context_restore (context);

			e_rgba_to_color (&rgba, link);
		}

		if (visited == NULL) {
			GdkRGBA rgba;
			GtkStateFlags state;

			visited = g_slice_new0 (GdkColor);
			visited->red = G_MAXINT16;

			rgba.alpha = 1;
			rgba.red = 1;
			rgba.green = 0;
			rgba.blue = 0;

			state = gtk_style_context_get_state (context);
			state = state & (~(GTK_STATE_FLAG_VISITED | GTK_STATE_FLAG_LINK));
			state = state | GTK_STATE_FLAG_VISITED;

			gtk_style_context_save (context);
			gtk_style_context_set_state (context, state);
			gtk_style_context_get_color (context, state, &rgba);
			gtk_style_context_restore (context);

			e_rgba_to_color (&rgba, visited);
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
	}

	wk_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view_widget));

	g_object_set (
		wk_settings,
		"default-font-size",
		e_util_normalize_font_size (
			view_widget, pango_font_description_get_size (vw) / PANGO_SCALE),
		"default-font-family",
		pango_font_description_get_family (vw),
		"monospace-font-family",
		pango_font_description_get_family (ms),
		"default-monospace-font-size",
		e_util_normalize_font_size (
			view_widget, pango_font_description_get_size (ms) / PANGO_SCALE),
		"minimum-font-size",
		e_util_normalize_font_size (
			view_widget, pango_font_description_get_size (min_size) / PANGO_SCALE),
		NULL);

	manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (view_widget));
	webkit_user_content_manager_remove_all_style_sheets (manager);

	style_sheet = webkit_user_style_sheet_new (
		stylesheet->str,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
		WEBKIT_USER_STYLE_LEVEL_USER,
		NULL,
		NULL);

	webkit_user_content_manager_add_style_sheet (manager, style_sheet);

	webkit_user_style_sheet_unref (style_sheet);

	g_string_free (stylesheet, TRUE);

	if (clean_ms)
		pango_font_description_free (ms);
	if (clean_vw)
		pango_font_description_free (vw);
}

WebKitSettings *
e_web_view_get_default_webkit_settings (void)
{
	return webkit_settings_new_with_settings (
		"auto-load-images", TRUE,
		"default-charset", "utf-8",
		"enable-html5-database", FALSE,
		"enable-dns-prefetching", FALSE,
		"enable-html5-local-storage", FALSE,
		"enable-java", FALSE,
		"enable-javascript", FALSE,
		"enable-offline-web-application-cache", FALSE,
		"enable-page-cache", FALSE,
		"enable-plugins", FALSE,
		"enable-smooth-scrolling", FALSE,
		"media-playback-allows-inline", FALSE,
		NULL);
}

void
e_web_view_update_fonts (EWebView *web_view)
{
	EWebViewClass *class;
	PangoFontDescription *ms = NULL, *vw = NULL;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_if_fail (class != NULL);

	if (class->set_fonts != NULL)
		class->set_fonts (web_view, &ms, &vw);

	e_web_view_update_fonts_settings (
		web_view->priv->font_settings,
		web_view->priv->aliasing_settings,
		ms, vw, GTK_WIDGET (web_view));

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

	e_util_load_file_chooser_folder (file_chooser);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		e_util_save_file_chooser_folder (file_chooser);

		destination = gtk_file_chooser_get_file (file_chooser);
	}

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
		text = g_strdup_printf (_("Saving image to “%s”"), uri);
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
web_view_request_process_thread (GTask *task,
				 gpointer source_object,
				 gpointer task_data,
				 GCancellable *cancellable)
{
	AsyncContext *async_context = task_data;
	gint64 stream_length = -1;
	gchar *mime_type = NULL;
	GError *local_error = NULL;

	if (!e_content_request_process_sync (async_context->content_request,
		async_context->uri, source_object, &async_context->input_stream,
		&stream_length, &mime_type, cancellable, &local_error)) {
		g_task_return_error (task, local_error);
	} else {
		g_task_return_boolean (task, TRUE);
	}

	g_free (mime_type);
}

/**
 * e_web_view_request:
 * @web_view: an #EWebView
 * @uri: the URI to load
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously requests data at @uri as displaed in the @web_view.
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
	EContentRequest *content_request = NULL;
	AsyncContext *async_context;
	GSList *link;
	GTask *task;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (uri != NULL);

	for (link = web_view->priv->content_requests; link; link = g_slist_next (link)) {
		EContentRequest *adept = link->data;

		if (!E_IS_CONTENT_REQUEST (adept) ||
		    !e_content_request_can_process_uri (adept, uri))
			continue;

		content_request = adept;
		break;
	}

	async_context = g_slice_new0 (AsyncContext);
	async_context->uri = g_strdup (uri);

	task = g_task_new (web_view, cancellable, callback, user_data);
	g_task_set_task_data (task, async_context, async_context_free);
	g_task_set_check_cancellable (task, TRUE);

	if (content_request) {
		async_context->content_request = g_object_ref (content_request);
		g_task_run_in_thread (task, web_view_request_process_thread);
	} else {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
			_("Cannot get URI “%s”, do not know how to download it."), uri);
	}

	g_object_unref (task);
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
	AsyncContext *async_context;

	g_return_val_if_fail (g_task_is_valid (result, web_view), NULL);

	if (!g_task_propagate_boolean (G_TASK (result), error))
		return NULL;

	async_context = g_task_get_task_data (G_TASK (result));

	g_return_val_if_fail (async_context->input_stream != NULL, NULL);

	return g_object_ref (async_context->input_stream);
}

/**
 * e_web_view_create_and_add_css_style_sheet:
 * @web_view: an #EWebView
 * @style_sheet_id: CSS style sheet's id
 *
 * Creates new CSS style sheet with given @style_sheel_id and inserts
 * it into given @web_view document.
 **/
void
e_web_view_create_and_add_css_style_sheet (EWebView *web_view,
                                           const gchar *style_sheet_id)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (style_sheet_id && *style_sheet_id);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (web_extension) {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			web_extension,
			"CreateAndAddCSSStyleSheet",
			g_variant_new (
				"(ts)",
				webkit_web_view_get_page_id (
					WEBKIT_WEB_VIEW (web_view)),
				style_sheet_id),
			NULL);
	}
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
e_web_view_add_css_rule_into_style_sheet (EWebView *web_view,
                                          const gchar *style_sheet_id,
                                          const gchar *selector,
                                          const gchar *style)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (style_sheet_id && *style_sheet_id);
	g_return_if_fail (selector && *selector);
	g_return_if_fail (style && *style);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (web_extension) {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			web_extension,
			"AddCSSRuleIntoStyleSheet",
			g_variant_new (
				"(tsss)",
				webkit_web_view_get_page_id (
					WEBKIT_WEB_VIEW (web_view)),
				style_sheet_id,
				selector,
				style),
			NULL);
	}
}

/**
 * e_web_view_get_document_uri_from_point:
 * @web_view: an #EWebView
 * @x: x-coordinate
 * @y: y-coordinate
 *
 * Returns: A document URI which is under the @x, @y coordinates or %NULL,
 * if there is none. Free the returned pointer with g_free() when done with it.
 *
 * Since: 3.22
 **/
gchar *
e_web_view_get_document_uri_from_point (EWebView *web_view,
					gint32 x,
					gint32 y)
{
	GDBusProxy *web_extension;
	GVariant *result;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (!web_extension)
		return NULL;

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_full (
		web_extension,
		"GetDocumentURIFromPoint",
		g_variant_new (
			"(tii)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)),
			x,
			y),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&local_error);

	if (local_error)
		g_dbus_error_strip_remote_error (local_error);

	e_util_claim_dbus_proxy_call_error (web_extension, "GetDocumentURIFromPoint", local_error);
	g_clear_error (&local_error);

	if (result) {
		gchar *uri = NULL;

		g_variant_get (result, "(s)", &uri);
		g_variant_unref (result);

		if (g_strcmp0 (uri, "") == 0) {
			g_free (uri);
			uri = NULL;
		}

		return uri;
	}

	return NULL;
}

/**
 * e_web_view_set_document_iframe_src:
 * @web_view: an #EWebView
 * @document_uri: a document URI for whose IFrame change the source
 * @new_iframe_src: the source to change the IFrame to
 *
 * Change IFrame source for the given @document_uri IFrame
 * to the @new_iframe_src.
 *
 * Since: 3.22
 **/
void
e_web_view_set_document_iframe_src (EWebView *web_view,
				    const gchar *document_uri,
				    const gchar *new_iframe_src)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (!web_extension)
		return;

	/* Cannot call this synchronously, blocking the local main loop, because the reload
	   can on the WebProcess side can be asking for a redirection policy, waiting
	   for a response which may be waiting in the blocked main loop. */
	e_util_invoke_g_dbus_proxy_call_with_error_check (
		web_extension,
		"SetDocumentIFrameSrc",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)),
			document_uri,
			new_iframe_src),
		NULL);
}

/**
 * EWebViewElementClickedFunc:
 * @web_view: an #EWebView
 * @element_class: an element class, as set on the element which had been clicked
 * @element_value: a 'value' attribute content of the clicked element
 * @element_position: a #GtkAllocation with the position of the clicked element
 * @user_data: user data as provided in the e_web_view_register_element_clicked() call
 *
 * The callback is called whenever an element of class @element_class is clicked.
 * The @element_value is a content of the 'value' attribute of the clicked element.
 * The @element_position is the place of the element within the web page, already
 * accounting scrollbar positions.
 *
 * See: e_web_view_register_element_clicked, e_web_view_unregister_element_clicked
 *
 * Since: 3.22
 **/

/**
 * e_web_view_register_element_clicked:
 * @web_view: an #EWebView
 * @element_class: an element class on which to listen for clicking
 * @callback: an #EWebViewElementClickedFunc to call, when the element is clicked
 * @user_data: user data to pass to @callback
 *
 * Registers a @callback to be called when any element of the class @element_class
 * is clicked. If the element contains a 'value' attribute, then it is passed to
 * the @callback too. These callback are valid until a new content of the @web_view
 * is loaded, after which all the registered callbacks are forgotten.
 *
 * Since: 3.22
 **/
void
e_web_view_register_element_clicked (EWebView *web_view,
				     const gchar *element_class,
				     EWebViewElementClickedFunc callback,
				     gpointer user_data)
{
	ElementClickedData *ecd;
	GPtrArray *cbs;
	guint ii;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_class != NULL);
	g_return_if_fail (callback != NULL);

	cbs = g_hash_table_lookup (web_view->priv->element_clicked_cbs, element_class);
	if (cbs) {
		for (ii = 0; ii < cbs->len; ii++) {
			ecd = g_ptr_array_index (cbs, ii);

			if (ecd && ecd->callback == callback && ecd->user_data == user_data) {
				/* Callback is already registered, but re-register it, in case the page
				   was changed dynamically and new elements with the given call are added. */
				web_view_register_element_clicked_hfunc ((gpointer) element_class, cbs, web_view);
				return;
			}
		}
	}

	ecd = g_new0 (ElementClickedData, 1);
	ecd->callback = callback;
	ecd->user_data = user_data;

	if (!cbs) {
		cbs = g_ptr_array_new_full (1, g_free);
		g_ptr_array_add (cbs, ecd);

		g_hash_table_insert (web_view->priv->element_clicked_cbs, g_strdup (element_class), cbs);
	} else {
		g_ptr_array_add (cbs, ecd);
	}

	/* Dynamically changing page can call this multiple times; re-register all classes */
	g_hash_table_foreach (web_view->priv->element_clicked_cbs, web_view_register_element_clicked_hfunc, web_view);
}

/**
 * e_web_view_unregister_element_clicked:
 * @web_view: an #EWebView
 * @element_class: an element class on which to listen for clicking
 * @callback: an #EWebViewElementClickedFunc to call, when the element is clicked
 * @user_data: user data to pass to @callback
 *
 * Unregisters the @callback for the @element_class with the given @user_data, which
 * should be previously registered with e_web_view_register_element_clicked(). This
 * unregister is usually not needed, because the @web_view unregisters all callbacks
 * when a new content is loaded.
 *
 * Since: 3.22
 **/
void
e_web_view_unregister_element_clicked (EWebView *web_view,
				       const gchar *element_class,
				       EWebViewElementClickedFunc callback,
				       gpointer user_data)
{
	ElementClickedData *ecd;
	GPtrArray *cbs;
	guint ii;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_class != NULL);
	g_return_if_fail (callback != NULL);

	cbs = g_hash_table_lookup (web_view->priv->element_clicked_cbs, element_class);
	if (!cbs)
		return;

	for (ii = 0; ii < cbs->len; ii++) {
		ecd = g_ptr_array_index (cbs, ii);

		if (ecd && ecd->callback == callback && ecd->user_data == user_data) {
			g_ptr_array_remove (cbs, ecd);
			if (!cbs->len)
				g_hash_table_remove (web_view->priv->element_clicked_cbs, element_class);
			break;
		}
	}
}

void
e_web_view_set_element_hidden (EWebView *web_view,
			       const gchar *element_id,
			       gboolean hidden)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id && *element_id);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (!web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		web_extension,
		"SetElementHidden",
		g_variant_new (
			"(tsb)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)),
			element_id,
			hidden),
		NULL);
}

void
e_web_view_set_element_style_property (EWebView *web_view,
				       const gchar *element_id,
				       const gchar *property_name,
				       const gchar *value,
				       const gchar *priority)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id && *element_id);
	g_return_if_fail (property_name && *property_name);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (!web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		web_extension,
		"SetElementStyleProperty",
		g_variant_new (
			"(tssss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)),
			element_id,
			property_name,
			value ? value : "",
			priority ? priority : ""),
		NULL);
}

void
e_web_view_set_element_attribute (EWebView *web_view,
				  const gchar *element_id,
				  const gchar *namespace_uri,
				  const gchar *qualified_name,
				  const gchar *value)
{
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id && *element_id);
	g_return_if_fail (qualified_name && *qualified_name);

	web_extension = e_web_view_get_web_extension_proxy (web_view);
	if (!web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		web_extension,
		"SetElementAttribute",
		g_variant_new (
			"(tssss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)),
			element_id,
			namespace_uri ? namespace_uri : "",
			qualified_name,
			value ? value : ""),
		NULL);
}
