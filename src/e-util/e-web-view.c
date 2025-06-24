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
#include "e-selectable.h"
#include "e-stock-request.h"
#include "e-ui-action.h"
#include "e-ui-action-group.h"
#include "e-ui-manager.h"
#include "e-web-view-jsc-utils.h"

#include "e-web-view.h"

typedef struct _AsyncContext AsyncContext;

typedef struct _ElementClickedData {
	EWebViewElementClickedFunc callback;
	gpointer user_data;
} ElementClickedData;

struct _EWebViewPrivate {
	EUIManager *ui_manager;
	gchar *selected_uri;
	gchar *cursor_image_src;

	GQueue highlights;
	gboolean highlights_enabled;

	EUIAction *open_proxy;
	EUIAction *print_proxy;
	EUIAction *save_as_proxy;

	/* Lockdown Options */
	gboolean disable_printing;
	gboolean disable_save_to_disk;

	gboolean caret_mode;

	GSettings *font_settings;
	gulong font_name_changed_handler_id;
	gulong monospace_font_name_changed_handler_id;

	GHashTable *scheme_handlers; /* gchar *scheme ~> EContentRequest */
	GHashTable *old_settings;

	WebKitFindController *find_controller;
	gulong found_text_handler_id;
	gulong failed_to_find_text_handler_id;

	gboolean has_hover_link;

	GHashTable *element_clicked_cbs; /* gchar *element_class ~> GPtrArray {ElementClickedData} */

	gboolean has_selection;
	gboolean need_input;

	GCancellable *cancellable;

	gchar *last_popup_iframe_src;
	gchar *last_popup_iframe_id;
	gchar *last_popup_element_id;
	gchar *last_popup_link_uri;

	gint minimum_font_size;
};

struct _AsyncContext {
	GTask *task;
	EActivity *activity;
	GFile *destination;
	GInputStream *input_stream;
	EContentRequest *content_request;
	gchar *uri;
};

enum {
	PROP_0,
	PROP_CARET_MODE,
	PROP_COPY_TARGET_LIST,
	PROP_CURSOR_IMAGE_SRC,
	PROP_DISABLE_PRINTING,
	PROP_DISABLE_SAVE_TO_DISK,
	PROP_HAS_SELECTION,
	PROP_NEED_INPUT,
	PROP_MINIMUM_FONT_SIZE,
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
	CONTENT_LOADED,
	BEFORE_POPUP_EVENT,
	RESOURCE_LOADED,
	SET_FONTS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void e_web_view_alert_sink_init (EAlertSinkInterface *iface);
static void e_web_view_selectable_init (ESelectableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EWebView, e_web_view, WEBKIT_TYPE_WEB_VIEW,
	G_ADD_PRIVATE (EWebView)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_web_view_alert_sink_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, e_web_view_selectable_init))

static void
async_context_free (gpointer ptr)
{
	AsyncContext *async_context = ptr;

	if (!async_context)
		return;

	g_clear_object (&async_context->task);
	g_clear_object (&async_context->activity);
	g_clear_object (&async_context->destination);
	g_clear_object (&async_context->input_stream);
	g_clear_object (&async_context->content_request);
	g_free (async_context->uri);

	g_slice_free (AsyncContext, async_context);
}

static void
e_web_view_update_spell_checking (EWebView *web_view,
				  GSettings *settings)
{
	WebKitWebContext *web_context;

	web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (web_view));

	if (g_settings_get_boolean (settings, "composer-inline-spelling")) {
		gchar **languages;

		languages = g_settings_get_strv (settings, "composer-spell-languages");

		if (languages)
			webkit_web_context_set_spell_checking_languages (web_context, (const gchar * const *) languages);
		webkit_web_context_set_spell_checking_enabled (web_context, languages != NULL);

		g_strfreev (languages);
	} else {
		webkit_web_context_set_spell_checking_enabled (web_context, FALSE);
	}
}

static void
e_web_view_spell_settings_changed_cb (GSettings *settings,
				      const gchar *key,
				      gpointer user_data)
{
	EWebView *web_view = user_data;

	e_web_view_update_spell_checking (web_view, settings);
}

static void
action_copy_clipboard_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EWebView *web_view = user_data;

	e_web_view_copy_clipboard (web_view);
}

static void
e_web_view_search_web_get_selection_cb (GObject *source,
					GAsyncResult *result,
					gpointer user_data)
{
	GSList *texts;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_WEB_VIEW (source));

	e_web_view_jsc_get_selection_finish (WEBKIT_WEB_VIEW (source), result, &texts, &local_error);

	if (local_error &&
	    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		e_alert_submit (E_ALERT_SINK (source), "widgets:get-selected-text-failed", local_error->message, NULL);
	} else if (texts) {
		GSettings *settings;
		gchar *text = texts->data;
		gchar *uri_prefix;
		gchar *escaped;
		gchar *uri;

		g_strstrip (text);

		settings = e_util_ref_settings ("org.gnome.evolution.shell");
		uri_prefix = g_settings_get_string (settings, "search-web-uri-prefix");
		g_object_unref (settings);

		escaped = camel_url_encode (text, "& ?#:;,/\\");

		uri = g_strconcat (uri_prefix, escaped, NULL);
		if (uri && g_ascii_strncasecmp (uri, "https://", 8) == 0) {
			GtkWidget *toplevel;

			toplevel = gtk_widget_get_toplevel (GTK_WIDGET (source));

			e_show_uri (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL, uri);
		} else {
			g_printerr ("Incorrect URI provided, expects https:// prefix, but has got: '%s'\n", uri ? uri : "null");
		}

		g_free (uri_prefix);
		g_free (escaped);
		g_free (uri);
	}

	g_clear_error (&local_error);
	g_slist_free_full (texts, g_free);
}

static void
action_search_web_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EWebView *web_view = user_data;

	e_web_view_jsc_get_selection (WEBKIT_WEB_VIEW (web_view), E_TEXT_FORMAT_PLAIN, web_view->priv->cancellable,
		e_web_view_search_web_get_selection_cb, NULL);
}

static void
action_http_open_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EWebView *web_view = user_data;
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
action_mailto_copy_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EWebView *web_view = user_data;

	webview_mailto_copy (web_view, FALSE);
}

static void
action_mailto_copy_raw_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EWebView *web_view = user_data;

	webview_mailto_copy (web_view, TRUE);
}

static void
action_select_all_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EWebView *web_view = user_data;

	e_web_view_select_all (web_view);
}

static void
action_send_message_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EWebView *web_view = user_data;
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
action_uri_copy_cb (EUIAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EWebView *web_view = user_data;
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
action_image_copy_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EWebView *web_view = user_data;

	e_web_view_cursor_image_copy (web_view);
}

static void
action_image_save_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EWebView *web_view = user_data;

	e_web_view_cursor_image_save (web_view);
}

static void
action_open_cb (EUIAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	EWebView *web_view = user_data;

	if (web_view->priv->open_proxy)
		g_action_activate (G_ACTION (web_view->priv->open_proxy), NULL);
}

static void
action_print_cb (EUIAction *action,
		 GVariant *parameter,
		 gpointer user_data)
{
	EWebView *web_view = user_data;

	if (web_view->priv->print_proxy)
		g_action_activate (G_ACTION (web_view->priv->print_proxy), NULL);
}

static void
action_save_as_cb (EUIAction *action,
		   GVariant *parameter,
		   gpointer user_data)
{
	EWebView *web_view = user_data;

	if (web_view->priv->save_as_proxy)
		g_action_activate (G_ACTION (web_view->priv->save_as_proxy), NULL);
}

static void
webkit_find_controller_found_text_cb (WebKitFindController *find_controller,
                                      guint match_count,
                                      EWebView *web_view)
{
	if (web_view->priv->highlights_enabled && !g_queue_is_empty (&web_view->priv->highlights))
		e_web_view_unselect_all (web_view);
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
			WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE,
			G_MAXUINT);
	}
}

static void
web_view_got_elem_from_point_for_popup_event_cb (GObject *source_object,
						 GAsyncResult *result,
						 gpointer user_data)
{
	EWebView *web_view;
	GdkEvent *event = user_data;
	GError *error = NULL;

	g_return_if_fail (E_IS_WEB_VIEW (source_object));

	web_view = E_WEB_VIEW (source_object);

	g_clear_pointer (&web_view->priv->last_popup_iframe_src, g_free);
	g_clear_pointer (&web_view->priv->last_popup_iframe_id, g_free);
	g_clear_pointer (&web_view->priv->last_popup_element_id, g_free);

	if (!e_web_view_jsc_get_element_from_point_finish (WEBKIT_WEB_VIEW (web_view), result,
		&web_view->priv->last_popup_iframe_src,
		&web_view->priv->last_popup_iframe_id,
		&web_view->priv->last_popup_element_id,
		&error)) {
		g_warning ("%s: Failed to get element from point: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		gboolean handled = FALSE;

		g_signal_emit (web_view, signals[BEFORE_POPUP_EVENT], 0,
			web_view->priv->last_popup_link_uri, NULL);

		g_signal_emit (web_view, signals[POPUP_EVENT], 0,
			web_view->priv->last_popup_link_uri, event, &handled);
	}

	if (event)
		gdk_event_free (event);
	g_clear_error (&error);
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
	gchar *link_uri = NULL;
	gdouble xx, yy;

	web_view = E_WEB_VIEW (webkit_web_view);

	g_clear_pointer (&web_view->priv->cursor_image_src, g_free);
	g_clear_pointer (&web_view->priv->last_popup_iframe_src, g_free);
	g_clear_pointer (&web_view->priv->last_popup_iframe_id, g_free);
	g_clear_pointer (&web_view->priv->last_popup_element_id, g_free);
	g_clear_pointer (&web_view->priv->last_popup_link_uri, g_free);

	if (!hit_test_result)
		return FALSE;

	context = webkit_hit_test_result_get_context (hit_test_result);

	/* Show the default menu for an editable */
	if ((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) != 0)
		return FALSE;

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE) {
		gchar *image_uri = NULL;

		g_object_get (hit_test_result, "image-uri", &image_uri, NULL);

		web_view->priv->cursor_image_src = image_uri;
	}

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
		g_object_get (hit_test_result, "link-uri", &link_uri, NULL);

	web_view->priv->last_popup_link_uri = link_uri;

	if (!gdk_event_get_coords (event, &xx, &yy)) {
		xx = 1;
		yy = 1;
	}

	e_web_view_jsc_get_element_from_point (WEBKIT_WEB_VIEW (web_view), xx, yy, web_view->priv->cancellable,
		web_view_got_elem_from_point_for_popup_event_cb, event ? gdk_event_copy (event) : NULL);

	return TRUE;
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
		GUri *uri_link, *uri_view;

		uri_link = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		uri_view = g_uri_parse (view_uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		if (uri_link && uri_view) {
			const gchar *tmp1, *tmp2;

			tmp1 = g_uri_get_scheme (uri_link);
			tmp2 = g_uri_get_scheme (uri_view);

			/* The scheme on both URIs should be the same */
			if (tmp1 && tmp2 && g_ascii_strcasecmp (tmp1, tmp2) != 0)
				goto free_uris;

			tmp1 = g_uri_get_host (uri_link);
			tmp2 = g_uri_get_host (uri_view);

			/* The host on both URIs should be the same */
			if (tmp1 && tmp2 && g_ascii_strcasecmp (tmp1, tmp2) != 0)
				goto free_uris;

			/* URI from link should have fragment set - could be empty */
			if (g_uri_get_fragment (uri_link)) {
				g_uri_unref (uri_link);
				g_uri_unref (uri_view);
				webkit_policy_decision_use (decision);
				return TRUE;
			}
		}

 free_uris:
		if (uri_link)
			g_uri_unref (uri_link);
		if (uri_view)
			g_uri_unref (uri_view);
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
e_web_view_update_styles (EWebView *web_view,
			  const gchar *iframe_id)
{
	GdkRGBA color;
	gchar *color_value;
	gchar *style;
	GtkStyleContext *style_context;
	WebKitWebView *webkit_web_view;
	gdouble bg_brightness, fg_brightness;

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

	webkit_web_view = WEBKIT_WEB_VIEW (web_view);

	webkit_web_view_set_background_color (webkit_web_view, &color);

	e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
		iframe_id,
		"-e-web-view-style-sheet",
		".-e-web-view-background-color",
		style,
		web_view->priv->cancellable);

	bg_brightness = e_utils_get_color_brightness (&color);

	g_free (color_value);
	g_free (style);

	if (gtk_style_context_lookup_color (style_context, "theme_fg_color", &color))
		color_value = g_strdup_printf ("#%06x", e_rgba_to_value (&color));
	else
		color_value = g_strdup (E_UTILS_DEFAULT_THEME_FG_COLOR);

	style = g_strconcat ("color: ", color_value, ";", NULL);

	e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
		iframe_id,
		"-e-web-view-style-sheet",
		".-e-web-view-text-color",
		style,
		web_view->priv->cancellable);

	fg_brightness = e_utils_get_color_brightness (&color);

	g_free (color_value);
	g_free (style);

	/* assume darker background than text color means dark theme */
	if (bg_brightness < fg_brightness) {
		e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
			iframe_id,
			"-e-web-view-style-sheet",
			"button",
			"color-scheme: dark;",
			web_view->priv->cancellable);
		e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
			iframe_id,
			"-e-web-view-style-sheet",
			".-evo-color-scheme-light",
			"display: none;",
			web_view->priv->cancellable);
		e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
			iframe_id,
			"-e-web-view-style-sheet",
			".-evo-color-scheme-dark",
			"display: initial;",
			web_view->priv->cancellable);
	} else {
		e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
			iframe_id,
			"-e-web-view-style-sheet",
			"button",
			"color-scheme: inherit;",
			web_view->priv->cancellable);
		e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
			iframe_id,
			"-e-web-view-style-sheet",
			".-evo-color-scheme-light",
			"display: initial;",
			web_view->priv->cancellable);
		e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
			iframe_id,
			"-e-web-view-style-sheet",
			".-evo-color-scheme-dark",
			"display: none;",
			web_view->priv->cancellable);
	}

	e_web_view_jsc_add_rule_into_style_sheet (webkit_web_view,
		iframe_id,
		"-e-web-view-style-sheet",
		"body, div, p, td",
		"unicode-bidi: plaintext;",
		web_view->priv->cancellable);
}

static void
style_updated_cb (EWebView *web_view)
{
	e_web_view_update_styles (web_view, "*");
}

static void
e_web_view_set_has_selection (EWebView *web_view,
			      gboolean has_selection)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if ((!web_view->priv->has_selection) == (!has_selection))
		return;

	web_view->priv->has_selection = has_selection;

	g_object_notify (G_OBJECT (web_view), "has-selection");
}

static void
web_view_load_changed_cb (WebKitWebView *webkit_web_view,
                          WebKitLoadEvent load_event,
                          gpointer user_data)
{
	EWebView *web_view;

	web_view = E_WEB_VIEW (webkit_web_view);

	if (load_event == WEBKIT_LOAD_STARTED) {
		g_hash_table_remove_all (web_view->priv->element_clicked_cbs);
		e_web_view_set_has_selection (web_view, FALSE);
	}

	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	/* Make sure the initialize function is called for the top document when it is loaded. */
	e_web_view_jsc_run_script (webkit_web_view, web_view->priv->cancellable,
		"Evo.EnsureMainDocumentInitialized();");

	e_web_view_update_styles (web_view, "");

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

static void
web_view_uri_request_done_cb (GObject *source_object,
			      GAsyncResult *result,
			      gpointer user_data)
{
	WebKitURISchemeRequest *request = user_data;
	WebKitWebView *web_view;
	GInputStream *stream = NULL;
	gint64 stream_length = -1;
	gchar *mime_type = NULL;
	GError *error = NULL;

	g_return_if_fail (E_IS_CONTENT_REQUEST (source_object));
	g_return_if_fail (WEBKIT_IS_URI_SCHEME_REQUEST (request));

	if (!e_content_request_process_finish (E_CONTENT_REQUEST (source_object),
		result, &stream, &stream_length, &mime_type, &error)) {
		if (!error)
			error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to get '%s'", webkit_uri_scheme_request_get_uri (request));
		webkit_uri_scheme_request_finish_error (request, error);
		g_clear_error (&error);
	} else {
		webkit_uri_scheme_request_finish (request, stream, stream_length, mime_type);

		g_clear_object (&stream);
		g_free (mime_type);
	}

	web_view = webkit_uri_scheme_request_get_web_view (request);
	g_signal_emit (web_view, signals[RESOURCE_LOADED], 0, NULL);

	g_object_unref (request);
}

static void
e_web_view_process_uri_request (EWebView *web_view,
				WebKitURISchemeRequest *request)
{
	EContentRequest *content_request;
	const gchar *scheme;
	const gchar *uri;
	gchar *redirect_to_uri = NULL;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (WEBKIT_IS_URI_SCHEME_REQUEST (request));

	scheme = webkit_uri_scheme_request_get_scheme (request);
	g_return_if_fail (scheme != NULL);

	content_request = g_hash_table_lookup (web_view->priv->scheme_handlers, scheme);

	if (!content_request) {
		g_warning ("%s: Cannot find handler for scheme '%s'", G_STRFUNC, scheme);
		return;
	}

	uri = webkit_uri_scheme_request_get_uri (request);

	g_return_if_fail (e_content_request_can_process_uri (content_request, uri));

	/* Expects an empty string to abandon the request,
	   or NULL to keep the passed-in uri,
	   or a new uri to load instead. */
	g_signal_emit (web_view, signals[URI_REQUESTED], 0, uri, &redirect_to_uri);

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

	e_content_request_process (content_request, uri, G_OBJECT (web_view), web_view->priv->cancellable,
		web_view_uri_request_done_cb, g_object_ref (request));

	g_free (redirect_to_uri);
}

static void
web_view_process_uri_request_cb (WebKitURISchemeRequest *request,
				 gpointer user_data)
{
	WebKitWebView *web_view;

	web_view = webkit_uri_scheme_request_get_web_view (request);

	if (E_IS_WEB_VIEW (web_view)) {
		e_web_view_process_uri_request (E_WEB_VIEW (web_view), request);
	} else {
		GError *error;

		error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED, "Unexpected WebView type");
		webkit_uri_scheme_request_finish_error (request, error);
		g_clear_error (&error);

		g_warning ("%s: Unexpected WebView type '%s' received", G_STRFUNC, web_view ? G_OBJECT_TYPE_NAME (web_view) : "null");

		return;
	}
}

static GSList *known_schemes = NULL;

static void
web_view_web_context_gone (gpointer user_data,
			   GObject *obj)
{
	gpointer *pweb_context = user_data;

	g_return_if_fail (pweb_context != NULL);

	*pweb_context = NULL;

	g_slist_free_full (known_schemes, g_free);
	known_schemes = NULL;
}

static void
web_view_ensure_scheme_known (WebKitWebContext *web_context,
			      const gchar *scheme)
{
	GSList *link;

	g_return_if_fail (WEBKIT_IS_WEB_CONTEXT (web_context));
	g_return_if_fail (scheme != NULL);

	for (link = known_schemes; link; link = g_slist_next (link)) {
		if (g_strcmp0 (scheme, link->data) == 0)
			break;
	}

	if (!link) {
		known_schemes = g_slist_prepend (known_schemes, g_strdup (scheme));

		webkit_web_context_register_uri_scheme (web_context, scheme, web_view_process_uri_request_cb, NULL, NULL);
	}
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
		param_spec = g_object_class_find_property (object_class, "web-context");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec))) {
			/* Share one web_context between all previews. */
			static gpointer web_context = NULL;

			if (!web_context) {
				GSList *link;
				gchar *plugins_path;
				#ifdef ENABLE_MAINTAINER_MODE
				const gchar *source_webkitdatadir;
				#endif

				web_context = webkit_web_context_new ();

				webkit_web_context_set_cache_model (web_context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
				webkit_web_context_set_web_extensions_directory (web_context, EVOLUTION_WEB_EXTENSIONS_DIR);
				webkit_web_context_set_sandbox_enabled (web_context, TRUE);
				webkit_web_context_add_path_to_sandbox (web_context, EVOLUTION_WEBKITDATADIR, TRUE);

				plugins_path = g_build_filename (e_get_user_data_dir (), "preview-plugins", NULL);
				if (g_file_test (plugins_path, G_FILE_TEST_IS_DIR))
					webkit_web_context_add_path_to_sandbox (web_context, plugins_path, TRUE);
				g_free (plugins_path);

				#ifdef ENABLE_MAINTAINER_MODE
				source_webkitdatadir = g_getenv ("EVOLUTION_SOURCE_WEBKITDATADIR");
				if (source_webkitdatadir && *source_webkitdatadir)
					webkit_web_context_add_path_to_sandbox (web_context, source_webkitdatadir, TRUE);
				#endif

				g_object_weak_ref (G_OBJECT (web_context), web_view_web_context_gone, &web_context);

				for (link = known_schemes; link; link = g_slist_next (link)) {
					const gchar *scheme = link->data;

					webkit_web_context_register_uri_scheme (web_context, scheme, web_view_process_uri_request_cb, NULL, NULL);
				}
			} else {
				g_object_ref (web_context);
			}

			g_value_take_object (param->value, web_context);
		}
	}

	g_type_class_unref (object_class);

	return G_OBJECT_CLASS (e_web_view_parent_class)->constructor (type, n_construct_properties, construct_properties);
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

		case PROP_MINIMUM_FONT_SIZE:
			e_web_view_set_minimum_font_size (
				E_WEB_VIEW (object),
				g_value_get_int (value));
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

		case PROP_HAS_SELECTION:
			g_value_set_boolean (value, e_web_view_has_selection (E_WEB_VIEW (object)));
			return;

		case PROP_MINIMUM_FONT_SIZE:
			g_value_set_int (
				value, e_web_view_get_minimum_font_size (
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
	EWebView *self = E_WEB_VIEW (object);

	/* This can be called during dispose, thus disconnect early */
	g_signal_handlers_disconnect_by_func (object, G_CALLBACK (style_updated_cb), NULL);

	if (self->priv->cancellable) {
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	if (self->priv->font_name_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->font_settings,
			self->priv->font_name_changed_handler_id);
		self->priv->font_name_changed_handler_id = 0;
	}

	if (self->priv->monospace_font_name_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->font_settings,
			self->priv->monospace_font_name_changed_handler_id);
		self->priv->monospace_font_name_changed_handler_id = 0;
	}

	if (self->priv->found_text_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->find_controller,
			self->priv->found_text_handler_id);
		self->priv->found_text_handler_id = 0;
	}

	if (self->priv->failed_to_find_text_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->find_controller,
			self->priv->failed_to_find_text_handler_id);
		self->priv->failed_to_find_text_handler_id = 0;
	}

	g_hash_table_remove_all (self->priv->scheme_handlers);
	g_hash_table_remove_all (self->priv->element_clicked_cbs);

	g_clear_object (&self->priv->ui_manager);
	g_clear_object (&self->priv->open_proxy);
	g_clear_object (&self->priv->print_proxy);
	g_clear_object (&self->priv->save_as_proxy);
	g_clear_object (&self->priv->font_settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_web_view_parent_class)->dispose (object);
}

static void
web_view_finalize (GObject *object)
{
	EWebView *self = E_WEB_VIEW (object);

	g_clear_pointer (&self->priv->last_popup_iframe_src, g_free);
	g_clear_pointer (&self->priv->last_popup_iframe_id, g_free);
	g_clear_pointer (&self->priv->last_popup_element_id, g_free);
	g_clear_pointer (&self->priv->last_popup_link_uri, g_free);
	g_free (self->priv->selected_uri);
	g_free (self->priv->cursor_image_src);

	while (!g_queue_is_empty (&self->priv->highlights))
		g_free (g_queue_pop_head (&self->priv->highlights));

	g_clear_pointer (&self->priv->old_settings, g_hash_table_destroy);

	g_hash_table_destroy (self->priv->scheme_handlers);
	g_hash_table_destroy (self->priv->element_clicked_cbs);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_web_view_parent_class)->finalize (object);
}

/* 'scheme' is like "file", not "file:" */
void
e_web_view_register_content_request_for_scheme (EWebView *web_view,
						const gchar *scheme,
						EContentRequest *content_request)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (E_IS_CONTENT_REQUEST (content_request));
	g_return_if_fail (scheme != NULL);

	g_hash_table_insert (web_view->priv->scheme_handlers, g_strdup (scheme), g_object_ref (content_request));

	web_view_ensure_scheme_known (webkit_web_view_get_context (WEBKIT_WEB_VIEW (web_view)), scheme);
}

static void
web_view_initialize (WebKitWebView *web_view)
{
	EContentRequest *content_request;
	GSettings *font_settings;

	content_request = e_file_request_new ();
	e_web_view_register_content_request_for_scheme (E_WEB_VIEW (web_view), "evo-file", content_request);
	g_object_unref (content_request);

	content_request = e_stock_request_new ();
	e_binding_bind_property (web_view, "scale-factor",
		content_request, "scale-factor",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
	e_web_view_register_content_request_for_scheme (E_WEB_VIEW (web_view), "gtk-stock", content_request);
	g_object_unref (content_request);

	font_settings = e_util_ref_settings ("org.gnome.desktop.interface");

	e_web_view_update_fonts_settings (font_settings, NULL, NULL, GTK_WIDGET (web_view));

	g_object_unref (font_settings);
}

static void
e_web_view_initialize_web_extensions_cb (WebKitWebContext *web_context,
					 gpointer user_data)
{
	EWebView *web_view = user_data;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_context_set_web_extensions_directory (web_context, EVOLUTION_WEB_EXTENSIONS_DIR);
}

static void
e_web_view_element_clicked_cb (WebKitUserContentManager *manager,
			       WebKitJavascriptResult *js_result,
			       gpointer user_data)
{
	EWebView *web_view = user_data;
	GtkAllocation elem_position;
	GPtrArray *listeners;
	JSCValue *jsc_object;
	gchar *iframe_id, *elem_id, *elem_class, *elem_value;
	gdouble zoom_level;

	g_return_if_fail (web_view != NULL);
	g_return_if_fail (js_result != NULL);

	jsc_object = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_object (jsc_object));

	iframe_id = e_web_view_jsc_get_object_property_string (jsc_object, "iframe-id", NULL);
	elem_id = e_web_view_jsc_get_object_property_string (jsc_object, "elem-id", NULL);
	elem_class = e_web_view_jsc_get_object_property_string (jsc_object, "elem-class", NULL);
	elem_value = e_web_view_jsc_get_object_property_string (jsc_object, "elem-value", NULL);
	elem_position.x = e_web_view_jsc_get_object_property_int32 (jsc_object, "left", 0);
	elem_position.y = e_web_view_jsc_get_object_property_int32 (jsc_object, "top", 0);
	elem_position.width = e_web_view_jsc_get_object_property_int32 (jsc_object, "width", 0);
	elem_position.height = e_web_view_jsc_get_object_property_int32 (jsc_object, "height", 0);

	zoom_level = webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (web_view));
	elem_position.x *= zoom_level;
	elem_position.y *= zoom_level;
	elem_position.width *= zoom_level;
	elem_position.height *= zoom_level;

	listeners = g_hash_table_lookup (web_view->priv->element_clicked_cbs, elem_class);

	if (listeners) {
		guint ii;

		for (ii = 0; ii < listeners->len; ii++) {
			ElementClickedData *ecd = g_ptr_array_index (listeners, ii);

			if (ecd && ecd->callback)
				ecd->callback (web_view, iframe_id, elem_id, elem_class, elem_value, &elem_position, ecd->user_data);
		}
	}

	g_free (iframe_id);
	g_free (elem_id);
	g_free (elem_class);
	g_free (elem_value);
}

static void
web_view_call_register_element_clicked (EWebView *web_view,
					const gchar *iframe_id,
					const gchar *only_elem_class)
{
	gchar *elem_classes = NULL;

	if (!only_elem_class) {
		GHashTableIter iter;
		gpointer key;
		GString *classes;

		classes = g_string_sized_new (128);

		g_hash_table_iter_init (&iter, web_view->priv->element_clicked_cbs);
		while (g_hash_table_iter_next (&iter, &key, NULL)) {
			if (classes->len)
				g_string_append_c (classes, '\n');

			g_string_append (classes, key);
		}

		elem_classes = g_string_free (classes, FALSE);
	}

	e_web_view_jsc_register_element_clicked (WEBKIT_WEB_VIEW (web_view), iframe_id,
		only_elem_class ? only_elem_class : elem_classes,
		web_view->priv->cancellable);

	g_free (elem_classes);
}

static void
e_web_view_content_loaded_cb (WebKitUserContentManager *manager,
			      WebKitJavascriptResult *js_result,
			      gpointer user_data)
{
	EWebView *web_view = user_data;
	JSCValue *jsc_value;
	gchar *iframe_id;

	g_return_if_fail (web_view != NULL);
	g_return_if_fail (js_result != NULL);

	jsc_value = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_string (jsc_value));

	iframe_id = jsc_value_to_string (jsc_value);

	if (!iframe_id || !*iframe_id)
		e_web_view_update_fonts (web_view);
	else
		e_web_view_update_styles (web_view, iframe_id);

	web_view_call_register_element_clicked (web_view, iframe_id, NULL);

	g_signal_emit (web_view, signals[CONTENT_LOADED], 0, iframe_id);

	g_free (iframe_id);
}

static void
e_web_view_has_selection_cb (WebKitUserContentManager *manager,
			     WebKitJavascriptResult *js_result,
			     gpointer user_data)
{
	EWebView *web_view = user_data;
	JSCValue *jsc_value;

	g_return_if_fail (web_view != NULL);
	g_return_if_fail (js_result != NULL);

	jsc_value = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_boolean (jsc_value));

	e_web_view_set_has_selection (web_view, jsc_value_to_boolean (jsc_value));
}

static void
e_web_view_set_need_input (EWebView *web_view,
			   gboolean need_input)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if ((!web_view->priv->need_input) == (!need_input))
		return;

	web_view->priv->need_input = need_input;

	g_object_notify (G_OBJECT (web_view), "need-input");
}

static void
e_web_view_need_input_changed_cb (WebKitUserContentManager *manager,
				  WebKitJavascriptResult *js_result,
				  gpointer user_data)
{
	EWebView *web_view = user_data;
	JSCValue *jsc_value;

	g_return_if_fail (web_view != NULL);
	g_return_if_fail (js_result != NULL);

	jsc_value = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_boolean (jsc_value));

	e_web_view_set_need_input (web_view, jsc_value_to_boolean (jsc_value));
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
web_view_init_ui (EWebView *web_view)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='context' is-popup='true'>"
		    "<item action='copy-clipboard'/>"
		    "<item action='search-web'/>"
		    "<separator/>"
		    "<placeholder id='custom-actions-1'>"
		      "<item action='open'/>"
		      "<item action='save-as'/>"
		      "<item action='http-open'/>"
		      "<item action='send-message'/>"
		      "<item action='print'/>"
		    "</placeholder>"
		    "<placeholder id='custom-actions-2'>"
		      "<item action='uri-copy'/>"
		      "<item action='mailto-copy'/>"
		      "<item action='mailto-copy-raw'/>"
		      "<item action='image-copy'/>"
		      "<item action='image-save'/>"
		    "</placeholder>"
		    "<placeholder id='custom-actions-3'/>"
		    "<separator/>"
		    "<item action='select-all'/>"
		    "<placeholder id='inspect-menu' />"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry uri_entries[] = {

		{ "uri-copy",
		  "edit-copy",
		  N_("_Copy Link Location"),
		  NULL,
		  N_("Copy the link to the clipboard"),
		  action_uri_copy_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry http_entries[] = {

		{ "http-open",
		  "emblem-web",
		  N_("_Open Link in Browser"),
		  NULL,
		  N_("Open the link in a web browser"),
		  action_http_open_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry mailto_entries[] = {

		{ "mailto-copy",
		  "edit-copy",
		  N_("_Copy Name and Email Address"),
		  NULL,
		  N_("Copy the email address with the name to the clipboard"),
		  action_mailto_copy_cb, NULL, NULL, NULL },

		{ "mailto-copy-raw",
		  "edit-copy",
		  N_("Copy Only Email Add_ress"),
		  NULL,
		  N_("Copy only the email address without the name to the clipboard"),
		  action_mailto_copy_raw_cb, NULL, NULL, NULL },

		{ "send-message",
		  "mail-message-new",
		  N_("_Send New Message To…"),
		  NULL,
		  N_("Send a mail message to this address"),
		  action_send_message_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry image_entries[] = {

		{ "image-copy",
		  "edit-copy",
		  N_("_Copy Image"),
		  NULL,
		  N_("Copy the image to the clipboard"),
		  action_image_copy_cb, NULL, NULL, NULL },

		{ "image-save",
		  "document-save",
		  N_("Save _Image…"),
		  "<Control>s",
		  N_("Save the image to a file"),
		  action_image_save_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry selection_entries[] = {

		{ "copy-clipboard",
		  "edit-copy",
		  N_("_Copy"),
		  NULL,
		  N_("Copy the selection"),
		  action_copy_clipboard_cb, NULL, NULL, NULL },

		{ "search-web",
		  NULL,
		  N_("Search _Web…"),
		  NULL,
		  N_("Search the Web with the selected text"),
		  action_search_web_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry standard_entries[] = {

		{ "open",
		  NULL,
		  N_("_Open"),
		  NULL,
		  NULL,
		  action_open_cb, NULL, NULL, NULL },

		{ "select-all",
		  "edit-select-all",
		  N_("Select _All"),
		  NULL,
		  N_("Select all text and images"),
		  action_select_all_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry lockdown_printing_entries[] = {

		{ "print",
		  "document-print",
		  N_("_Print"),
		  NULL,
		  NULL,
		  action_print_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry lockdown_save_to_disk_entries[] = {

		{ "save-as",
		  "document-save-as",
		  N_("Save _as…"),
		  NULL,
		  NULL,
		  action_save_as_cb, NULL, NULL, NULL }
	};

	EUIManager *ui_manager;
	GSettings *settings;
	gulong handler_id;
	const gchar *klass_name;

	klass_name = G_OBJECT_TYPE_NAME (web_view);
	if (klass_name && *klass_name) {
		GString *filename;
		guint ii;

		filename = g_string_new (klass_name);
		for (ii = 0; ii < filename->len; ii++) {
			const gchar chr = filename->str[ii];

			if (g_ascii_isupper (chr)) {
				filename->str[ii] = g_ascii_tolower (chr);
				if (ii > 0) {
					g_string_insert_c (filename, ii, '-');
					ii++;
				}
			}
		}

		ui_manager = e_ui_manager_new (e_ui_customizer_util_dup_filename_for_component (filename->str));

		g_string_free (filename, TRUE);
	} else {
		ui_manager = e_ui_manager_new (NULL);
	}
	web_view->priv->ui_manager = ui_manager;

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

	e_ui_manager_add_actions (ui_manager, "uri", NULL,
		uri_entries, G_N_ELEMENTS (uri_entries), web_view);
	e_ui_manager_add_actions (ui_manager, "http", NULL,
		http_entries, G_N_ELEMENTS (http_entries), web_view);
	e_ui_manager_add_actions (ui_manager, "mailto", NULL,
		mailto_entries, G_N_ELEMENTS (mailto_entries), web_view);
	e_ui_manager_add_actions (ui_manager, "image", NULL,
		image_entries, G_N_ELEMENTS (image_entries), web_view);
	e_ui_manager_add_actions (ui_manager, "selection", NULL,
		selection_entries, G_N_ELEMENTS (selection_entries), web_view);
	/* Support lockdown. */
	e_ui_manager_add_actions (ui_manager, "lockdown-printing", NULL,
		lockdown_printing_entries, G_N_ELEMENTS (lockdown_printing_entries), web_view);
	e_ui_manager_add_actions (ui_manager, "lockdown-save-to-disk", NULL,
		lockdown_save_to_disk_entries, G_N_ELEMENTS (lockdown_save_to_disk_entries), web_view);
	/* finally add actions with the .eui data */
	e_ui_manager_add_actions_with_eui_data (ui_manager, "standard", NULL,
		standard_entries, G_N_ELEMENTS (standard_entries), web_view, eui);

	web_view->priv->element_clicked_cbs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);

	web_view->priv->cancellable = NULL;

	e_ui_manager_set_action_groups_widget (ui_manager, GTK_WIDGET (web_view));
}

static void
web_view_constructed (GObject *object)
{
	WebKitSettings *web_settings;
	WebKitUserContentManager *manager;
	EWebView *web_view = E_WEB_VIEW (object);
	GSettings *settings;

	web_view_init_ui (web_view);

#ifndef G_OS_WIN32
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

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	g_settings_bind (
		settings, "webkit-minimum-font-size",
		object, "minimum-font-size",
		G_SETTINGS_BIND_GET);

	g_clear_object (&settings);

	g_signal_connect_object (webkit_web_view_get_context (WEBKIT_WEB_VIEW (web_view)), "initialize-web-extensions",
		G_CALLBACK (e_web_view_initialize_web_extensions_cb), web_view, 0);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_web_view_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	web_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (object));
	webkit_settings_set_enable_write_console_messages_to_stdout (web_settings, e_util_get_webkit_developer_mode_enabled ());

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

	web_view_set_find_controller (web_view);

	manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (object));

	g_signal_connect_object (manager, "script-message-received::elementClicked",
		G_CALLBACK (e_web_view_element_clicked_cb), web_view, 0);

	g_signal_connect_object (manager, "script-message-received::contentLoaded",
		G_CALLBACK (e_web_view_content_loaded_cb), web_view, 0);

	g_signal_connect_object (manager, "script-message-received::hasSelection",
		G_CALLBACK (e_web_view_has_selection_cb), web_view, 0);

	g_signal_connect_object (manager, "script-message-received::needInputChanged",
		G_CALLBACK (e_web_view_need_input_changed_cb), web_view, 0);

	webkit_user_content_manager_register_script_message_handler (manager, "contentLoaded");
	webkit_user_content_manager_register_script_message_handler (manager, "elementClicked");
	webkit_user_content_manager_register_script_message_handler (manager, "hasSelection");
	webkit_user_content_manager_register_script_message_handler (manager, "needInputChanged");

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_signal_connect_object (settings, "changed::composer-inline-spelling",
		G_CALLBACK (e_web_view_spell_settings_changed_cb), web_view, 0);
	g_signal_connect_object (settings, "changed::composer-spell-languages",
		G_CALLBACK (e_web_view_spell_settings_changed_cb), web_view, 0);
	e_web_view_update_spell_checking (web_view, settings);
	g_clear_object (&settings);
}

static void
e_web_view_replace_load_cancellable (EWebView *web_view,
				     gboolean create_new)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->cancellable) {
		g_cancellable_cancel (web_view->priv->cancellable);
		g_clear_object (&web_view->priv->cancellable);
	}

	if (create_new)
		web_view->priv->cancellable = g_cancellable_new ();
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
	gchar *message = e_util_get_uri_tooltip (uri);

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

	if (g_ascii_strncasecmp (uri, "open-map:", 9) == 0) {
		GUri *guri;

		guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		if (guri) {
			e_open_map_uri (parent, g_uri_get_path (guri));
			g_uri_unref (guri);
		}

		return;
	}

	e_show_uri (parent, uri);
}

static void
web_view_load_string (EWebView *web_view,
                      const gchar *string)
{
	if (!string || !*string) {
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (web_view), "", "evo-file:///");
	} else {
		GBytes *bytes;

		bytes = g_bytes_new (string, strlen (string));
		webkit_web_view_load_bytes (WEBKIT_WEB_VIEW (web_view), bytes, NULL, NULL, "evo-file:///");
		g_bytes_unref (bytes);
	}
}

static void
web_view_load_uri (EWebView *web_view,
                   const gchar *uri)
{
	if (!uri)
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

static void
web_view_before_popup_event (EWebView *web_view,
			     const gchar *uri)
{
	e_web_view_set_selected_uri (web_view, uri);
	e_web_view_update_actions (web_view);
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
e_web_view_update_action_from_proxy (EUIAction *action,
				     EUIAction *proxy)
{
	e_ui_action_set_visible (action, proxy && e_ui_action_is_visible (proxy));

	if (!e_ui_action_get_visible (action))
		return;

	e_ui_action_set_label (action, e_ui_action_get_label (proxy));
	e_ui_action_set_icon_name (action, e_ui_action_get_icon_name (proxy));
	e_ui_action_set_tooltip (action, e_ui_action_get_tooltip (proxy));
	e_ui_action_set_sensitive (action, g_action_get_enabled (G_ACTION (proxy)));
}

static void
web_view_update_actions (EWebView *web_view)
{
	EUIActionGroup *action_group;
	EUIAction *action;
	gboolean can_copy;
	gboolean scheme_is_http = FALSE;
	gboolean scheme_is_mailto = FALSE;
	gboolean uri_is_valid = FALSE;
	gboolean visible;
	const gchar *cursor_image_src;
	const gchar *uri;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	uri = e_web_view_get_selected_uri (web_view);
	can_copy = e_web_view_has_selection (web_view);
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
	visible = (uri != NULL) && !scheme_is_mailto;
	action_group = e_web_view_get_action_group (web_view, "uri");
	e_ui_action_group_set_visible (action_group, visible);

	visible = uri_is_valid && scheme_is_http;
	action_group = e_web_view_get_action_group (web_view, "http");
	e_ui_action_group_set_visible (action_group, visible);

	visible = uri_is_valid && scheme_is_mailto;
	action_group = e_web_view_get_action_group (web_view, "mailto");
	e_ui_action_group_set_visible (action_group, visible);

	if (visible) {
		CamelURL *curl;

		curl = camel_url_new (uri, NULL);
		if (curl) {
			CamelInternetAddress *inet_addr;
			const gchar *name = NULL, *email = NULL;

			inet_addr = camel_internet_address_new ();
			camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);

			action = e_ui_action_group_get_action (action_group, "mailto-copy-raw");
			e_ui_action_set_visible (action,
				camel_internet_address_get (inet_addr, 0, &name, &email) &&
				name && *name && email && *email);

			g_object_unref (inet_addr);
			camel_url_free (curl);
		}
	}

	visible = (cursor_image_src != NULL);
	action_group = e_web_view_get_action_group (web_view, "image");
	e_ui_action_group_set_visible (action_group, visible);

	visible = can_copy;
	action_group = e_web_view_get_action_group (web_view, "selection");
	e_ui_action_group_set_visible (action_group, visible);

	visible = (uri == NULL);
	action_group = e_web_view_get_action_group (web_view, "standard");
	e_ui_action_group_set_visible (action_group, visible);

	visible = (uri == NULL) && !web_view->priv->disable_printing;
	action_group = e_web_view_get_action_group (web_view, "lockdown-printing");
	e_ui_action_group_set_visible (action_group, visible);

	visible = (uri == NULL) && !web_view->priv->disable_save_to_disk;
	action_group = e_web_view_get_action_group (web_view, "lockdown-save-to-disk");
	e_ui_action_group_set_visible (action_group, visible);

	e_web_view_update_action_from_proxy (e_web_view_get_action (web_view, "open"), web_view->priv->open_proxy);
	e_web_view_update_action_from_proxy (e_web_view_get_action (web_view, "print"), web_view->priv->print_proxy);
	e_web_view_update_action_from_proxy (e_web_view_get_action (web_view, "save-as"), web_view->priv->save_as_proxy);
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
		"<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">"
		"<meta name=\"color-scheme\" content=\"light dark\">"
		"</head>"
		"<body>");

	g_string_append (
		buffer,
		"<table bgcolor='#000000' width='100%'"
		" cellpadding='1' cellspacing='0'>"
		"<tr>"
		"<td>"
		"<table bgcolor='#dddddd' width='100%' cellpadding='6' style=\"color:#000000;\">"
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
                                         EUIAction *action)
{
	gboolean can_do_command;

	can_do_command = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	e_ui_action_set_sensitive (action, can_do_command);
	g_object_unref (action);
}

static void
web_view_selectable_update_actions (ESelectable *selectable,
                                    EFocusTracker *focus_tracker,
                                    GdkAtom *clipboard_targets,
                                    gint n_clipboard_targets)
{
	EWebView *web_view;
	EUIAction *action;
	gboolean can_copy = FALSE;

	web_view = E_WEB_VIEW (selectable);

	can_copy = e_web_view_has_selection (web_view);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	e_ui_action_set_sensitive (action, can_copy);
	e_ui_action_set_tooltip (action, _("Copy the selection"));

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_CUT,
		NULL, /* cancellable */
		(GAsyncReadyCallback) web_view_can_execute_editing_command_cb,
		g_object_ref (action));
	e_ui_action_set_tooltip (action, _("Cut the selection"));

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_PASTE,
		NULL, /* cancellable */
		(GAsyncReadyCallback) web_view_can_execute_editing_command_cb,
		g_object_ref (action));
	e_ui_action_set_tooltip (action, _("Paste the clipboard"));

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	e_ui_action_set_sensitive (action, TRUE);
	e_ui_action_set_tooltip (action, _("Select all text and images"));
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
web_view_toplevel_event_after_cb (GtkWidget *widget,
				  GdkEvent *event,
				  EWebView *web_view)
{
	if (event && event->type == GDK_MOTION_NOTIFY && web_view->priv->has_hover_link &&
	    gdk_event_get_window (event) != gtk_widget_get_window (GTK_WIDGET (web_view))) {
		/* This won't reset WebKitGTK's cached link the cursor stays on, but do this,
		   instead of sending a fake signal to the WebKitGTK. */
		e_web_view_status_message (web_view, NULL);

		web_view->priv->has_hover_link = FALSE;
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

	e_web_view_status_message (E_WEB_VIEW (widget), NULL);
}

static void
e_web_view_class_init (EWebViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

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
	class->before_popup_event = web_view_before_popup_event;
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
		PROP_HAS_SELECTION,
		g_param_spec_boolean (
			"has-selection",
			"Has Selection",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_MINIMUM_FONT_SIZE,
		g_param_spec_int (
			"minimum-font-size",
			"Minimum Font Size",
			NULL,
			G_MININT, G_MAXINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_NEED_INPUT,
		g_param_spec_boolean (
			"need-input",
			"Need Input",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_OPEN_PROXY,
		g_param_spec_object (
			"open-proxy",
			"Open Proxy",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PRINT_PROXY,
		g_param_spec_object (
			"print-proxy",
			"Print Proxy",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SAVE_AS_PROXY,
		g_param_spec_object (
			"save-as-proxy",
			"Save As Proxy",
			NULL,
			E_TYPE_UI_ACTION,
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

	signals[SET_FONTS] = g_signal_new (
		"set-fonts",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EWebViewClass, set_fonts),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

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

	signals[BEFORE_POPUP_EVENT] = g_signal_new (
		"before-popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, before_popup_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1, G_TYPE_STRING);

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

	signals[CONTENT_LOADED] = g_signal_new (
		"content-loaded",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebViewClass, content_loaded),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[RESOURCE_LOADED] = g_signal_new (
		"resource-loaded",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 0, G_TYPE_NONE);
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
	web_view->priv = e_web_view_get_instance_private (web_view);

	web_view->priv->highlights_enabled = TRUE;
	web_view->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
	web_view->priv->scheme_handlers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

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
		web_view, "load-changed",
		G_CALLBACK (web_view_load_changed_cb), NULL);

	g_signal_connect (
		web_view, "style-updated",
		G_CALLBACK (style_updated_cb), NULL);

	g_signal_connect (
		web_view, "state-flags-changed",
		G_CALLBACK (style_updated_cb), NULL);
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
		"<head>"
		"<meta name=\"color-scheme\" content=\"light dark\">"
		"</head>"
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
		e_util_make_safe_filename (filename);

	return filename;
}

void
e_web_view_reload (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_replace_load_cancellable (web_view, TRUE);

	webkit_web_view_reload (WEBKIT_WEB_VIEW (web_view));
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

gboolean
e_web_view_get_need_input (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return web_view->priv->need_input;
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

EUIAction *
e_web_view_get_open_proxy (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->open_proxy;
}

void
e_web_view_set_open_proxy (EWebView *web_view,
			   EUIAction *open_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->open_proxy == open_proxy)
		return;

	if (open_proxy != NULL) {
		g_return_if_fail (E_IS_UI_ACTION (open_proxy));
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

EUIAction *
e_web_view_get_print_proxy (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->print_proxy;
}

void
e_web_view_set_print_proxy (EWebView *web_view,
			    EUIAction *print_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->print_proxy == print_proxy)
		return;

	if (print_proxy != NULL) {
		g_return_if_fail (E_IS_UI_ACTION (print_proxy));
		g_object_ref (print_proxy);
	}

	if (web_view->priv->print_proxy != NULL)
		g_object_unref (web_view->priv->print_proxy);

	web_view->priv->print_proxy = print_proxy;

	g_object_notify (G_OBJECT (web_view), "print-proxy");
}

EUIAction *
e_web_view_get_save_as_proxy (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->save_as_proxy;
}

void
e_web_view_set_save_as_proxy (EWebView *web_view,
			      EUIAction *save_as_proxy)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->save_as_proxy == save_as_proxy)
		return;

	if (save_as_proxy != NULL) {
		g_return_if_fail (E_IS_UI_ACTION (save_as_proxy));
		g_object_ref (save_as_proxy);
	}

	if (web_view->priv->save_as_proxy != NULL)
		g_object_unref (web_view->priv->save_as_proxy);

	web_view->priv->save_as_proxy = save_as_proxy;

	g_object_notify (G_OBJECT (web_view), "save-as-proxy");
}

void
e_web_view_get_last_popup_place (EWebView *web_view,
				 gchar **out_iframe_src,
				 gchar **out_iframe_id,
				 gchar **out_element_id,
				 gchar **out_link_uri)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (out_iframe_src)
		*out_iframe_src = g_strdup (web_view->priv->last_popup_iframe_src);

	if (out_iframe_id)
		*out_iframe_id = g_strdup (web_view->priv->last_popup_iframe_id);

	if (out_element_id)
		*out_element_id = g_strdup (web_view->priv->last_popup_element_id);

	if (out_link_uri)
		*out_link_uri = g_strdup (web_view->priv->last_popup_link_uri);
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
		WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE,
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

	web_view->priv->highlights_enabled = TRUE;
	web_view_update_document_highlights (web_view);
}

void
e_web_view_disable_highlights (EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	web_view->priv->highlights_enabled = FALSE;
}

EUIAction *
e_web_view_get_action (EWebView *web_view,
                       const gchar *action_name)
{
	EUIManager *ui_manager;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_web_view_get_ui_manager (web_view);

	return e_ui_manager_get_action (ui_manager, action_name);
}

EUIActionGroup *
e_web_view_get_action_group (EWebView *web_view,
                             const gchar *group_name)
{
	EUIManager *ui_manager;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_web_view_get_ui_manager (web_view);

	return e_ui_manager_get_action_group (ui_manager, group_name);
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
e_web_view_has_selection (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);

	return web_view->priv->has_selection;
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
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (web_view), "Unselect");
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

EUIManager *
e_web_view_get_ui_manager (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->ui_manager;
}

void
e_web_view_show_popup_menu (EWebView *web_view,
			    GdkEvent *event)
{
	EUIManager *ui_manager;
	GtkMenu *menu;
	GObject *ui_item;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_update_actions (web_view);

	ui_manager = e_web_view_get_ui_manager (web_view);
	ui_item = e_ui_manager_create_item (ui_manager, "context");
	menu = GTK_MENU (gtk_menu_new_from_model (G_MENU_MODEL (ui_item)));
	g_clear_object (&ui_item);

	gtk_menu_attach_to_widget (menu, GTK_WIDGET (web_view), NULL);
	e_util_connect_menu_detach_after_deactivate (menu);

	gtk_menu_popup_at_pointer (menu, event);
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
                                  PangoFontDescription *ms_font,
                                  PangoFontDescription *vw_font,
                                  GtkWidget *view_widget)
{
	gboolean clean_ms = FALSE, clean_vw = FALSE;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	gchar fsbuff[G_ASCII_DTOSTR_BUF_SIZE];
	GdkRGBA *link = NULL;
	GdkRGBA *visited = NULL;
	GString *stylesheet;
	GtkStyleContext *context;
	PangoFontDescription *ms, *vw;
	WebKitSettings *wk_settings;
	WebKitUserContentManager *manager;
	WebKitUserStyleSheet *style_sheet;

	if (!ms_font) {
		gchar *font;

		font = g_settings_get_string (
			font_settings,
			"monospace-font-name");

		ms = pango_font_description_from_string (
			(font && *font) ? font : "monospace 10");

		clean_ms = TRUE;

		g_free (font);
	} else
		ms = ms_font;

	if (!pango_font_description_get_family (ms) ||
	    !pango_font_description_get_size (ms)) {
		if (clean_ms)
			pango_font_description_free (ms);

		clean_ms = TRUE;
		ms = pango_font_description_from_string ("monospace 10");
	}

	if (!vw_font) {
		gchar *font;

		font = g_settings_get_string (
			font_settings,
			"font-name");

		vw = pango_font_description_from_string (
			(font && *font) ? font : "serif 10");

		clean_vw = TRUE;

		g_free (font);
	} else
		vw = vw_font;

	if (!pango_font_description_get_family (vw) ||
	    !pango_font_description_get_size (vw)) {
		if (clean_vw)
			pango_font_description_free (vw);

		clean_vw = TRUE;
		vw = pango_font_description_from_string ("serif 10");
	}

	stylesheet = g_string_new ("");
	g_ascii_dtostr (fsbuff, G_ASCII_DTOSTR_BUF_SIZE,
		((gdouble) pango_font_description_get_size (vw)) / PANGO_SCALE);

	g_string_append_printf (
		stylesheet,
		"body {\n"
		"  font-family: '%s';\n"
		"  font-size: %spt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n",
		pango_font_description_get_family (vw),
		fsbuff,
		pango_font_description_get_weight (vw),
		styles[pango_font_description_get_style (vw)]);

	g_string_append (stylesheet, "}\n");

	g_ascii_dtostr (fsbuff, G_ASCII_DTOSTR_BUF_SIZE,
		((gdouble) pango_font_description_get_size (ms)) / PANGO_SCALE);

	g_string_append_printf (
		stylesheet,
		"pre,code,.pre {\n"
		"  font-family: '%s';\n"
		"  font-size: %spt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n"
		"  margin: 0px;\n"
		"}\n",
		pango_font_description_get_family (ms),
		fsbuff,
		pango_font_description_get_weight (ms),
		styles[pango_font_description_get_style (ms)]);

	if (view_widget) {
		gchar *link_str, *visited_str;
		context = gtk_widget_get_style_context (view_widget);
		gtk_style_context_get_style (
			context,
			"link-color", &link,
			"visited-link-color", &visited,
			NULL);

		if (link == NULL) {
			GtkStateFlags state;

			link = g_new0 (GdkRGBA, 1);
			state = gtk_style_context_get_state (context);
			state = state & (~(GTK_STATE_FLAG_VISITED | GTK_STATE_FLAG_LINK));
			state = state | GTK_STATE_FLAG_LINK;

			gtk_style_context_save (context);
			gtk_style_context_set_state (context, state);
			gtk_style_context_get_color (context, state, link);
			gtk_style_context_restore (context);
		}

		if (visited == NULL) {
			GtkStateFlags state;

			visited = g_new0 (GdkRGBA, 1);
			state = gtk_style_context_get_state (context);
			state = state & (~(GTK_STATE_FLAG_VISITED | GTK_STATE_FLAG_LINK));
			state = state | GTK_STATE_FLAG_VISITED;

			gtk_style_context_save (context);
			gtk_style_context_set_state (context, state);
			gtk_style_context_get_color (context, state, visited);
			gtk_style_context_restore (context);
		}

		link_str = gdk_rgba_to_string (link);
		gdk_rgba_free (link);

		visited_str = gdk_rgba_to_string (visited);
		gdk_rgba_free (visited);

		g_string_append_printf (
			stylesheet,
			"span.navigable, div.navigable, p.navigable {\n"
			"  color: %s;\n"
			"}\n"
			"a {\n"
			"  color: %s;\n"
			"}\n"
			"a:visited {\n"
			"  color: %s;\n"
			"}\n",
			link_str,
			link_str,
			visited_str);

		g_free (link_str);
		g_free (visited_str);

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
			"  margin: 0 0 6px 0;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (1));

		g_string_append_printf (
			stylesheet,
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"{\n"
			"  border-color: %s;\n"
			"  margin: 0ch;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (2));

		g_string_append_printf (
			stylesheet,
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"{\n"
			"  border-color: %s;\n"
			"  margin: 0ch;\n"
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
			"  margin: 0ch;\n"
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
			"  margin: 0ch;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (5));

		g_string_append_printf (
			stylesheet,
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
			"{\n"
			"  border-color: %s;\n"
			"  padding: 0ch 0ch 0ch 1ch;\n"
			"  margin: 0ch;\n"
			"  border-width: 0px 0px 0px 2px;\n"
			"  border-style: none none none solid;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (1));
	}

	wk_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view_widget));

	g_object_set (
		wk_settings,
		"default-font-size",
		webkit_settings_font_size_to_pixels (
			pango_font_description_get_size (vw) / PANGO_SCALE),
		"default-font-family",
		pango_font_description_get_family (vw),
		"monospace-font-family",
		pango_font_description_get_family (ms),
		"default-monospace-font-size",
		webkit_settings_font_size_to_pixels (
			pango_font_description_get_size (ms) / PANGO_SCALE),
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

	e_web_view_update_styles (E_WEB_VIEW (view_widget), "*");
}

WebKitSettings *
e_web_view_get_default_webkit_settings (void)
{
	WebKitSettings *settings;

	settings = webkit_settings_new_with_settings (
		"auto-load-images", TRUE,
		"default-charset", "utf-8",
		"enable-html5-database", FALSE,
		"enable-dns-prefetching", FALSE,
		"enable-html5-local-storage", FALSE,
		"enable-java", FALSE,
		"enable-javascript", TRUE, /* Needed for JavaScriptCore API to work */
		"enable-javascript-markup", FALSE, /* Discards user-provided javascript in HTML */
		"enable-offline-web-application-cache", FALSE,
		"enable-page-cache", FALSE,
		"enable-smooth-scrolling", FALSE,
		"hardware-acceleration-policy", WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER,
		NULL);

	e_web_view_utils_apply_minimum_font_size (settings);

	return settings;
}

void
e_web_view_utils_apply_minimum_font_size (WebKitSettings *wk_settings)
{
	GSettings *settings;
	gint value;

	g_return_if_fail (WEBKIT_IS_SETTINGS (wk_settings));

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	value = g_settings_get_int (settings, "webkit-minimum-font-size");
	g_clear_object (&settings);

	if (value < 0)
		value = 0;

	if (webkit_settings_get_minimum_font_size (wk_settings) != (guint32) value)
		webkit_settings_set_minimum_font_size (wk_settings, value);
}

gint
e_web_view_get_minimum_font_size (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), -1);

	return web_view->priv->minimum_font_size;
}

void
e_web_view_set_minimum_font_size (EWebView *web_view,
				  gint pixels)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->minimum_font_size != pixels) {
		WebKitSettings *wk_settings;

		web_view->priv->minimum_font_size = pixels;

		wk_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (web_view));
		e_web_view_utils_apply_minimum_font_size (wk_settings);

		g_object_notify (G_OBJECT (web_view), "minimum-font-size");
	}
}

GCancellable *
e_web_view_get_cancellable (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return web_view->priv->cancellable;
}

void
e_web_view_update_fonts (EWebView *web_view)
{
	EWebViewClass *class;
	PangoFontDescription *ms = NULL, *vw = NULL;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	class = E_WEB_VIEW_GET_CLASS (web_view);
	g_return_if_fail (class != NULL);

	g_signal_emit (web_view, signals[SET_FONTS], 0, &ms, &vw, NULL);

	e_web_view_update_fonts_settings (
		web_view->priv->font_settings,
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
 * This function triggers an #EWebView::new-activity signal emission so
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
 * This function triggers an #EWebView::new-activity signal emission so
 * the asynchronous operation can be tracked and/or cancelled.
 **/
void
e_web_view_cursor_image_save (EWebView *web_view)
{
	GtkFileChooser *file_chooser;
	GtkFileChooserNative *native;
	GFile *destination = NULL;
	gchar *suggestion;
	gpointer toplevel;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (web_view->priv->cursor_image_src == NULL)
		return;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	toplevel = gtk_widget_is_toplevel (toplevel) ? toplevel : NULL;

	native = gtk_file_chooser_native_new (
		_("Save Image"), toplevel,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Save"), _("_Cancel"));

	file_chooser = GTK_FILE_CHOOSER (native);
	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (file_chooser, TRUE);

	suggestion = e_web_view_suggest_filename (
		web_view, web_view->priv->cursor_image_src);

	if (suggestion != NULL) {
		gtk_file_chooser_set_current_name (file_chooser, suggestion);
		g_free (suggestion);
	}

	e_util_load_file_chooser_folder (file_chooser);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		e_util_save_file_chooser_folder (file_chooser);

		destination = gtk_file_chooser_get_file (file_chooser);
	}

	g_object_unref (native);

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

static void
web_view_content_request_processed_cb (GObject *source_object,
				       GAsyncResult *result,
				       gpointer user_data)
{
	AsyncContext *async_context = user_data;
	GTask *task = g_steal_pointer (&async_context->task);
	gint64 stream_length = -1;
	gchar *mime_type = NULL;
	GError *local_error = NULL;

	if (!e_content_request_process_finish (E_CONTENT_REQUEST (source_object), result,
		&async_context->input_stream, &stream_length, &mime_type, &local_error)) {
		g_task_return_error (task, local_error);
	} else {
		g_task_return_boolean (task, TRUE);
	}

	g_free (mime_type);
	g_clear_object (&task);
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
	GHashTableIter iter;
	GTask *task;
	gboolean is_processed = FALSE;
	gpointer value;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (uri != NULL);

	g_hash_table_iter_init (&iter, web_view->priv->scheme_handlers);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		EContentRequest *adept = value;

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
		async_context->task = g_object_ref (task);
		e_content_request_process (async_context->content_request,
			async_context->uri,
			G_OBJECT (web_view),
			cancellable,
			web_view_content_request_processed_cb,
			async_context);
		is_processed = TRUE;

	/* Handle base64-encoded "data:" URIs manually */
	} else if (g_ascii_strncasecmp (uri, "data:", 5) == 0) {
		/* data:[<mime type>][;charset=<charset>][;base64],<encoded data> */
		const gchar *ptr, *from;
		gboolean is_base64 = FALSE;

		ptr = uri + 5;
		from = ptr;
		while (*ptr && *ptr != ',') {
			ptr++;

			if (*ptr == ',' || *ptr == ';') {
				if (g_ascii_strncasecmp (from, "base64", ptr - from) == 0)
					is_base64 = TRUE;

				from = ptr + 1;
			}
		}

		if (is_base64 && *ptr == ',') {
			guchar *data;
			gsize len = 0;

			data = g_base64_decode (ptr + 1, &len);

			if (data && len > 0) {
				async_context->input_stream = g_memory_input_stream_new_from_data (data, len, g_free);
				g_task_return_boolean (task, TRUE);
				is_processed = TRUE;
			} else {
				g_free (data);
			}
		}
	}

	if (!is_processed) {
		GString *shorten_uri = NULL;
		gint len;

		len = g_utf8_strlen (uri, -1);

		/* The "data:" URIs can be quite long */
		if (len > 512) {
			const gchar *ptr = g_utf8_offset_to_pointer (uri, 512);

			shorten_uri = g_string_sized_new (ptr - uri + 16);
			g_string_append_len (shorten_uri, uri, ptr - uri);
			g_string_append (shorten_uri, _("…"));
		}

		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
			_("Cannot get URI “%s”, do not know how to download it."), shorten_uri ? shorten_uri->str : uri);

		if (shorten_uri)
			g_string_free (shorten_uri, TRUE);
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
 * e_web_view_set_iframe_src:
 * @web_view: an #EWebView
 * @document_uri: a document URI for whose IFrame change the source
 * @src_uri: the source to change the IFrame to
 *
 * Change IFrame source for the given @document_uri IFrame
 * to the @new_iframe_src.
 *
 * Since: 3.22
 **/
void
e_web_view_set_iframe_src (EWebView *web_view,
			   const gchar *iframe_id,
			   const gchar *src_uri)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), web_view->priv->cancellable,
		"Evo.SetIFrameSrc(%s, %s);",
		iframe_id, src_uri);
}

/**
 * EWebViewElementClickedFunc:
 * @web_view: an #EWebView
 * @iframe_id: an iframe ID in which the click happened; empty string for the main frame
 * @element_id: an element ID
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
				web_view_call_register_element_clicked (web_view, "*", element_class);
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
	web_view_call_register_element_clicked (web_view, "*", NULL);
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
	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id && *element_id);

	e_web_view_jsc_set_element_hidden (WEBKIT_WEB_VIEW (web_view),
		"*", element_id, hidden,
		web_view->priv->cancellable);
}

void
e_web_view_set_element_style_property (EWebView *web_view,
				       const gchar *element_id,
				       const gchar *property_name,
				       const gchar *value)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id && *element_id);
	g_return_if_fail (property_name && *property_name);

	e_web_view_jsc_set_element_style_property (WEBKIT_WEB_VIEW (web_view),
		"*", element_id, property_name, value,
		web_view->priv->cancellable);
}

void
e_web_view_set_element_attribute (EWebView *web_view,
				  const gchar *element_id,
				  const gchar *namespace_uri,
				  const gchar *qualified_name,
				  const gchar *value)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id && *element_id);
	g_return_if_fail (qualified_name && *qualified_name);

	e_web_view_jsc_set_element_attribute (WEBKIT_WEB_VIEW (web_view),
		"*", element_id, namespace_uri, qualified_name, value,
		web_view->priv->cancellable);
}
