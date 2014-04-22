/*
 * e-mail-display.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-display.h"

#include <config.h>
#include <glib/gi18n.h>

#include <gdk/gdk.h>
#include <camel/camel.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-formatter-enumtypes.h>
#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter-print.h>
#include <em-format/e-mail-part-attachment.h>
#include <em-format/e-mail-part-utils.h>

#include "e-http-request.h"
#include "e-mail-display-popup-extension.h"
#include "e-mail-request.h"
#include "em-composer-utils.h"
#include "em-utils.h"

#define d(x)

#define E_MAIL_DISPLAY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplayPrivate))

struct _EMailDisplayPrivate {
	EMailPartList *part_list;
	EMailFormatterMode mode;
	EMailFormatter *formatter;

	gboolean headers_collapsable;
	gboolean headers_collapsed;
	gboolean force_image_load;

	GSettings *settings;

	GHashTable *widgets;

	guint scheduled_reload;
};

enum {
	PROP_0,
	PROP_FORMATTER,
	PROP_HEADERS_COLLAPSABLE,
	PROP_HEADERS_COLLAPSED,
	PROP_MODE,
	PROP_PART_LIST
};

static CamelDataCache *emd_global_http_cache = NULL;

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

G_DEFINE_TYPE (
	EMailDisplay,
	e_mail_display,
	E_TYPE_WEB_VIEW);

static void
formatter_image_loading_policy_changed_cb (GObject *object,
                                           GParamSpec *pspec,
                                           gpointer user_data)
{
	EMailDisplay *display = user_data;
	EMailFormatter *formatter = E_MAIL_FORMATTER (object);
	EImageLoadingPolicy policy;

	policy = e_mail_formatter_get_image_loading_policy (formatter);

	if (policy == E_IMAGE_LOADING_POLICY_ALWAYS)
		e_mail_display_load_images (display);
	else
		e_mail_display_reload (display);
}

static gboolean
mail_display_image_exists_in_cache (const gchar *image_uri)
{
	gchar *filename;
	gchar *hash;
	gboolean exists = FALSE;

	g_return_val_if_fail (emd_global_http_cache != NULL, FALSE);

	hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, image_uri, -1);
	filename = camel_data_cache_get_filename (
		emd_global_http_cache, "http", hash);

	if (filename != NULL) {
		exists = g_file_test (filename, G_FILE_TEST_EXISTS);
		g_free (filename);
	}

	g_free (hash);

	return exists;
}

static void
mail_display_update_formatter_colors (EMailDisplay *display)
{
	EMailFormatter *formatter;
	GtkStateFlags state_flags;

	formatter = display->priv->formatter;
	state_flags = gtk_widget_get_state_flags (GTK_WIDGET (display));

	if (formatter != NULL)
		e_mail_formatter_update_style (formatter, state_flags);
}

static void
mail_display_plugin_widget_disconnect_children (GtkWidget *widget,
                                                gpointer mail_display)
{
	g_signal_handlers_disconnect_by_data (widget, mail_display);
}

static void
mail_display_plugin_widget_disconnect (gpointer widget_uri,
                                       gpointer widget,
                                       gpointer mail_display)
{
	if (E_IS_ATTACHMENT_BAR (widget))
		g_signal_handlers_disconnect_by_data (widget, mail_display);
	else if (E_IS_ATTACHMENT_BUTTON (widget))
		g_signal_handlers_disconnect_by_data (widget, mail_display);
	else if (GTK_IS_CONTAINER (widget))
		gtk_container_foreach (
			widget,
			mail_display_plugin_widget_disconnect_children,
			mail_display);
}

static gboolean
mail_display_process_mailto (EWebView *web_view,
                             const gchar *mailto_uri,
                             gpointer user_data)
{
	gboolean handled = FALSE;

	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), FALSE);
	g_return_val_if_fail (mailto_uri != NULL, FALSE);

	if (g_ascii_strncasecmp (mailto_uri, "mailto:", 7) == 0) {
		EShell *shell;
		EMailPartList *part_list;
		CamelFolder *folder;

		part_list = E_MAIL_DISPLAY (web_view)->priv->part_list;
		folder = e_mail_part_list_get_folder (part_list);

		shell = e_shell_get_default ();
		em_utils_compose_new_message_with_mailto (
			shell, mailto_uri, folder);

		handled = TRUE;
	}

	return handled;
}

static gboolean
mail_display_link_clicked (WebKitWebView *web_view,
                           WebKitWebFrame *frame,
                           WebKitNetworkRequest *request,
                           WebKitWebNavigationAction *navigation_action,
                           WebKitWebPolicyDecision *policy_decision,
                           gpointer user_data)
{
	const gchar *uri = webkit_network_request_get_uri (request);

	if (g_str_has_prefix (uri, "file://")) {
		gchar *filename;

		filename = g_filename_from_uri (uri, NULL, NULL);

		if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
			webkit_web_policy_decision_ignore (policy_decision);
			webkit_network_request_set_uri (request, "about:blank");
			g_free (filename);
			return TRUE;
		}

		g_free (filename);
	}

	if (mail_display_process_mailto (E_WEB_VIEW (web_view), uri, NULL)) {
		/* do nothing, function handled the "mailto:" uri already */
		webkit_web_policy_decision_ignore (policy_decision);
		return TRUE;

	} else if (g_ascii_strncasecmp (uri, "thismessage:", 12) == 0) {
		/* ignore */
		webkit_web_policy_decision_ignore (policy_decision);
		return TRUE;

	} else if (g_ascii_strncasecmp (uri, "cid:", 4) == 0) {
		/* ignore */
		webkit_web_policy_decision_ignore (policy_decision);
		return TRUE;

	}

	/* Let WebKit handle it. */
	return FALSE;
}

static void
mail_display_resource_requested (WebKitWebView *web_view,
                                 WebKitWebFrame *frame,
                                 WebKitWebResource *resource,
                                 WebKitNetworkRequest *request,
                                 WebKitNetworkResponse *response,
                                 gpointer user_data)
{
	const gchar *original_uri;

	original_uri = webkit_network_request_get_uri (request);

	if (original_uri != NULL) {
		gchar *redirected_uri;

		redirected_uri = e_web_view_redirect_uri (
			E_WEB_VIEW (web_view), original_uri);

		webkit_network_request_set_uri (request, redirected_uri);

		g_free (redirected_uri);
	}
}

static WebKitDOMElement *
find_element_by_id (WebKitDOMDocument *document,
                    const gchar *id)
{
	WebKitDOMNodeList *frames;
	WebKitDOMElement *element;
	gulong ii, length;

	if (!WEBKIT_DOM_IS_DOCUMENT (document))
		return NULL;

	/* Try to look up the element in this DOM document */
	element = webkit_dom_document_get_element_by_id (document, id);
	if (element != NULL)
		return element;

	/* If the element is not here then recursively scan all frames */
	frames = webkit_dom_document_get_elements_by_tag_name (
		document, "iframe");
	length = webkit_dom_node_list_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMHTMLIFrameElement *iframe;
		WebKitDOMDocument *frame_doc;
		WebKitDOMElement *element;

		iframe = WEBKIT_DOM_HTML_IFRAME_ELEMENT (
			webkit_dom_node_list_item (frames, ii));

		frame_doc = webkit_dom_html_iframe_element_get_content_document (iframe);

		element = find_element_by_id (frame_doc, id);

		if (element != NULL)
			return element;
	}

	return NULL;
}

static void
mail_display_plugin_widget_resize (GtkWidget *widget,
                                   gpointer dummy,
                                   EMailDisplay *display)
{
	WebKitDOMElement *parent_element;
	gchar *dim;
	gint height, width;
	gfloat scale;

	parent_element = g_object_get_data (
		G_OBJECT (widget), "parent_element");

	if (!WEBKIT_DOM_IS_ELEMENT (parent_element)) {
		d (
			printf ("%s: %s does not have (valid) parent element!\n",
			G_STRFUNC, (gchar *) g_object_get_data (G_OBJECT (widget), "uri")));
		return;
	}

	scale = webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (display));
	width = gtk_widget_get_allocated_width (widget);
	gtk_widget_get_preferred_height_for_width (widget, width, &height, NULL);

	/* When zooming WebKit does not change dimensions of the elements,
	 * but only scales them on the canvas.  GtkWidget can't be scaled
	 * though so we need to cope with the dimension changes to keep the
	 * the widgets the correct size.  Due to inaccuracy in rounding
	 * (float -> int) it still acts a bit funny, but at least it does
	 * not cause widgets in WebKit to go crazy when zooming. */
	height = height * (1 / scale);

	/* Int -> Str */
	dim = g_strdup_printf ("%d", height);

	/* Set height of the containment <object> to match height of the
	 * GtkWidget it contains */
	webkit_dom_html_object_element_set_height (
		WEBKIT_DOM_HTML_OBJECT_ELEMENT (parent_element), dim);
	g_free (dim);
}

static void
plugin_widget_set_parent_element (GtkWidget *widget,
                                  EMailDisplay *display)
{
	const gchar *uri;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	uri = g_object_get_data (G_OBJECT (widget), "uri");
	if (uri == NULL || *uri == '\0')
		return;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (display));
	element = find_element_by_id (document, uri);

	if (!WEBKIT_DOM_IS_ELEMENT (element)) {
		g_warning ("Failed to find parent <object> for '%s' - no ID set?", uri);
		return;
	}

	/* Assign the WebKitDOMElement to "parent_element" data of the
	 * GtkWidget and the GtkWidget to "widget" data of the DOM Element. */
	g_object_set_data (G_OBJECT (widget), "parent_element", element);
	g_object_set_data (G_OBJECT (element), "widget", widget);

	g_object_bind_property (
		element, "hidden",
		widget, "visible",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);
}

static void
attachment_button_expanded (GObject *object,
                            GParamSpec *pspec,
                            gpointer user_data)
{
	EAttachmentButton *button = E_ATTACHMENT_BUTTON (object);
	EMailDisplay *display = user_data;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMCSSStyleDeclaration *css;
	const gchar *attachment_part_id;
	gchar *element_id;
	gboolean expanded;

	d (
		printf ("Attachment button %s has been %s!\n",
		(gchar *) g_object_get_data (object, "uri"),
		(e_attachment_button_get_expanded (button) ? "expanded" : "collapsed")));

	expanded =
		e_attachment_button_get_expanded (button) &&
		gtk_widget_get_visible (GTK_WIDGET (button));

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (display));
	attachment_part_id = g_object_get_data (object, "attachment_id");

	element_id = g_strconcat (attachment_part_id, ".wrapper", NULL);
	element = find_element_by_id (document, element_id);
	g_free (element_id);

	if (!WEBKIT_DOM_IS_ELEMENT (element)) {
		d (
			printf ("%s: Content <div> of attachment %s does not exist!!\n",
			G_STRFUNC, (gchar *) g_object_get_data (object, "uri")));
		return;
	}

	/* Show or hide the DIV which contains
	 * the attachment (iframe, image...). */
	css = webkit_dom_element_get_style (element);
	webkit_dom_css_style_declaration_set_property (
		css, "display", expanded ? "block" : "none", "", NULL);
}

static void
mail_display_attachment_count_changed (EAttachmentStore *store,
                                       GParamSpec *pspec,
                                       GtkWidget *box)
{
	WebKitDOMHTMLElement *element;
	GList *children;

	children = gtk_container_get_children (GTK_CONTAINER (box));
	g_return_if_fail (children && children->data);

	element = g_object_get_data (children->data, "parent_element");
	g_list_free (children);

	g_return_if_fail (WEBKIT_DOM_IS_HTML_ELEMENT (element));

	if (e_attachment_store_get_num_attachments (store) == 0) {
		gtk_widget_hide (box);
		webkit_dom_html_element_set_hidden (element, TRUE);
	} else {
		gtk_widget_show (box);
		webkit_dom_html_element_set_hidden (element, FALSE);
	}
}

static GtkWidget *
mail_display_plugin_widget_requested (WebKitWebView *web_view,
                                      gchar *mime_type,
                                      gchar *uri,
                                      GHashTable *param,
                                      gpointer user_data)
{
	EMailDisplay *display;
	EMailExtensionRegistry *reg;
	EMailFormatterExtension *extension;
	GQueue *extensions;
	GList *head, *link;
	EMailPart *part = NULL;
	GtkWidget *widget = NULL;
	gchar *part_id, *type, *object_uri;

	part_id = g_hash_table_lookup (param, "data");
	if (part_id == NULL || !g_str_has_prefix (uri, "mail://"))
		return NULL;

	type = g_hash_table_lookup (param, "type");
	if (type == NULL)
		return NULL;

	display = E_MAIL_DISPLAY (web_view);

	widget = g_hash_table_lookup (display->priv->widgets, part_id);
	if (widget != NULL) {
		d (printf ("Handeled %s widget request from cache\n", part_id));
		return widget;
	}

	/* Find the EMailPart representing the requested widget. */
	part = e_mail_part_list_ref_part (display->priv->part_list, part_id);
	if (part == NULL)
		return NULL;

	reg = e_mail_formatter_get_extension_registry (display->priv->formatter);
	extensions = e_mail_extension_registry_get_for_mime_type (reg, type);
	if (extensions == NULL)
		goto exit;

	extension = NULL;
	head = g_queue_peek_head_link (extensions);
	for (link = head; link != NULL; link = g_list_next (link)) {
		extension = link->data;

		if (extension == NULL)
			continue;

		if (e_mail_formatter_extension_has_widget (extension))
			break;
	}

	if (extension == NULL)
		goto exit;

	/* Get the widget from formatter */
	widget = e_mail_formatter_extension_get_widget (
		extension, display->priv->part_list, part, param);
	d (
		printf ("Created widget %s (%p) for part %s\n",
			G_OBJECT_TYPE_NAME (widget), widget, part_id));

	/* Should not happen! WebKit will display an ugly 'Plug-in not
	 * available' placeholder instead of hiding the <object> element. */
	if (widget == NULL)
		goto exit;

	/* Attachment button has URI different then the actual PURI because
	 * that URI identifies the attachment itself */
	if (E_IS_ATTACHMENT_BUTTON (widget)) {
		EMailPartAttachment *empa = (EMailPartAttachment *) part;
		gchar *attachment_part_id;

		if (empa->attachment_view_part_id)
			attachment_part_id = empa->attachment_view_part_id;
		else
			attachment_part_id = part_id;

		object_uri = g_strconcat (
			attachment_part_id, ".attachment_button", NULL);
		g_object_set_data_full (
			G_OBJECT (widget), "attachment_id",
			g_strdup (attachment_part_id),
			(GDestroyNotify) g_free);
	} else {
		object_uri = g_strdup (part_id);
	}

	/* Store the uri as data of the widget */
	g_object_set_data_full (
		G_OBJECT (widget), "uri",
		object_uri, (GDestroyNotify) g_free);

	/* Set pointer to the <object> element as GObject data
	 * "parent_element" and set pointer to the widget as GObject
	 * data "widget" to the <object> element. */
	plugin_widget_set_parent_element (widget, display);

	/* Resizing a GtkWidget requires changing size of parent
	 * <object> HTML element in DOM. */
	g_signal_connect (
		widget, "size-allocate",
		G_CALLBACK (mail_display_plugin_widget_resize), display);

	if (E_IS_ATTACHMENT_BAR (widget)) {
		GtkWidget *box = NULL;
		EAttachmentStore *store;

		/* Only when packed in box (grid does not work),
		 * EAttachmentBar reports correct height */
		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);

		/* When EAttachmentBar is expanded/collapsed it does not
		 * emit size-allocate signal despite it changes it's height. */
		g_signal_connect (
			widget, "notify::expanded",
			G_CALLBACK (mail_display_plugin_widget_resize),
			display);
		g_signal_connect (
			widget, "notify::active-view",
			G_CALLBACK (mail_display_plugin_widget_resize),
			display);

		/* Always hide an attachment bar without attachments */
		store = e_attachment_bar_get_store (E_ATTACHMENT_BAR (widget));
		g_signal_connect (
			store, "notify::num-attachments",
			G_CALLBACK (mail_display_attachment_count_changed),
			box);

		gtk_widget_show (widget);
		gtk_widget_show (box);

		/* Initial sync */
		mail_display_attachment_count_changed (store, NULL, box);

		widget = box;

	} else if (E_IS_ATTACHMENT_BUTTON (widget)) {

		/* Bind visibility of DOM element containing related
		 * attachment with 'expanded' property of this
		 * attachment button. */
		WebKitDOMElement *attachment;
		WebKitDOMDocument *document;
		EMailPartAttachment *empa = (EMailPartAttachment *) part;
		gchar *attachment_part_id;
		gchar *wrapper_element_id;

		if (empa->attachment_view_part_id)
			attachment_part_id = empa->attachment_view_part_id;
		else
			attachment_part_id = part_id;

		/* Find attachment-wrapper div which contains
		 * the content of the attachment (iframe). */
		document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (display));
		wrapper_element_id = g_strconcat (
			attachment_part_id, ".wrapper", NULL);
		attachment = find_element_by_id (document, wrapper_element_id);
		g_free (wrapper_element_id);

		/* None found? Attachment cannot be expanded */
		if (attachment == NULL) {
			e_attachment_button_set_expandable (
				E_ATTACHMENT_BUTTON (widget), FALSE);
		} else {
			CamelMimePart *mime_part;
			const CamelContentDisposition *disposition;

			e_attachment_button_set_expandable (
				E_ATTACHMENT_BUTTON (widget), TRUE);

			/* Show/hide the attachment when the EAttachmentButton
			 * is expanded/collapsed or shown/hidden. */
			g_signal_connect (
				widget, "notify::expanded",
				G_CALLBACK (attachment_button_expanded),
				display);
			g_signal_connect (
				widget, "notify::visible",
				G_CALLBACK (attachment_button_expanded),
				display);

			mime_part = e_mail_part_ref_mime_part (part);

			/* Automatically expand attachments that have inline
			 * disposition or the EMailParts have specific
			 * force_inline flag set. */
			disposition =
				camel_mime_part_get_content_disposition (mime_part);
			if (!part->force_collapse &&
			    (part->force_inline ||
			    (g_strcmp0 (empa->snoop_mime_type, "message/rfc822") == 0) ||
			     (disposition && disposition->disposition &&
				g_ascii_strncasecmp (
					disposition->disposition, "inline", 6) == 0))) {

				e_attachment_button_set_expanded (
					E_ATTACHMENT_BUTTON (widget), TRUE);
			} else {
				e_attachment_button_set_expanded (
					E_ATTACHMENT_BUTTON (widget), FALSE);
				attachment_button_expanded (
					G_OBJECT (widget), NULL, display);
			}

			g_object_unref (mime_part);
		}
	}

	g_hash_table_insert (
		display->priv->widgets,
		g_strdup (object_uri), g_object_ref (widget));

exit:
	if (part != NULL)
		g_object_unref (part);

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
	gchar *css_value;

	document = webkit_web_view_get_dom_document (web_view);

	short_headers = webkit_dom_document_get_element_by_id (
		document, "__evo-short-headers");
	if (short_headers == NULL)
		return;

	css_short = webkit_dom_element_get_style (short_headers);

	full_headers = webkit_dom_document_get_element_by_id (
		document, "__evo-full-headers");
	if (full_headers == NULL)
		return;

	css_full = webkit_dom_element_get_style (full_headers);
	css_value = webkit_dom_css_style_declaration_get_property_value (
		css_full, "display");
	expanded = (g_strcmp0 (css_value, "table") == 0);
	g_free (css_value);

	webkit_dom_css_style_declaration_set_property (
		css_full, "display",
		expanded ? "none" : "table", "", NULL);
	webkit_dom_css_style_declaration_set_property (
		css_short, "display",
		expanded ? "table" : "none", "", NULL);

	if (expanded)
		path = "evo-file://" EVOLUTION_IMAGESDIR "/plus.png";
	else
		path = "evo-file://" EVOLUTION_IMAGESDIR "/minus.png";

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (button), path);

	e_mail_display_set_headers_collapsed (
		E_MAIL_DISPLAY (web_view), expanded);

	d (printf ("Headers %s!\n", expanded ? "collapsed" : "expanded"));
}

static void
toggle_address_visibility (WebKitDOMElement *button,
                           WebKitDOMEvent *event)
{
	WebKitDOMElement *full_addr, *ellipsis;
	WebKitDOMElement *parent;
	WebKitDOMCSSStyleDeclaration *css_full, *css_ellipsis;
	const gchar *path;
	gboolean expanded;

	/* <b> element */
	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (button));
	/* <td> element */
	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (parent));

	full_addr = webkit_dom_element_query_selector (parent, "#__evo-moreaddr", NULL);

	if (!full_addr)
		return;

	css_full = webkit_dom_element_get_style (full_addr);

	ellipsis = webkit_dom_element_query_selector (parent, "#__evo-moreaddr-ellipsis", NULL);

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

	if (expanded)
		path = "evo-file://" EVOLUTION_IMAGESDIR "/plus.png";
	else
		path = "evo-file://" EVOLUTION_IMAGESDIR "/minus.png";

	if (!WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (button)) {
		button = webkit_dom_element_query_selector (parent, "#__evo-moreaddr-img", NULL);

		if (!button)
			return;
	}

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (button), path);
}

static void
add_color_css_rule_for_web_view (EWebView *view,
                                 const gchar *color_name,
                                 const gchar *color_value)
{
	gchar *selector;
	gchar *style;

	selector = g_strconcat (".-e-mail-formatter-", color_name, NULL);

	if (g_strstr_len (color_name, -1, "header")) {
		style = g_strconcat (
			"color: ", color_value, " !important;", NULL);
	} else if (g_strstr_len (color_name, -1, "frame")) {
		style = g_strconcat (
			"border-color: ", color_value, " !important;", NULL);
	} else {
		style = g_strconcat (
			"background-color: ", color_value, " !important;", NULL);
	}

	e_web_view_add_css_rule_into_style_sheet (
		view,
		"-e-mail-formatter-style-sheet",
		selector,
		style);

	g_free (style);
	g_free (selector);
}

static void
initialize_web_view_colors (EMailDisplay *display)
{
	EMailFormatter *formatter;
	gint ii;

	const gchar *color_names[] = {
		"body-color",
		"citation-color",
		"frame-color",
		"header-color",
		NULL
	};

	formatter = e_mail_display_get_formatter (display);

	for (ii = 0; color_names[ii]; ii++) {
		GdkRGBA *color = NULL;
		gchar *color_value;

		g_object_get (formatter, color_names[ii], &color, NULL);
		color_value = g_strdup_printf ("#%06x", e_rgba_to_value (color));

		add_color_css_rule_for_web_view (
			E_WEB_VIEW (display),
			color_names[ii],
			color_value);

		gdk_rgba_free (color);
		g_free (color_value);
	}
}

static void
setup_image_click_event_listeners_on_document (WebKitDOMDocument *document,
                                                WebKitWebView *web_view)
{
	gint length, ii = 0;
	WebKitDOMElement *button;
	WebKitDOMNodeList *list;

	/* Install event listeners on document */
	button = webkit_dom_document_get_element_by_id (
		document, "__evo-collapse-headers-img");
	if (button != NULL)
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (button), "click",
			G_CALLBACK (toggle_headers_visibility),
			FALSE, web_view);

	list = webkit_dom_document_query_selector_all (document, "*[id^=__evo-moreaddr-]", NULL);

	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		button = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (list, ii));

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (button), "click",
			G_CALLBACK (toggle_address_visibility), FALSE,
			NULL);
	}
}

static void
setup_dom_bindings (WebKitWebView *web_view,
                    WebKitWebFrame *frame,
                    gpointer user_data)
{
	WebKitDOMDocument *document;

	document = webkit_web_frame_get_dom_document (frame);

	setup_image_click_event_listeners_on_document (document, web_view);
}

static void
mail_parts_bind_dom (GObject *object,
                     GParamSpec *pspec,
                     gpointer user_data)
{
	WebKitWebFrame *frame;
	WebKitLoadStatus load_status;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	EMailDisplay *display;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	const gchar *frame_name;

	frame = WEBKIT_WEB_FRAME (object);
	load_status = webkit_web_frame_get_load_status (frame);

	if (load_status != WEBKIT_LOAD_FINISHED)
		return;

	web_view = webkit_web_frame_get_web_view (frame);
	display = E_MAIL_DISPLAY (web_view);
	if (display->priv->part_list == NULL)
		return;

	initialize_web_view_colors (display);
	frame_name = webkit_web_frame_get_name (frame);
	if (frame_name == NULL || *frame_name == '\0')
		frame_name = ".message.headers";

	document = webkit_web_view_get_dom_document (web_view);

	e_mail_part_list_queue_parts (
		display->priv->part_list, frame_name, &queue);
	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = E_MAIL_PART (link->data);
		WebKitDOMElement *element;
		const gchar *part_id;

		/* Iterate only the parts rendered in
		 * the frame and all it's subparts. */
		if (!e_mail_part_id_has_prefix (part, frame_name))
			break;

		part_id = e_mail_part_get_id (part);
		element = find_element_by_id (document, part_id);

		if (element != NULL)
			e_mail_part_bind_dom_element (part, element);
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));
}

static void
mail_display_frame_created (WebKitWebView *web_view,
                            WebKitWebFrame *frame,
                            gpointer user_data)
{
	d (printf ("Frame %s created!\n", webkit_web_frame_get_name (frame)));

	/* Call bind_func of all parts written in this frame */
	g_signal_connect (
		frame, "notify::load-status",
		G_CALLBACK (mail_parts_bind_dom), NULL);
}

static void
mail_display_uri_changed (EMailDisplay *display,
                          GParamSpec *pspec,
                          gpointer dummy)
{
	d (printf ("EMailDisplay URI changed, recreating widgets hashtable\n"));

	if (display->priv->widgets != NULL) {
		g_hash_table_foreach (
			display->priv->widgets,
			mail_display_plugin_widget_disconnect, display);
		g_hash_table_destroy (display->priv->widgets);
	}

	display->priv->widgets = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);
}

static void
mail_display_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
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

		case PROP_MODE:
			e_mail_display_set_mode (
				E_MAIL_DISPLAY (object),
				g_value_get_enum (value));
			return;

		case PROP_PART_LIST:
			e_mail_display_set_part_list (
				E_MAIL_DISPLAY (object),
				g_value_get_pointer (value));
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
				value,
				e_mail_display_get_formatter (
				E_MAIL_DISPLAY (object)));
			return;

		case PROP_HEADERS_COLLAPSABLE:
			g_value_set_boolean (
				value,
				e_mail_display_get_headers_collapsable (
				E_MAIL_DISPLAY (object)));
			return;

		case PROP_HEADERS_COLLAPSED:
			g_value_set_boolean (
				value,
				e_mail_display_get_headers_collapsed (
				E_MAIL_DISPLAY (object)));
			return;

		case PROP_MODE:
			g_value_set_enum (
				value,
				e_mail_display_get_mode (
				E_MAIL_DISPLAY (object)));
			return;

		case PROP_PART_LIST:
			g_value_set_pointer (
				value,
				e_mail_display_get_part_list (
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

	if (priv->scheduled_reload > 0) {
		g_source_remove (priv->scheduled_reload);
		priv->scheduled_reload = 0;
	}

	if (priv->widgets != NULL) {
		g_hash_table_foreach (
			priv->widgets,
			mail_display_plugin_widget_disconnect, object);
		g_hash_table_destroy (priv->widgets);
		priv->widgets = NULL;
	}

	if (priv->settings != NULL)
		g_signal_handlers_disconnect_matched (
			priv->settings, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);

	g_clear_object (&priv->part_list);
	g_clear_object (&priv->formatter);
	g_clear_object (&priv->settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_display_parent_class)->dispose (object);
}

static void
mail_display_constructed (GObject *object)
{
	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_display_parent_class)->constructed (object);
}

static void
mail_display_realize (GtkWidget *widget)
{
	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_mail_display_parent_class)->realize (widget);

	mail_display_update_formatter_colors (E_MAIL_DISPLAY (widget));
}

static void
mail_display_style_updated (GtkWidget *widget)
{
	EMailDisplay *display = E_MAIL_DISPLAY (widget);

	mail_display_update_formatter_colors (display);

	/* Chain up to parent's style_updated() method. */
	GTK_WIDGET_CLASS (e_mail_display_parent_class)->
		style_updated (widget);
}

static gboolean
mail_display_button_press_event (GtkWidget *widget,
                                 GdkEventButton *event)
{
	EWebView *web_view = E_WEB_VIEW (widget);
	WebKitHitTestResult *hit_test;
	GList *list, *link;

	if (event->button != 3)
		goto chainup;

	hit_test = webkit_web_view_get_hit_test_result (
		WEBKIT_WEB_VIEW (web_view), event);

	list = e_extensible_list_extensions (
		E_EXTENSIBLE (web_view), E_TYPE_EXTENSION);
	for (link = list; link != NULL; link = g_list_next (link)) {
		EExtension *extension = link->data;

		if (!E_IS_MAIL_DISPLAY_POPUP_EXTENSION (extension))
			continue;

		e_mail_display_popup_extension_update_actions (
			E_MAIL_DISPLAY_POPUP_EXTENSION (extension), hit_test);
	}
	g_list_free (list);

	g_object_unref (hit_test);

chainup:
	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_mail_display_parent_class)->
		button_press_event (widget, event);
}

static gchar *
mail_display_redirect_uri (EWebView *web_view,
                           const gchar *uri)
{
	EMailDisplay *display;
	EMailPartList *part_list;
	gboolean uri_is_http;

	display = E_MAIL_DISPLAY (web_view);
	part_list = e_mail_display_get_part_list (display);

	if (part_list == NULL)
		goto chainup;

	/* Redirect cid:part_id to mail://mail_id/cid:part_id */
	if (g_str_has_prefix (uri, "cid:")) {
		CamelFolder *folder;
		const gchar *message_uid;

		folder = e_mail_part_list_get_folder (part_list);
		message_uid = e_mail_part_list_get_message_uid (part_list);

		/* Always write raw content of CID object. */
		return e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, uri,
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_CID, NULL);
	}

	/* WebKit won't allow to load a local file when displaying
	 * "remote" mail:// protocol, so we need to handle this manually. */
	if (g_str_has_prefix (uri, "file:")) {
		gchar *content = NULL;
		gchar *content_type;
		gchar *filename;
		gchar *encoded;
		gchar *new_uri;
		gsize length = 0;

		filename = g_filename_from_uri (uri, NULL, NULL);
		if (filename == NULL)
			goto chainup;

		if (!g_file_get_contents (filename, &content, &length, NULL)) {
			g_free (filename);
			goto chainup;
		}

		encoded = g_base64_encode ((guchar *) content, length);
		content_type = g_content_type_guess (filename, NULL, 0, NULL);

		new_uri = g_strdup_printf (
			"data:%s;base64,%s", content_type, encoded);

		g_free (content_type);
		g_free (content);
		g_free (filename);
		g_free (encoded);

		return new_uri;
	}

	uri_is_http =
		g_str_has_prefix (uri, "http:") ||
		g_str_has_prefix (uri, "https:") ||
		g_str_has_prefix (uri, "evo-http:") ||
		g_str_has_prefix (uri, "evo-https:");

	/* Redirect http(s) request to evo-http(s) protocol.
	 * See EMailRequest for further details about this. */
	if (uri_is_http) {
		CamelFolder *folder;
		const gchar *message_uid;
		gchar *new_uri, *mail_uri, *enc;
		SoupURI *soup_uri;
		GHashTable *query;
		gboolean image_exists;
		EImageLoadingPolicy image_policy;

		/* Check Evolution's cache */
		image_exists = mail_display_image_exists_in_cache (uri);

		/* If the URI is not cached and we are not allowed to load it
		 * then redirect to invalid URI, so that webkit would display
		 * a native placeholder for it. */
		image_policy = e_mail_formatter_get_image_loading_policy (
			display->priv->formatter);
		if (!image_exists && !display->priv->force_image_load &&
		    (image_policy == E_IMAGE_LOADING_POLICY_NEVER)) {
			return g_strdup ("about:blank");
		}

		folder = e_mail_part_list_get_folder (part_list);
		message_uid = e_mail_part_list_get_message_uid (part_list);

		new_uri = g_strconcat ("evo-", uri, NULL);
		mail_uri = e_mail_part_build_uri (
			folder, message_uid, NULL, NULL);

		soup_uri = soup_uri_new (new_uri);
		if (soup_uri->query)
			query = soup_form_decode (soup_uri->query);
		else
			query = g_hash_table_new_full (
				g_str_hash, g_str_equal,
				g_free, g_free);
		enc = soup_uri_encode (mail_uri, NULL);
		g_hash_table_insert (query, g_strdup ("__evo-mail"), enc);

		if (display->priv->force_image_load) {
			g_hash_table_insert (
				query,
				g_strdup ("__evo-load-images"),
				g_strdup ("true"));
		}

		g_free (mail_uri);

		soup_uri_set_query_from_form (soup_uri, query);
		g_free (new_uri);

		new_uri = soup_uri_to_string (soup_uri, FALSE);

		soup_uri_free (soup_uri);
		g_hash_table_unref (query);

		return new_uri;
	}

chainup:
	/* Chain up to parent's redirect_uri() method. */
	return E_WEB_VIEW_CLASS (e_mail_display_parent_class)->
		redirect_uri (web_view, uri);
}

static CamelMimePart *
camel_mime_part_from_cid (EMailDisplay *display,
                          const gchar *uri)
{
	EMailPartList *part_list;
	CamelMimeMessage *message;
	CamelMimePart *mime_part;

	if (!g_str_has_prefix (uri, "cid:"))
		return NULL;

	part_list = e_mail_display_get_part_list (display);
	if (!part_list)
		return NULL;

	message = e_mail_part_list_get_message (part_list);
	if (!message)
		return NULL;

	mime_part = camel_mime_message_get_part_by_content_id (
		message, uri + 4);

	return mime_part;
}

static gchar *
mail_display_suggest_filename (EWebView *web_view,
                               const gchar *uri)
{
	EMailDisplay *display;
	CamelMimePart *mime_part;

	/* Note, this assumes the URI comes
	 * from the currently loaded message. */
	display = E_MAIL_DISPLAY (web_view);

	mime_part = camel_mime_part_from_cid (display, uri);

	if (mime_part)
		return g_strdup (camel_mime_part_get_filename (mime_part));

	/* Chain up to parent's suggest_filename() method. */
	return E_WEB_VIEW_CLASS (e_mail_display_parent_class)->
		suggest_filename (web_view, uri);
}

static void
mail_display_set_fonts (EWebView *web_view,
                        PangoFontDescription **monospace,
                        PangoFontDescription **variable)
{
	EMailDisplay *display = E_MAIL_DISPLAY (web_view);
	gboolean use_custom_font;
	gchar *monospace_font;
	gchar *variable_font;

	use_custom_font = g_settings_get_boolean (
		display->priv->settings, "use-custom-font");
	if (!use_custom_font) {
		*monospace = NULL;
		*variable = NULL;
		return;
	}

	monospace_font = g_settings_get_string (
		display->priv->settings, "monospace-font");
	variable_font = g_settings_get_string (
		display->priv->settings, "variable-width-font");

	*monospace = (monospace_font != NULL) ?
		pango_font_description_from_string (monospace_font) : NULL;
	*variable = (variable_font != NULL) ?
		pango_font_description_from_string (variable_font) : NULL;

	g_free (monospace_font);
	g_free (variable_font);
}

static void
mail_display_drag_data_get (GtkWidget *widget,
                            GdkDragContext *context,
                            GtkSelectionData *data,
                            guint info,
                            guint time,
                            EMailDisplay *display)
{
	CamelDataWrapper *dw;
	CamelMimePart *mime_part;
	CamelStream *stream;
	gchar *src, *base64_encoded, *mime_type, *uri;
	const gchar *filename;
	const guchar *data_from_webkit;
	gint length;
	GByteArray *byte_array;

	data_from_webkit = gtk_selection_data_get_data (data);
	length = gtk_selection_data_get_length (data);

	uri = g_strndup ((const gchar *) data_from_webkit, length);

	mime_part = camel_mime_part_from_cid (display, uri);

	if (!mime_part)
		goto out;

	stream = camel_stream_mem_new ();
	dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	g_return_if_fail (dw);

	mime_type = camel_data_wrapper_get_mime_type (dw);
	camel_data_wrapper_decode_to_stream_sync (dw, stream, NULL, NULL);
	camel_stream_close (stream, NULL, NULL);

	byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (stream));

	if (!byte_array->data) {
		g_object_unref (stream);
		goto out;
	}

	base64_encoded = g_base64_encode ((const guchar *) byte_array->data, byte_array->len);

	filename = camel_mime_part_get_filename (mime_part);
	/* Insert filename before base64 data */
	src = g_strconcat (filename, ";data:", mime_type, ";base64,", base64_encoded, NULL);

	gtk_selection_data_set (
		data,
		gtk_selection_data_get_data_type (data),
		gtk_selection_data_get_format (data),
		(const guchar *) src, strlen (src));

	g_free (src);
	g_free (base64_encoded);
	g_free (mime_type);
	g_object_unref (stream);
 out:
	g_free (uri);
}

static void
e_mail_display_class_init (EMailDisplayClass *class)
{
	GObjectClass *object_class;
	EWebViewClass *web_view_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EMailDisplayPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_display_constructed;
	object_class->set_property = mail_display_set_property;
	object_class->get_property = mail_display_get_property;
	object_class->dispose = mail_display_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = mail_display_realize;
	widget_class->style_updated = mail_display_style_updated;
	widget_class->button_press_event = mail_display_button_press_event;

	web_view_class = E_WEB_VIEW_CLASS (class);
	web_view_class->redirect_uri = mail_display_redirect_uri;
	web_view_class->suggest_filename = mail_display_suggest_filename;
	web_view_class->set_fonts = mail_display_set_fonts;

	g_object_class_install_property (
		object_class,
		PROP_FORMATTER,
		g_param_spec_pointer (
			"formatter",
			"Mail Formatter",
			NULL,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_HEADERS_COLLAPSABLE,
		g_param_spec_boolean (
			"headers-collapsable",
			"Headers Collapsable",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_HEADERS_COLLAPSED,
		g_param_spec_boolean (
			"headers-collapsed",
			"Headers Collapsed",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MODE,
		g_param_spec_enum (
			"mode",
			"Mode",
			NULL,
			E_TYPE_MAIL_FORMATTER_MODE,
			E_MAIL_FORMATTER_MODE_NORMAL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_PART_LIST,
		g_param_spec_pointer (
			"part-list",
			"Part List",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_display_init (EMailDisplay *display)
{
	GtkUIManager *ui_manager;
	const gchar *user_cache_dir;
	WebKitWebSettings *settings;
	WebKitWebFrame *main_frame;
	GtkActionGroup *actions;

	display->priv = E_MAIL_DISPLAY_GET_PRIVATE (display);

	/* Set invalid mode so that MODE property initialization is run
	 * completely (see e_mail_display_set_mode) */
	display->priv->mode = E_MAIL_FORMATTER_MODE_INVALID;
	e_mail_display_set_mode (display, E_MAIL_FORMATTER_MODE_NORMAL);
	display->priv->force_image_load = FALSE;
	display->priv->scheduled_reload = 0;

	webkit_web_view_set_full_content_zoom (WEBKIT_WEB_VIEW (display), TRUE);

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (display));
	g_object_set (settings, "enable-frame-flattening", TRUE, NULL);

	g_signal_connect (
		display, "navigation-policy-decision-requested",
		G_CALLBACK (mail_display_link_clicked), NULL);
	g_signal_connect (
		display, "resource-request-starting",
		G_CALLBACK (mail_display_resource_requested), NULL);
	g_signal_connect (
		display, "process-mailto",
		G_CALLBACK (mail_display_process_mailto), NULL);
	g_signal_connect (
		display, "create-plugin-widget",
		G_CALLBACK (mail_display_plugin_widget_requested), NULL);
	g_signal_connect (
		display, "frame-created",
		G_CALLBACK (mail_display_frame_created), NULL);
	g_signal_connect (
		display, "notify::uri",
		G_CALLBACK (mail_display_uri_changed), NULL);
	g_signal_connect (
		display, "document-load-finished",
		G_CALLBACK (setup_dom_bindings), NULL);
	g_signal_connect (
		display, "document-load-finished",
		G_CALLBACK (initialize_web_view_colors), NULL);
	g_signal_connect_after (
		display, "drag-data-get",
		G_CALLBACK (mail_display_drag_data_get), display);

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
	g_signal_connect (
		main_frame, "notify::load-status",
		G_CALLBACK (mail_parts_bind_dom), NULL);

	actions = e_web_view_get_action_group (E_WEB_VIEW (display), "mailto");
	gtk_action_group_add_actions (
		actions, mailto_entries,
		G_N_ELEMENTS (mailto_entries), display);
	ui_manager = e_web_view_get_ui_manager (E_WEB_VIEW (display));
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	e_web_view_install_request_handler (
		E_WEB_VIEW (display), E_TYPE_MAIL_REQUEST);
	e_web_view_install_request_handler (
		E_WEB_VIEW (display), E_TYPE_HTTP_REQUEST);
	e_web_view_install_request_handler (
		E_WEB_VIEW (display), E_TYPE_FILE_REQUEST);
	e_web_view_install_request_handler (
		E_WEB_VIEW (display), E_TYPE_STOCK_REQUEST);

	if (emd_global_http_cache == NULL) {
		user_cache_dir = e_get_user_cache_dir ();
		emd_global_http_cache = camel_data_cache_new (user_cache_dir, NULL);

		/* cache expiry - 2 hour access, 1 day max */
		camel_data_cache_set_expire_age (
			emd_global_http_cache, 24 * 60 * 60);
		camel_data_cache_set_expire_access (
			emd_global_http_cache, 2 * 60 * 60);
	}
}

static void
e_mail_display_update_colors (EMailDisplay *display,
                              GParamSpec *param_spec,
                              EMailFormatter *formatter)
{
	GdkRGBA *color = NULL;
	gchar *color_value;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	g_object_get (formatter, param_spec->name, &color, NULL);

	color_value = g_strdup_printf ("#%06x", e_rgba_to_value (color));

	add_color_css_rule_for_web_view (
		E_WEB_VIEW (display),
		param_spec->name,
		color_value);

	gdk_rgba_free (color);
	g_free (color_value);
}

GtkWidget *
e_mail_display_new (void)
{
	return g_object_new (E_TYPE_MAIL_DISPLAY, NULL);
}

EMailFormatterMode
e_mail_display_get_mode (EMailDisplay *display)
{
	g_return_val_if_fail (
		E_IS_MAIL_DISPLAY (display),
		E_MAIL_FORMATTER_MODE_INVALID);

	return display->priv->mode;
}

void
e_mail_display_set_mode (EMailDisplay *display,
                         EMailFormatterMode mode)
{
	EMailFormatter *formatter;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if (display->priv->mode == mode)
		return;

	display->priv->mode = mode;

	if (display->priv->mode == E_MAIL_FORMATTER_MODE_PRINTING)
		formatter = e_mail_formatter_print_new ();
	else
		formatter = e_mail_formatter_new ();

	g_clear_object (&display->priv->formatter);
	display->priv->formatter = formatter;
	mail_display_update_formatter_colors (display);

	g_signal_connect (
		formatter, "notify::image-loading-policy",
		G_CALLBACK (formatter_image_loading_policy_changed_cb),
		display);

	g_object_connect (
		formatter,
		"swapped-object-signal::notify::charset",
			G_CALLBACK (e_mail_display_reload), display,
		"swapped-object-signal::notify::image-loading-policy",
			G_CALLBACK (e_mail_display_reload), display,
		"swapped-object-signal::notify::mark-citations",
			G_CALLBACK (e_mail_display_reload), display,
		"swapped-object-signal::notify::show-sender-photo",
			G_CALLBACK (e_mail_display_reload), display,
		"swapped-object-signal::notify::show-real-date",
			G_CALLBACK (e_mail_display_reload), display,
		"swapped-object-signal::notify::animate-images",
			G_CALLBACK (e_mail_display_reload), display,
		"swapped-object-signal::notify::body-color",
			G_CALLBACK (e_mail_display_update_colors), display,
		"swapped-object-signal::notify::citation-color",
			G_CALLBACK (e_mail_display_update_colors), display,
		"swapped-object-signal::notify::frame-color",
			G_CALLBACK (e_mail_display_update_colors), display,
		"swapped-object-signal::notify::header-color",
			G_CALLBACK (e_mail_display_update_colors), display,
		"swapped-object-signal::need-redraw",
			G_CALLBACK (e_mail_display_reload), display,
		NULL);

	e_mail_display_reload (display);

	g_object_notify (G_OBJECT (display), "mode");
}

EMailFormatter *
e_mail_display_get_formatter (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	return display->priv->formatter;
}

EMailPartList *
e_mail_display_get_part_list (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	return display->priv->part_list;
}

void
e_mail_display_set_part_list (EMailDisplay *display,
                              EMailPartList *part_list)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if (display->priv->part_list == part_list)
		return;

	if (part_list != NULL) {
		g_return_if_fail (E_IS_MAIL_PART_LIST (part_list));
		g_object_ref (part_list);
	}

	if (display->priv->part_list != NULL)
		g_object_unref (display->priv->part_list);

	display->priv->part_list = part_list;

	g_object_notify (G_OBJECT (display), "part-list");
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
	EMailPartList *part_list;
	CamelFolder *folder;
	const gchar *message_uid;
	const gchar *default_charset, *charset;
	gchar *uri;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	display->priv->force_image_load = FALSE;

	part_list = display->priv->part_list;
	if (part_list == NULL) {
		e_web_view_clear (E_WEB_VIEW (display));
		return;
	}

	folder = e_mail_part_list_get_folder (part_list);
	message_uid = e_mail_part_list_get_message_uid (part_list);
	default_charset = e_mail_formatter_get_default_charset (display->priv->formatter);
	charset = e_mail_formatter_get_charset (display->priv->formatter);

	if (!default_charset)
		default_charset = "";
	if (!charset)
		charset = "";

	uri = e_mail_part_build_uri (
		folder, message_uid,
		"mode", G_TYPE_INT, display->priv->mode,
		"headers_collapsable", G_TYPE_BOOLEAN, display->priv->headers_collapsable,
		"headers_collapsed", G_TYPE_BOOLEAN, display->priv->headers_collapsed,
		"formatter_default_charset", G_TYPE_STRING, default_charset,
		"formatter_charset", G_TYPE_STRING, charset,
		NULL);

	e_web_view_load_uri (E_WEB_VIEW (display), uri);

	g_free (uri);
}

static gboolean
do_reload_display (EMailDisplay *display)
{
	EWebView *web_view;
	gchar *uri, *query;
	GHashTable *table;
	SoupURI *soup_uri;
	gchar *mode, *collapsable, *collapsed;
	const gchar *default_charset, *charset;

	web_view = E_WEB_VIEW (display);
	uri = (gchar *) webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view));

	display->priv->scheduled_reload = 0;

	if (uri == NULL || *uri == '\0')
		return FALSE;

	if (strstr (uri, "?") == NULL) {
		e_web_view_reload (web_view);
		return FALSE;
	}

	soup_uri = soup_uri_new (uri);

	mode = g_strdup_printf ("%d", display->priv->mode);
	collapsable = g_strdup_printf ("%d", display->priv->headers_collapsable);
	collapsed = g_strdup_printf ("%d", display->priv->headers_collapsed);
	default_charset = e_mail_formatter_get_default_charset (display->priv->formatter);
	charset = e_mail_formatter_get_charset (display->priv->formatter);

	if (!default_charset)
		default_charset = "";
	if (!charset)
		charset = "";

	table = soup_form_decode (soup_uri->query);
	g_hash_table_replace (
		table, g_strdup ("mode"), mode);
	g_hash_table_replace (
		table, g_strdup ("headers_collapsable"), collapsable);
	g_hash_table_replace (
		table, g_strdup ("headers_collapsed"), collapsed);
	g_hash_table_replace (
		table, g_strdup ("formatter_default_charset"), (gpointer) default_charset);
	g_hash_table_replace (
		table, g_strdup ("formatter_charset"), (gpointer) charset);

	query = soup_form_encode_hash (table);

	/* The hash table does not free custom values upon destruction */
	g_free (mode);
	g_free (collapsable);
	g_free (collapsed);
	g_hash_table_destroy (table);

	soup_uri_set_query (soup_uri, query);
	g_free (query);

	uri = soup_uri_to_string (soup_uri, FALSE);
	e_web_view_load_uri (web_view, uri);
	g_free (uri);
	soup_uri_free (soup_uri);

	return FALSE;
}

void
e_mail_display_reload (EMailDisplay *display)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if (display->priv->scheduled_reload > 0)
		return;

	/* Schedule reloading if neccessary.
	 * Prioritize ahead of GTK+ redraws. */
	display->priv->scheduled_reload = g_idle_add_full (
		G_PRIORITY_HIGH_IDLE,
		(GSourceFunc) do_reload_display, display, NULL);
}

GtkAction *
e_mail_display_get_action (EMailDisplay *display,
                           const gchar *action_name)
{
	GtkAction *action;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	action = e_web_view_get_action (E_WEB_VIEW (display), action_name);

	return action;
}

void
e_mail_display_set_status (EMailDisplay *display,
                           const gchar *status)
{
	gchar *str;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	str = g_strdup_printf (
		"<!DOCTYPE HTML>\n"
		"<html>\n"
		"<head>\n"
		"<meta name=\"generator\" content=\"Evolution Mail\"/>\n"
		"<title>Evolution Mail Display</title>\n"
		"</head>\n"
		"<body class=\"-e-web-view-background-color e-web-view-text-color\">"
		"  <style>html, body { height: 100%%; }</style>\n"
		"  <table border=\"0\" width=\"100%%\" height=\"100%%\">\n"
		"    <tr height=\"100%%\" valign=\"middle\">\n"
		"      <td width=\"100%%\" align=\"center\">\n"
		"        <strong>%s</strong>\n"
		"      </td>\n"
		"    </tr>\n"
		"  </table>\n"
		"</body>\n"
		"</html>\n",
		status);

	e_web_view_load_string (E_WEB_VIEW (display), str);

	g_free (str);
}

static gchar *
mail_display_get_frame_selection_text (WebKitDOMElement *iframe)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMNodeList *frames;
	gulong ii, length;

	document = webkit_dom_html_iframe_element_get_content_document (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (selection && (webkit_dom_dom_selection_get_range_count (selection) > 0)) {
		WebKitDOMRange *range;

		range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
		if (range != NULL)
			return webkit_dom_range_to_string (range, NULL);
	}

	frames = webkit_dom_document_get_elements_by_tag_name (
		document, "IFRAME");
	length = webkit_dom_node_list_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		gchar *text;

		node = webkit_dom_node_list_item (frames, ii);

		text = mail_display_get_frame_selection_text (
			WEBKIT_DOM_ELEMENT (node));

		if (text != NULL)
			return text;
	}

	return NULL;
}

gchar *
e_mail_display_get_selection_plain_text (EMailDisplay *display)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *frames;
	gulong ii, length;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	if (!webkit_web_view_has_selection (WEBKIT_WEB_VIEW (display)))
		return NULL;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (display));
	frames = webkit_dom_document_get_elements_by_tag_name (document, "IFRAME");
	length = webkit_dom_node_list_get_length (frames);

	for (ii = 0; ii < length; ii++) {
		gchar *text;
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (frames, ii);

		text = mail_display_get_frame_selection_text (
			WEBKIT_DOM_ELEMENT (node));

		if (text != NULL)
			return text;
	}

	return NULL;
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

