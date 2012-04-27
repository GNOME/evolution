/*
 * e-mail-display.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "e-mail-display.h"

#include <glib/gi18n.h>
#include <gdk/gdk.h>

#include "e-util/e-marshal.h"
#include "e-util/e-util.h"
#include "e-util/e-plugin-ui.h"
#include "e-util/e-file-request.h"
#include "e-util/e-stock-request.h"
#include "mail/em-composer-utils.h"
#include "mail/em-utils.h"
#include "mail/e-mail-request.h"
#include "mail/e-http-request.h"
#include "mail/em-format-html-display.h"
#include "mail/e-mail-attachment-bar.h"
#include "widgets/misc/e-attachment-button.h"

#include <camel/camel.h>

#include <JavaScriptCore/JavaScript.h>

#define d(x)

G_DEFINE_TYPE (EMailDisplay, e_mail_display, E_TYPE_WEB_VIEW)

#define E_MAIL_DISPLAY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplayPrivate))

struct _EMailDisplayPrivate {
	EMFormatHTML *formatter;

	EMFormatWriteMode mode;
	gboolean headers_collapsable;
	gboolean headers_collapsed;

	GtkActionGroup *mailto_actions;
        GtkActionGroup *images_actions;

        gint force_image_load: 1;

	GSettings *settings;
};

enum {
	PROP_0,
	PROP_FORMATTER,
	PROP_MODE,
	PROP_HEADERS_COLLAPSABLE,
	PROP_HEADERS_COLLAPSED,
};

static gpointer parent_class;

static CamelDataCache *emd_global_http_cache = 0;

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions-1'>"
"      <menuitem action='add-to-address-book'/>"
"      <menuitem action='send-reply'/>"
"    </placeholder>"
"    <placeholder name='custom-actions-3'>"
"      <menu action='search-folder-menu'>"
"        <menuitem action='search-folder-recipient'/>"
"        <menuitem action='search-folder-sender'/>"
"      </menu>"
"    </placeholder>"
"  </popup>"
"</ui>";

static const gchar *image_ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions-2'>"
"      <menuitem action='image-save'/>"
"    </placeholder>"
"  </popup>"
"</ui>";

static GtkActionEntry mailto_entries[] = {

	{ "add-to-address-book",
	  "contact-new",
	  N_("_Add to Address Book..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  NULL   /* Handled by EMailReader */ },

	{ "search-folder-recipient",
	  NULL,
	  N_("_To This Address"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  NULL   /* Handled by EMailReader */ },

	{ "search-folder-sender",
	  NULL,
	  N_("_From This Address"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  NULL   /* Handled by EMailReader */ },

	{ "send-reply",
	  NULL,
	  N_("Send _Reply To..."),
	  NULL,
	  N_("Send a reply message to this address"),
	  NULL   /* Handled by EMailReader */ },

	/*** Menus ***/

	{ "search-folder-menu",
	  "folder-saved-search",
	  N_("Create Search _Folder"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkActionEntry image_entries[] = {

        { "image-save",
        GTK_STOCK_SAVE,
        N_("Save _Image..."),
        NULL,
        N_("Save the image to a file"),
        NULL    /* Handled by EMailReader */ },

};

static void
mail_display_webview_update_actions (EWebView *web_view,
                                     gpointer user_data)
{
	const gchar *image_src;
	gboolean visible;
	GtkAction *action;

	g_return_if_fail (web_view != NULL);

	image_src = e_web_view_get_cursor_image_src (web_view);
	visible = image_src && g_str_has_prefix (image_src, "cid:");
	if (!visible && image_src) {
		CamelStream *image_stream;

		image_stream = camel_data_cache_get (emd_global_http_cache, "http", image_src, NULL);

		visible = image_stream != NULL;

		if (image_stream)
			g_object_unref (image_stream);
	}

	action = e_web_view_get_action (web_view, "image-save");
	if (action)
		gtk_action_set_visible (action, visible);
}

static void
formatter_image_loading_policy_changed_cb (GObject *object,
                                           GParamSpec *pspec,
                                           gpointer user_data)
{
	EMailDisplay *display = user_data;

	e_mail_display_load_images (display);
}

static void
mail_display_update_formatter_colors (EMailDisplay *display)
{
	EMFormatHTMLColorType type;
	EMFormatHTML *formatter;
	GdkColor *color;
	GtkStateType state;
	GtkStyle *style;

	state = gtk_widget_get_state (GTK_WIDGET (display));
	formatter = display->priv->formatter;

	if (!display->priv->formatter)
		return;

	style = gtk_widget_get_style (GTK_WIDGET (display));
	if (style == NULL)
		return;

	g_object_freeze_notify (G_OBJECT (formatter));

	color = &style->bg[state];
	type = EM_FORMAT_HTML_COLOR_BODY;
	em_format_html_set_color (formatter, type, color);

	color = &style->base[GTK_STATE_NORMAL];
	type = EM_FORMAT_HTML_COLOR_CONTENT;
	em_format_html_set_color (formatter, type, color);

	color = &style->dark[state];
	type = EM_FORMAT_HTML_COLOR_FRAME;
	em_format_html_set_color (formatter, type, color);

	color = &style->fg[state];
	type = EM_FORMAT_HTML_COLOR_HEADER;
	em_format_html_set_color (formatter, type, color);

	color = &style->text[state];
	type = EM_FORMAT_HTML_COLOR_TEXT;
	em_format_html_set_color (formatter, type, color);

	g_object_thaw_notify (G_OBJECT (formatter));
}

static void
mail_display_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FORMATTER:
			e_mail_display_set_formatter (
				E_MAIL_DISPLAY (object),
				g_value_get_object (value));
			return;
		case PROP_MODE:
			e_mail_display_set_mode (
				E_MAIL_DISPLAY (object),
				g_value_get_int (value));
			return;
		case PROP_HEADERS_COLLAPSABLE:
			e_mail_display_set_headers_collapsable (
				E_MAIL_DISPLAY (object),
				g_value_get_boolean (value));
			return;
		case PROP_HEADERS_COLLAPSED:
			e_mail_display_set_headers_collapsed (
				E_MAIL_DISPLAY (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_display_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FORMATTER:
			g_value_set_object (
				value, e_mail_display_get_formatter (
				E_MAIL_DISPLAY (object)));
			return;
		case PROP_MODE:
			g_value_set_int (
				value, e_mail_display_get_mode (
				E_MAIL_DISPLAY (object)));
			return;
		case PROP_HEADERS_COLLAPSABLE:
			g_value_set_boolean (
				value, e_mail_display_get_headers_collapsable (
				E_MAIL_DISPLAY (object)));
			return;
		case PROP_HEADERS_COLLAPSED:
			g_value_set_boolean (
				value, e_mail_display_get_headers_collapsed (
				E_MAIL_DISPLAY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_display_dispose (GObject *object)
{
	EMailDisplayPrivate *priv;

	priv = E_MAIL_DISPLAY_GET_PRIVATE (object);

	if (priv->formatter) {
		g_object_unref (priv->formatter);
		priv->formatter = NULL;
	}

	if (priv->settings) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_display_realize (GtkWidget *widget)
{
	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	mail_display_update_formatter_colors (E_MAIL_DISPLAY (widget));
}

static void
mail_display_style_set (GtkWidget *widget,
                        GtkStyle *previous_style)
{
	EMailDisplay *display = E_MAIL_DISPLAY (widget);

	mail_display_update_formatter_colors (display);

	/* Chain up to parent's style_set() method. */
	GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);
}

static gboolean
mail_display_process_mailto (EWebView *web_view,
                             const gchar *mailto_uri,
                             gpointer user_data)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);
	g_return_val_if_fail (mailto_uri != NULL, FALSE);

	if (g_ascii_strncasecmp (mailto_uri, "mailto:", 7) == 0) {
		EMFormat *format;
		CamelFolder *folder = NULL;
		EShell *shell;

		format = (EMFormat *) E_MAIL_DISPLAY (web_view)->priv->formatter;

		if (format != NULL && format->folder != NULL)
			folder = format->folder;

		shell = e_shell_get_default ();
		em_utils_compose_new_message_with_mailto (
			shell, mailto_uri, folder);

		return TRUE;
	}

	return FALSE;
}

static gboolean
mail_display_link_clicked (WebKitWebView *web_view,
                           WebKitWebFrame *frame,
                           WebKitNetworkRequest *request,
                           WebKitWebNavigationAction *navigation_action,
                           WebKitWebPolicyDecision *policy_decision,
                           gpointer user_data)
{
	EMailDisplay *display;
	const gchar *uri = webkit_network_request_get_uri (request);

	if (g_str_has_prefix (uri, "file://")) {
		gchar *filename;
		filename = g_filename_from_uri (uri, NULL, NULL);

		if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
			webkit_web_policy_decision_ignore (policy_decision);
			webkit_network_request_set_uri (request, "about:blank");
			return TRUE;
		}

		g_free (filename);
	}

	display = E_MAIL_DISPLAY (web_view);
	if (display->priv->formatter == NULL)
		return FALSE;

	if (mail_display_process_mailto (E_WEB_VIEW (web_view), uri, NULL)) {
		/* do nothing, function handled the "mailto:" uri already */
		webkit_web_policy_decision_ignore (policy_decision);
		return TRUE;

	} else if (g_ascii_strncasecmp (uri, "thismessage:", 12) == 0) {
		/* ignore */ ;
		webkit_web_policy_decision_ignore (policy_decision);
		return TRUE;

	} else if (g_ascii_strncasecmp (uri, "cid:", 4) == 0) {
		/* ignore */ ;
		webkit_web_policy_decision_ignore (policy_decision);
		return TRUE;

	}

	/* Let webkit handle it */
	return FALSE;
}

static void
webkit_request_load_from_file (WebKitNetworkRequest *request,
                               const gchar *path)
{
	gchar *data = NULL;
	gsize length = 0;
	gboolean status;
	gchar *b64, *new_uri;
	gchar *ct;

	status = g_file_get_contents (path, &data, &length, NULL);
	if (!status)
		return;

	b64 = g_base64_encode ((guchar *) data, length);
	ct = g_content_type_guess (path, NULL, 0, NULL);

	new_uri =  g_strdup_printf ("data:%s;base64,%s", ct, b64);
	webkit_network_request_set_uri (request, new_uri);

	g_free (b64);
	g_free (new_uri);
	g_free (ct);
	g_free (data);
}

static void
mail_display_resource_requested (WebKitWebView *web_view,
                                 WebKitWebFrame *frame,
                                 WebKitWebResource *resource,
                                 WebKitNetworkRequest *request,
                                 WebKitNetworkResponse *response,
                                 gpointer user_data)
{
	EMailDisplay *display = E_MAIL_DISPLAY (web_view);
	EMFormat *formatter = EM_FORMAT (display->priv->formatter);
	const gchar *uri = webkit_network_request_get_uri (request);

	if (!formatter) {
		webkit_network_request_set_uri (request, "about:blank");
		return;
	}

        /* Redirect cid:part_id to mail://mail_id/cid:part_id */
	if (g_str_has_prefix (uri, "cid:")) {

		/* Always write raw content of CID object */
		gchar *new_uri = em_format_build_mail_uri (formatter->folder,
			formatter->message_uid,
			"part_id", G_TYPE_STRING, uri,
			"mode", G_TYPE_INT, EM_FORMAT_WRITE_MODE_RAW, NULL);

		webkit_network_request_set_uri (request, new_uri);

		g_free (new_uri);

        /* WebKit won't allow to load a local file when displaing "remote" mail://,
	   protocol, so we need to handle this manually */
	} else if (g_str_has_prefix (uri, "file:")) {
		gchar *path;

		path = g_filename_from_uri (uri, NULL, NULL);
		if (!path)
			return;

		webkit_request_load_from_file (request, path);

		g_free (path);

        /* Redirect http(s) request to evo-http(s) protocol. See EMailRequest for
         * further details about this. */
	} else if (g_str_has_prefix (uri, "http:") || g_str_has_prefix (uri, "https")) {

		gchar *new_uri, *mail_uri, *enc;
		SoupURI *soup_uri;
		GHashTable *query;
		gchar *uri_md5;
		CamelStream *stream;

                /* Open Evolution's cache */
		uri_md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
		stream = camel_data_cache_get (
				emd_global_http_cache, "http", uri_md5, NULL);
		g_free (uri_md5);

                /* If the URI is not cached and we are not allowed to load it
                 * then redirect to invalid URI, so that webkit would display
                 * a native placeholder for it. */
		if (!stream && !display->priv->force_image_load &&
		    !em_format_html_can_load_images (display->priv->formatter)) {
			webkit_network_request_set_uri (request, "about:blank");
			return;
		}

		new_uri = g_strconcat ("evo-", uri, NULL);
		mail_uri = em_format_build_mail_uri (formatter->folder,
				formatter->message_uid, NULL, NULL);

		soup_uri = soup_uri_new (new_uri);
		if (soup_uri->query) {
			query = soup_form_decode (soup_uri->query);
		} else {
			query = g_hash_table_new_full (g_str_hash, g_str_equal,
						       g_free, g_free);
		}
		enc = soup_uri_encode (mail_uri, NULL);
		g_hash_table_insert (query, g_strdup ("__evo-mail"), enc);

		if (display->priv->force_image_load) {
			g_hash_table_insert (query,
				g_strdup ("__evo-load-images"),
					     g_strdup ("true"));
		}

		g_free (mail_uri);

		soup_uri_set_query_from_form (soup_uri, query);
		g_free (new_uri);

		new_uri = soup_uri_to_string (soup_uri, FALSE);
		webkit_network_request_set_uri (request, new_uri);

		g_free (new_uri);
		soup_uri_free (soup_uri);
		g_hash_table_unref (query);
	}
}

static WebKitDOMElement *
find_element_by_id (WebKitDOMDocument *document,
                    const gchar *id)
{
	WebKitDOMNodeList *frames;
	WebKitDOMElement *element;
	gulong i, length;

        /* Try to look up the element in this DOM document */
	element = webkit_dom_document_get_element_by_id (document, id);
	if (element)
		return element;

        /* If the element is not here then recursively scan all frames */
	frames = webkit_dom_document_get_elements_by_tag_name(document, "iframe");
	length = webkit_dom_node_list_get_length (frames);
	for (i = 0; i < length; i++) {

		WebKitDOMHTMLIFrameElement *iframe =
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (
				webkit_dom_node_list_item (frames, i));

		WebKitDOMDocument *frame_doc =
			webkit_dom_html_iframe_element_get_content_document (iframe);

		WebKitDOMElement *el =
			find_element_by_id (frame_doc, id);

		if (el)
			return el;
	}

	return NULL;
}

static void
mail_display_plugin_widget_resize (GObject *object,
                                   gpointer dummy,
                                   EMailDisplay *display)
{
	GtkWidget *widget;
	WebKitDOMElement *parent_element;
	gchar *dim;
	gint height;

	widget = GTK_WIDGET (object);
	gtk_widget_get_preferred_height (widget, &height, NULL);
	parent_element = g_object_get_data (object, "parent_element");

	if (!parent_element || !WEBKIT_DOM_IS_ELEMENT (parent_element)) {
		d(printf("%s: %s does not have (valid) parent element!\n",
			G_STRFUNC, (gchar *) g_object_get_data (object, "uri")));
		return;
	}

        /* Int -> Str */
	dim = g_strdup_printf ("%d", height);

        /* Set height of the containment <object> to match height of the
         * GtkWidget it contains */
	webkit_dom_html_object_element_set_height (
		WEBKIT_DOM_HTML_OBJECT_ELEMENT (parent_element), dim);
	g_free (dim);
}

static void
mail_display_plugin_widget_realize_cb (GtkWidget *widget,
                                       gpointer user_data)
{
        /* Initial resize of the <object> element when the widget
         * is displayed for the first time. */
	mail_display_plugin_widget_resize (G_OBJECT (widget), NULL, user_data);
}

static void
plugin_widget_set_parent_element (GtkWidget *widget,
                                  EMailDisplay *display)
{
	const gchar *uri;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	uri = g_object_get_data (G_OBJECT (widget), "uri");
	if (!uri || !*uri)
		return;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (display));
	element = find_element_by_id (document, uri);

	if (!element || !WEBKIT_DOM_IS_ELEMENT (element)) {
		g_warning ("Failed to find parent <object> for '%s' - no ID set?", uri);
		return;
	}

        /* Assign the WebKitDOMElement to "parent_element" data of the GtkWidget
         * and the GtkWidget to "widget" data of the DOM Element */
	g_object_set_data (G_OBJECT (widget), "parent_element", element);
	g_object_set_data (G_OBJECT (element), "widget", widget);
}

static void
attachment_button_expanded (GObject *object,
                            GParamSpec *pspec,
                            gpointer user_data)
{
	EAttachmentButton *button = E_ATTACHMENT_BUTTON (object);
	WebKitDOMElement *attachment = user_data;
	WebKitDOMCSSStyleDeclaration *css;
	gboolean expanded;

	d(printf("Attachment button %s (%p) expansion state toggled!\n",
		(gchar *) g_object_get_data (object, "uri"), object));

	expanded = e_attachment_button_get_expanded (button) &&
			gtk_widget_get_visible (GTK_WIDGET (button));

	if (!WEBKIT_DOM_IS_ELEMENT (attachment)) {
		d(printf("%s: Parent element for button %s does not exist!\n",
			G_STRFUNC, (gchar *) g_object_get_data (object, "uri")));
		return;
	}

        /* Show or hide the DIV which contains the attachment (iframe, image...) */
	css = webkit_dom_element_get_style (attachment);
	webkit_dom_css_style_declaration_set_property (
		css, "display", expanded ? "block" : "none", "", NULL);
}

static void
constraint_widget_visibility (GObject *object,
                              GParamSpec *pspec,
                              gpointer user_data)
{
	GtkWidget *widget = GTK_WIDGET (object);
	EAttachmentButton *button = user_data;

	gboolean can_show = e_attachment_button_get_expanded (button);
	gboolean is_visible = gtk_widget_get_visible (widget);

	if (is_visible && !can_show)
		gtk_widget_hide (widget);
	else if (!is_visible && can_show)
		gtk_widget_show (widget);

        /* Otherwise it's OK */
}

static void
bind_iframe_content_visibility (EAttachmentButton *button,
                                WebKitDOMElement *iframe)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *nodes;
	gulong i, length;

	if (!WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (iframe))
		return;

	document = webkit_dom_html_iframe_element_get_content_document (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));
	nodes = webkit_dom_document_get_elements_by_tag_name (document, "object");
	length = webkit_dom_node_list_get_length (nodes);

	d(printf("Found %ld objects within iframe %s\n", length,
		webkit_dom_html_iframe_element_get_name (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe))));

        /* Iterate through all <object>s and bind visibility of their widget
         * with expanded-state of related attachment button */
	for (i = 0; i < length; i++) {

		WebKitDOMNode *node = webkit_dom_node_list_item (nodes, i);
		GtkWidget *widget;

		widget = g_object_get_data (G_OBJECT (node), "widget");
		if (!widget)
			continue;

		d(printf("Binding visibility of widget %s (%p) with button %s (%p)\n",
			(gchar *) g_object_get_data (G_OBJECT (widget), "uri"), widget,
			(gchar *) g_object_get_data (G_OBJECT (button), "uri"), button));

		g_object_bind_property (
			button, "expanded",
			widget, "visible",
			G_BINDING_SYNC_CREATE);

                /* Ensure that someone won't attempt to _show() the widget when
                 * it is supposed to be hidden and vice versa. */
		g_signal_connect (widget, "notify::visible",
			G_CALLBACK (constraint_widget_visibility), button);
	}
}

static void
bind_attachment_iframe_visibility (GObject *object,
                                   GParamSpec *pspec,
                                   gpointer user_data)
{
	WebKitWebFrame *webframe;
	const gchar *frame_name;
	gchar *button_uri;
	WebKitDOMDocument *document;
	WebKitDOMElement *attachment;
	WebKitDOMElement *button_element;
	WebKitDOMNodeList *nodes;
	gulong i, length;
	GtkWidget *button;

        /* Whenever an <iframe> is loaded, bind visibility of all GtkWidgets
         * the document within the <iframe> contains with "expanded" property
         * of the EAttachmentButton */

	webframe = WEBKIT_WEB_FRAME (object);
	if (webkit_web_frame_get_load_status (webframe) != WEBKIT_LOAD_FINISHED)
		return;

	frame_name = webkit_web_frame_get_name (webframe);

	d(printf("Rebinding visibility of frame %s because it's URL changed\n",
		 frame_name));

        /* Get DOMDocument of the main document */
	document = webkit_web_view_get_dom_document (
		webkit_web_frame_get_web_view (webframe));
	if (!document)
		return;

        /* Find the <DIV> containing the <iframe> and related EAttachmentButton
         * within the DOM */
	attachment = find_element_by_id (document, frame_name);
	if (!attachment)
		return;

	button_uri = g_strconcat (frame_name, ".attachment_button", NULL);
	button_element = find_element_by_id (document, button_uri);
	g_free (button_uri);
	if (!button_element)
		return;

	button = g_object_get_data (G_OBJECT (button_element), "widget");

        /* Get <iframe> representing the attachment content */
	nodes = webkit_dom_element_get_elements_by_tag_name (attachment, "iframe");
	length = webkit_dom_node_list_get_length (nodes);
	for (i = 0; i < length; i++) {

		WebKitDOMNode *node =
			webkit_dom_node_list_item (nodes, i);

		if (!WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (node))
			continue;

                /* Bind visibility of all GtkWidget within the
                 * iframe with "expanded" property of the button */
		bind_iframe_content_visibility (
			E_ATTACHMENT_BUTTON (button),
			WEBKIT_DOM_ELEMENT (node));
	}
}

static GtkWidget *
mail_display_plugin_widget_requested (WebKitWebView *web_view,
                                      gchar *mime_type,
                                      gchar *uri,
                                      GHashTable *param,
                                      gpointer user_data)
{
	EMFormat *emf;
	EMailDisplay *display;
	EMFormatPURI *puri;
	GtkWidget *widget;
	gchar *puri_uri;

	puri_uri = g_hash_table_lookup (param, "data");
	if (!puri_uri || !g_str_has_prefix (uri, "mail://"))
		return NULL;

	display = E_MAIL_DISPLAY (web_view);
	emf = (EMFormat *) display->priv->formatter;

	puri = em_format_find_puri (emf, puri_uri);
	if (!puri) {
		return NULL;
	}

	if (puri->widget_func)
		widget = puri->widget_func (emf, puri, NULL);
	else
		widget = NULL;

	if (!widget)
		return NULL;

	if (E_IS_ATTACHMENT_BUTTON (widget)) {
                /* Attachment button has URI different then the actual PURI because
                 * that URI identifies the attachment itself */
		gchar *button_uri = g_strconcat (puri_uri, ".attachment_button", NULL);
		g_object_set_data_full (G_OBJECT (widget), "uri",
			button_uri, (GDestroyNotify) g_free);
	} else {
		g_object_set_data_full (G_OBJECT (widget), "uri",
			g_strdup (puri_uri), (GDestroyNotify) g_free);
	}

        /* Set widget's <object> container as GObject data "parent_element" */
	plugin_widget_set_parent_element (widget, display);

        /* Resizing a GtkWidget requires changing size of parent
         * <object> HTML element in DOM. */
	g_signal_connect (widget, "realize",
			  G_CALLBACK (mail_display_plugin_widget_realize_cb), display);
	g_signal_connect (widget, "size-allocate",
			  G_CALLBACK (mail_display_plugin_widget_resize), display);

        /* Embed the attachment bar into the GtkBox before we do anything
         * further with the widget. */
	if (E_IS_MAIL_ATTACHMENT_BAR (widget)) {

                /* When EMailAttachmentBar is expanded/collapsed it does not
                 * emit size-allocate signal despite it changes it's height. */
		GtkWidget *box = NULL;

                /* Only when packed in box, EMailAttachmentBar reports correct 
                 * height */
		box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

		g_signal_connect (widget, "notify::expanded",
			G_CALLBACK (mail_display_plugin_widget_resize), display);
		g_signal_connect (widget, "notify::active-view",
			G_CALLBACK (mail_display_plugin_widget_resize), display);

                /* Show the EAttachmentBar but not the containing layout */
		gtk_widget_show (widget);

		widget = box;

	} else if (E_IS_ATTACHMENT_BUTTON (widget)) {

                /* Bind visibility of DOM element containing related
                 * attachment with 'expanded' property of this
                 * attachment button. */
		WebKitDOMElement *attachment;
		WebKitDOMDocument *document;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (display));
		attachment = find_element_by_id (document, puri_uri);
		if (!attachment) {
			e_attachment_button_set_expandable (
				E_ATTACHMENT_BUTTON (widget), FALSE);
		} else {
			const CamelContentDisposition *disposition;
			WebKitDOMNodeList *nodes;
			gulong i, length;

                        /* Show/hide the attachment when the EAttachmentButton
                         * is expanded/collapsed or shown/hidden */
			g_signal_connect_data (widget, "notify::expanded",
				G_CALLBACK (attachment_button_expanded),
				g_object_ref (attachment), (GClosureNotify) g_object_unref, 0);
			g_signal_connect_data (widget, "notify::visible",
				G_CALLBACK (attachment_button_expanded),
				g_object_ref (attachment), (GClosureNotify) g_object_unref, 0);
                        /* Initial synchronization */
			attachment_button_expanded (G_OBJECT (widget),
				NULL, attachment);

                        /* Find all <iframes> within the attachment and bind
                         * it's visiblity to expanded state of the attachment btn */
			nodes = webkit_dom_element_get_elements_by_tag_name (
					attachment, "iframe");
			length = webkit_dom_node_list_get_length (nodes);
			for (i = 0; i < length; i++) {

				WebKitDOMNode *node =
					webkit_dom_node_list_item (nodes, i);

				if (!WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (node))
					continue;

				bind_iframe_content_visibility (
					E_ATTACHMENT_BUTTON (widget),
					WEBKIT_DOM_ELEMENT (node));
			}

                        /* Expand inlined attachments */
			disposition =
				camel_mime_part_get_content_disposition (puri->part);
			if (disposition &&
			    g_ascii_strncasecmp (
				disposition->disposition, "inline", 6) == 0) {

				e_attachment_button_set_expanded (
					E_ATTACHMENT_BUTTON (widget), TRUE);
			}
		}
	}

	d(printf("Created widget %s (%p)\n", puri_uri, widget));
	return widget;
}

static void
toggle_headers_visibility (WebKitDOMElement *button,
                           WebKitDOMEvent *event,
                           WebKitWebView *web_view)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *short_headers, *full_headers;
	WebKitDOMCSSStyleDeclaration *css_short, *css_full;
	gboolean expanded;
	const gchar *path;

	document = webkit_web_view_get_dom_document (web_view);

	short_headers = webkit_dom_document_get_element_by_id (
		document, "__evo-short-headers");
	if (!short_headers)
		return;

	css_short = webkit_dom_element_get_style (short_headers);

	full_headers = webkit_dom_document_get_element_by_id (
		document, "__evo-full-headers");
	if (!full_headers)
		return;

	css_full = webkit_dom_element_get_style (full_headers);

	expanded = (g_strcmp0 (webkit_dom_css_style_declaration_get_property_value (
			css_full, "display"), "block") == 0);

	webkit_dom_css_style_declaration_set_property (css_full, "display",
		expanded ? "none" : "block", "", NULL);
	webkit_dom_css_style_declaration_set_property (css_short, "display",
		expanded ? "block" : "none", "", NULL);

	if (expanded)
		path = "evo-file://" EVOLUTION_IMAGESDIR "/plus.png";
	else
		path = "evo-file://" EVOLUTION_IMAGESDIR "/minus.png";

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (button), path);

	e_mail_display_set_headers_collapsed (E_MAIL_DISPLAY (web_view), expanded);

	d(printf("Headers %s!\n", expanded ? "collapsed" : "expanded"));
}

static const gchar* addresses[] = { "to", "cc", "bcc" };

static void
toggle_address_visibility (WebKitDOMElement *button,
                           WebKitDOMEvent *event,
                           const gchar *address)
{
	WebKitDOMElement *full_addr, *ellipsis;
	WebKitDOMCSSStyleDeclaration *css_full, *css_ellipsis;
	WebKitDOMDocument *document;
	gchar *id;
	const gchar *path;
	gboolean expanded;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (button));

	id = g_strconcat ("__evo-moreaddr-", address, NULL);
	full_addr = webkit_dom_document_get_element_by_id (document, id);
	g_free (id);

	if (!full_addr)
		return;

	css_full = webkit_dom_element_get_style (full_addr);

	id = g_strconcat ("__evo-moreaddr-ellipsis-", address, NULL);
	ellipsis = webkit_dom_document_get_element_by_id (document, id);
	g_free (id);

	if (!ellipsis)
		return;

	css_ellipsis = webkit_dom_element_get_style (ellipsis);

	expanded = (g_strcmp0 (
		webkit_dom_css_style_declaration_get_property_value (
		css_full, "display"), "inline") == 0);

	webkit_dom_css_style_declaration_set_property (
		css_full, "display", (expanded ? "none" : "inline"), "", NULL);
	webkit_dom_css_style_declaration_set_property (
		css_ellipsis, "display", (expanded ? "inline" : "none"), "", NULL);

	if (expanded) {
		path = "evo-file://" EVOLUTION_IMAGESDIR "/plus.png";
	} else {
		path = "evo-file://" EVOLUTION_IMAGESDIR "/minus.png";
	}

	if (!WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (button)) {
		id = g_strconcat ("__evo-moreaddr-img-", address, NULL);
		button = webkit_dom_document_get_element_by_id (document, id);
		g_free (id);

		if (!button)
			return;
	}

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (button), path);

}

static void
setup_DOM_bindings (GObject *object,
                    GParamSpec *pspec,
                    gpointer user_data)
{
	WebKitWebView *web_view;
	WebKitWebFrame *frame;
	WebKitLoadStatus load_status;
	WebKitDOMDocument *document;
	WebKitDOMElement *button;
	gint i = 0;

	frame = WEBKIT_WEB_FRAME (object);
	load_status = webkit_web_frame_get_load_status (frame);
	if (load_status != WEBKIT_LOAD_FINISHED)
		return;

	web_view = webkit_web_frame_get_web_view (frame);
	document = webkit_web_view_get_dom_document (web_view);

	button = webkit_dom_document_get_element_by_id (
			document, "__evo-collapse-headers-img");
	if (!button)
		return;

	d(printf("Conntecting to __evo-collapsable-headers-img::click event\n"));

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (button), "click",
		G_CALLBACK (toggle_headers_visibility), FALSE, web_view);

	for (i = 0; i < 3; i++) {
		gchar *id;
		id = g_strconcat ("__evo-moreaddr-img-", addresses[i], NULL);
		button = webkit_dom_document_get_element_by_id (document, id);
		g_free (id);

		if (!button)
			continue;

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (button), "click",
			G_CALLBACK (toggle_address_visibility), FALSE,
			(gpointer) addresses[i]);

		id = g_strconcat ("__evo-moreaddr-ellipsis-", addresses[i], NULL);
		button = webkit_dom_document_get_element_by_id (document, id);
		g_free (id);

		if (!button)
			continue;

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (button), "click",
			G_CALLBACK (toggle_address_visibility), FALSE,
			(gpointer) addresses[i]);
	}
}

static void
puri_bind_dom (GObject *object,
               GParamSpec *pspec,
               gpointer user_data)
{
	WebKitWebFrame *frame;
	WebKitLoadStatus load_status;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	EMailDisplay *display;
	GList *iter;
	EMFormat *emf;
	const gchar *frame_puri;

	frame = WEBKIT_WEB_FRAME (object);
	load_status = webkit_web_frame_get_load_status (frame);

	if (load_status != WEBKIT_LOAD_FINISHED)
		return;

	frame_puri = webkit_web_frame_get_name (frame);
	web_view = webkit_web_frame_get_web_view (frame);
	display = E_MAIL_DISPLAY (web_view);

	emf = EM_FORMAT (display->priv->formatter);
	if (!emf)
		return;

	iter = g_hash_table_lookup (
			emf->mail_part_table,
			webkit_web_frame_get_name (frame));

	document = webkit_web_view_get_dom_document (web_view);

	while (iter) {

		EMFormatPURI *puri = iter->data;

		if (!puri)
			continue;

		/* Iterate only the PURI rendered in the frame and all it's "subPURIs" */
		if (!g_str_has_prefix (puri->uri, frame_puri))
			break;

		if (puri->bind_func) {
			WebKitDOMElement *el = find_element_by_id (document, puri->uri);
			if (el) {
				d(printf("bind_func for %s\n", puri->uri));
				puri->bind_func (el, puri);
			}
		}

		iter = iter->next;
	}
}

static void
mail_display_frame_created (WebKitWebView *web_view,
                            WebKitWebFrame *frame,
                            gpointer user_data)
{
	d(printf("Frame %s created!\n", webkit_web_frame_get_name (frame)));

        /* Re-bind visibility of this newly created <iframe> with
         * related EAttachmentButton whenever content of this <iframe> is
         * (re)loaded */
	g_signal_connect (frame, "notify::load-status",
		G_CALLBACK (bind_attachment_iframe_visibility), NULL);

	/* Call bind_func of all PURIs written in this frame */
	g_signal_connect (frame, "notify::load-status",
		G_CALLBACK (puri_bind_dom), NULL);
}

static void
mail_display_set_fonts (EWebView *web_view,
			PangoFontDescription **monospace,
			PangoFontDescription **variable)
{
	EMailDisplay *display = E_MAIL_DISPLAY (web_view);
	gboolean use_custom_font;
	gchar *monospace_font, *variable_font;

	use_custom_font = g_settings_get_boolean (display->priv->settings, "use-custom-font");
	if (!use_custom_font) {
		*monospace = NULL;
		*variable = NULL;
		return;
	}

	monospace_font = g_settings_get_string (
				display->priv->settings,
				"monospace-font");
	variable_font = g_settings_get_string (
				display->priv->settings,
				"variable-width-font");

	*monospace = monospace_font ? pango_font_description_from_string (monospace_font) : NULL;
	*variable = variable_font ? pango_font_description_from_string (variable_font) : NULL;

	if (monospace_font)
		g_free (monospace_font);
	if (variable_font)
		g_free (variable_font);
}

static void
e_mail_display_class_init (EMailDisplayClass *class)
{
	GObjectClass *object_class;
	EWebViewClass *web_view_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailDisplayPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_display_set_property;
	object_class->get_property = mail_display_get_property;
	object_class->dispose = mail_display_dispose;

	web_view_class = E_WEB_VIEW_CLASS (class);
	web_view_class->set_fonts = mail_display_set_fonts;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = mail_display_realize;
	widget_class->style_set = mail_display_style_set;

	g_object_class_install_property (
		object_class,
		PROP_FORMATTER,
		g_param_spec_object (
			"formatter",
			"HTML Formatter",
			NULL,
			EM_TYPE_FORMAT_HTML,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MODE,
		g_param_spec_int (
			"mode",
			"Display Mode",
			NULL,
			0,
			G_MAXINT,
			EM_FORMAT_WRITE_MODE_NORMAL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HEADERS_COLLAPSABLE,
		g_param_spec_boolean (
			"headers-collapsable",
			"Headers Collapsable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HEADERS_COLLAPSED,
		g_param_spec_boolean (
			"headers-collapsed",
			"Headers Collapsed",
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_mail_display_init (EMailDisplay *display)
{
	GtkUIManager *ui_manager;
	GError *error = NULL;
	const gchar *user_cache_dir;
	WebKitWebSettings *settings;
	WebKitWebFrame *main_frame;

	display->priv = E_MAIL_DISPLAY_GET_PRIVATE (display);

	display->priv->force_image_load = FALSE;
	display->priv->mailto_actions = gtk_action_group_new ("mailto");
	gtk_action_group_add_actions (display->priv->mailto_actions, mailto_entries,
		G_N_ELEMENTS (mailto_entries), NULL);

	display->priv->images_actions = gtk_action_group_new ("image");
	gtk_action_group_add_actions (display->priv->images_actions, image_entries,
		G_N_ELEMENTS (image_entries), NULL);

	webkit_web_view_set_full_content_zoom (WEBKIT_WEB_VIEW (display), TRUE);

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (display));
	g_object_set (settings, "enable-frame-flattening", TRUE, NULL);

	g_signal_connect (display, "navigation-policy-decision-requested",
			  G_CALLBACK (mail_display_link_clicked), NULL);
	g_signal_connect (display, "resource-request-starting",
			  G_CALLBACK (mail_display_resource_requested), NULL);
	g_signal_connect (display, "process-mailto",
			  G_CALLBACK (mail_display_process_mailto), NULL);
	g_signal_connect (display, "update-actions",
			  G_CALLBACK (mail_display_webview_update_actions), NULL);
	g_signal_connect (display, "create-plugin-widget",
			  G_CALLBACK (mail_display_plugin_widget_requested), NULL);
	g_signal_connect (display, "frame-created",
			  G_CALLBACK (mail_display_frame_created), NULL);

	display->priv->settings = g_settings_new ("org.gnome.evolution.mail");
	g_signal_connect_swapped (
		display->priv->settings , "changed::monospace-font",
		G_CALLBACK (e_web_view_update_fonts), display);
	g_signal_connect_swapped (
		display->priv->settings , "changed::variable-width-font",
		G_CALLBACK (e_web_view_update_fonts), display);
	g_signal_connect_swapped (
		display->priv->settings , "changed::use-custom-font",
		G_CALLBACK (e_web_view_update_fonts), display);

	e_web_view_update_fonts (E_WEB_VIEW (display));

	main_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (display));
	g_signal_connect (main_frame, "notify::load-status",
		G_CALLBACK (setup_DOM_bindings), NULL);
	main_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (display));
	g_signal_connect (main_frame, "notify::load-status",
			  G_CALLBACK (puri_bind_dom), NULL);

        /* Because we are loading from a hard-coded string, there is
         * no chance of I/O errors.  Failure here implies a malformed
         * UI definition.  Full stop. */
	ui_manager = e_web_view_get_ui_manager (E_WEB_VIEW (display));
	gtk_ui_manager_insert_action_group (ui_manager, display->priv->mailto_actions, 0);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	if (error != NULL) {
		g_error ("%s", error->message);
		g_error_free (error);
	}

	error = NULL;
	gtk_ui_manager_insert_action_group (ui_manager, display->priv->images_actions, 0);
	gtk_ui_manager_add_ui_from_string (ui_manager, image_ui, -1, &error);

	if (error != NULL) {
		g_error ("%s", error->message);
		g_error_free (error);
	}

	e_web_view_install_request_handler (E_WEB_VIEW (display), E_TYPE_MAIL_REQUEST);
	e_web_view_install_request_handler (E_WEB_VIEW (display), E_TYPE_HTTP_REQUEST);
	e_web_view_install_request_handler (E_WEB_VIEW (display), E_TYPE_FILE_REQUEST);
	e_web_view_install_request_handler (E_WEB_VIEW (display), E_TYPE_STOCK_REQUEST);

	/* cache expiry - 2 hour access, 1 day max */
	user_cache_dir = e_get_user_cache_dir ();
	emd_global_http_cache = camel_data_cache_new (user_cache_dir, NULL);
	if (emd_global_http_cache) {
		camel_data_cache_set_expire_age (emd_global_http_cache, 24 * 60 * 60);
		camel_data_cache_set_expire_access (emd_global_http_cache, 2 * 60 * 60);
	}
}

EMFormatHTML *
e_mail_display_get_formatter (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	return display->priv->formatter;
}

void
e_mail_display_set_formatter (EMailDisplay *display,
                              EMFormatHTML *formatter)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if (formatter) {
		g_return_if_fail (EM_IS_FORMAT_HTML (formatter));
		g_object_ref (formatter);
	}

	if (display->priv->formatter != NULL) {
		/* The formatter might still exist after unrefing it, so 
		 * we need to stop listening to it's request for redrawing */
		g_signal_handlers_disconnect_by_func (
			display->priv->formatter, e_mail_display_reload, display);
		g_object_unref (display->priv->formatter);
	}

	display->priv->formatter = formatter;

	if (!formatter) {
		e_web_view_clear (E_WEB_VIEW (display));
		return;
	}

	mail_display_update_formatter_colors (display);

	g_signal_connect (formatter, "notify::image-loading-policy",
		G_CALLBACK (formatter_image_loading_policy_changed_cb), display);
	g_signal_connect_swapped (formatter, "redraw-requested",
		G_CALLBACK (e_mail_display_reload), display);
	g_signal_connect_swapped (formatter, "notify::charset",
		G_CALLBACK (e_mail_display_reload), display);

	g_object_notify (G_OBJECT (display), "formatter");
}

EMFormatWriteMode
e_mail_display_get_mode (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display),
			EM_FORMAT_WRITE_MODE_NORMAL);

	return display->priv->mode;
}

void
e_mail_display_set_mode (EMailDisplay *display,
                         EMFormatWriteMode mode)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if (display->priv->mode == mode)
		return;

	display->priv->mode = mode;

	e_mail_display_reload (display);

	g_object_notify (G_OBJECT (display), "mode");
}

gboolean
e_mail_display_get_headers_collapsable (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), FALSE);

	return display->priv->headers_collapsable;
}

void
e_mail_display_set_headers_collapsable (EMailDisplay *display,
                                        gboolean collapsable)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if (display->priv->headers_collapsable == collapsable)
		return;

	display->priv->headers_collapsable = collapsable;
	e_mail_display_reload (display);

	g_object_notify (G_OBJECT (display), "headers-collapsable");
}

gboolean
e_mail_display_get_headers_collapsed (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), FALSE);

	if (display->priv->headers_collapsable)
		return display->priv->headers_collapsed;

	return FALSE;
}

void
e_mail_display_set_headers_collapsed (EMailDisplay *display,
                                      gboolean collapsed)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if (display->priv->headers_collapsed == collapsed)
		return;

	display->priv->headers_collapsed = collapsed;

	g_object_notify (G_OBJECT (display), "headers-collapsed");
}

void
e_mail_display_load (EMailDisplay *display,
                     const gchar *msg_uri)
{
	EMFormat *emf;
	gchar *uri;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	display->priv->force_image_load = FALSE;

	emf = EM_FORMAT (display->priv->formatter);

	uri = em_format_build_mail_uri (emf->folder, emf->message_uid,
		"mode", G_TYPE_INT, display->priv->mode,
		"headers_collapsable", G_TYPE_BOOLEAN, display->priv->headers_collapsable,
		"headers_collapsed", G_TYPE_BOOLEAN, display->priv->headers_collapsed,
		NULL);

	e_web_view_load_uri (E_WEB_VIEW (display), uri);

	g_free (uri);
}

void
e_mail_display_reload (EMailDisplay *display)
{
	EWebView *web_view;
	const gchar *uri;
	gchar *base;
	GString *new_uri;
	GHashTable *table;
	GHashTableIter table_iter;
	gpointer key, val;
	gchar separator;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	web_view = E_WEB_VIEW (display);
	uri = e_web_view_get_uri (web_view);

	if (!uri || !*uri)
		return;

	if (strstr(uri, "?") == NULL) {
		e_web_view_reload (web_view);
		return;
	}

	base = g_strndup (uri, strstr (uri, "?") - uri);
	new_uri = g_string_new (base);
	g_free (base);

	table = soup_form_decode (strstr (uri, "?") + 1);
	g_hash_table_insert (table, g_strdup ("mode"), g_strdup_printf ("%d", display->priv->mode));
	g_hash_table_insert (table, g_strdup ("headers_collapsable"), g_strdup_printf ("%d", display->priv->headers_collapsable));
	g_hash_table_insert (table, g_strdup ("headers_collapsed"), g_strdup_printf ("%d", display->priv->headers_collapsed));

	g_hash_table_iter_init (&table_iter, table);
	separator = '?';
	while (g_hash_table_iter_next (&table_iter, &key, &val)) {
		g_string_append_printf (new_uri, "%c%s=%s", separator,
			(gchar *) key, (gchar *) val);

	if (separator == '?')
		separator = '&';
	}

	e_web_view_load_uri (web_view, new_uri->str);

	g_string_free (new_uri, TRUE);
	g_hash_table_destroy (table);
}

GtkAction *
e_mail_display_get_action (EMailDisplay *display,
                           const gchar *action_name)
{
	GtkAction *action;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	action = gtk_action_group_get_action (display->priv->mailto_actions, action_name);
	if (!action)
		action = gtk_action_group_get_action (display->priv->images_actions, action_name);

	return action;
}

void
e_mail_display_set_status (EMailDisplay *display,
                           const gchar *status)
{
	gchar *str;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	str = g_strdup_printf (
		"<!DOCTYPE>"
		"<html>"
		  "<head><title>Evolution Mail Display</title></head>"
		  "<body>"
		    "<table border=\"0\" width=\"100%%\" height=\"100%%\">"
		      "<tr height=\"100%%\" valign=\"middle\">"
			"<td width=\"100%%\" align=\"center\">"
			  "<strong>%s</strong>"
			"</td>"
		      "</tr>"
		    "</table>"
		  "</body>"
		"</html>", status);

	e_web_view_load_string (E_WEB_VIEW (display), str);
	g_free (str);

	gtk_widget_show_all (GTK_WIDGET (display));
}

gchar *
e_mail_display_get_selection_plain_text (EMailDisplay *display,
                                         gint *len)
{
	EWebView *web_view;
	WebKitWebFrame *frame;
	const gchar *frame_name;
	GValue value = {0};
	GType type;
	const gchar *str;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	web_view = E_WEB_VIEW (display);
	frame = webkit_web_view_get_focused_frame (WEBKIT_WEB_VIEW (web_view));
	frame_name = webkit_web_frame_get_name (frame);

	type = e_web_view_frame_exec_script (web_view, frame_name, "window.getSelection().toString()", &value);
	g_return_val_if_fail (type == G_TYPE_STRING, NULL);

	str = g_value_get_string (&value);

	if (len)
		*len = strlen (str);

	return g_strdup (str);
}

void
e_mail_display_load_images (EMailDisplay *display)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	display->priv->force_image_load = TRUE;
	e_web_view_reload (E_WEB_VIEW (display));
}

void
e_mail_display_set_force_load_images (EMailDisplay *display,
                                      gboolean force_load_images)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	display->priv->force_image_load = force_load_images;
}
