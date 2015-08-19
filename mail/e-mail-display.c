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
#include <glib/gstdio.h>

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
#include "e-mail-ui-session.h"
#include "em-composer-utils.h"
#include "em-utils.h"

#include <web-extensions/e-web-extension-names.h>

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

	GHashTable *old_settings;

	GMutex remote_content_lock;
	EMailRemoteContent *remote_content;
	GHashTable *skipped_remote_content_sites;

	guint web_extension_headers_collapsed_signal_id;
};

enum {
	PROP_0,
	PROP_FORMATTER,
	PROP_HEADERS_COLLAPSABLE,
	PROP_HEADERS_COLLAPSED,
	PROP_MODE,
	PROP_PART_LIST,
	PROP_REMOTE_CONTENT
};

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
e_mail_display_claim_skipped_uri (EMailDisplay *mail_display,
				  const gchar *uri)
{
	SoupURI *soup_uri;
	const gchar *site;

	g_return_if_fail (E_IS_MAIL_DISPLAY (mail_display));
	g_return_if_fail (uri != NULL);

	soup_uri = soup_uri_new (uri);
	if (!soup_uri)
		return;

	site = soup_uri_get_host (soup_uri);
	if (site && *site) {
		g_mutex_lock (&mail_display->priv->remote_content_lock);

		if (!g_hash_table_contains (mail_display->priv->skipped_remote_content_sites, site)) {
			g_hash_table_insert (mail_display->priv->skipped_remote_content_sites, g_strdup (site), NULL);
		}

		g_mutex_unlock (&mail_display->priv->remote_content_lock);
	}

	soup_uri_free (soup_uri);
}

static void
e_mail_display_cleanup_skipped_uris (EMailDisplay *mail_display)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (mail_display));

	g_mutex_lock (&mail_display->priv->remote_content_lock);
	g_hash_table_remove_all (mail_display->priv->skipped_remote_content_sites);
	g_mutex_unlock (&mail_display->priv->remote_content_lock);
}

static gboolean
e_mail_display_can_download_uri (EMailDisplay *mail_display,
				 const gchar *uri)
{
	SoupURI *soup_uri;
	const gchar *site;
	gboolean can_download = FALSE;
	EMailRemoteContent *remote_content;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (mail_display), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	remote_content = e_mail_display_ref_remote_content (mail_display);
	if (!remote_content)
		return FALSE;

	soup_uri = soup_uri_new (uri);
	if (!soup_uri) {
		g_object_unref (remote_content);
		return FALSE;
	}

	site = soup_uri_get_host (soup_uri);
	if (site && *site)
		can_download = e_mail_remote_content_has_site (remote_content, site);

	soup_uri_free (soup_uri);

	if (!can_download && mail_display->priv->part_list) {
		CamelMimeMessage *message;

		message = e_mail_part_list_get_message (mail_display->priv->part_list);
		if (message) {
			CamelInternetAddress *from;

			from = camel_mime_message_get_from (message);
			if (from) {
				gint ii, len;

				len = camel_address_length (CAMEL_ADDRESS (from));
				for (ii = 0; ii < len && !can_download; ii++) {
					const gchar *mail = NULL;

					if (!camel_internet_address_get	(from, ii, NULL, &mail))
						break;

					if (mail && *mail)
						can_download = e_mail_remote_content_has_mail (remote_content, mail);
				}
			}
		}
	}

	g_object_unref (remote_content);

	return can_download;
}

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

#if 0
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
#endif
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
decide_policy_cb (WebKitWebView *web_view,
                  WebKitPolicyDecision *decision,
                  WebKitPolicyDecisionType type)
{
	WebKitNavigationPolicyDecision *navigation_decision;
	WebKitNavigationAction *navigation_action;
	WebKitURIRequest *request;
	const gchar *uri;

	if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
		return FALSE;

	navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
	navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
	request = webkit_navigation_action_get_request (navigation_action);

	uri = webkit_uri_request_get_uri (request);

	if (!uri || !*uri) {
		g_warning ("asdasdasdasdadasdasd"); /* FIXME WK2 */
		webkit_policy_decision_ignore (decision);
		return TRUE;
	}

	if (g_str_has_prefix (uri, "file://")) {
		gchar *filename;

		filename = g_filename_from_uri (uri, NULL, NULL);

		if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
			webkit_policy_decision_ignore (decision);
			/* FIXME XXX WK2 Not sure if the request will be changed there */
			webkit_uri_request_set_uri (request, "about:blank");
			g_free (filename);
			return TRUE;
		}

		g_free (filename);
	}

	if (mail_display_process_mailto (E_WEB_VIEW (web_view), uri, NULL)) {
		/* do nothing, function handled the "mailto:" uri already */
		webkit_policy_decision_ignore (decision);
		return TRUE;

	} else if (g_ascii_strncasecmp (uri, "thismessage:", 12) == 0) {
		/* ignore */
		webkit_policy_decision_ignore (decision);
		return TRUE;

	} else if (g_ascii_strncasecmp (uri, "cid:", 4) == 0) {
		/* ignore */
		webkit_policy_decision_ignore (decision);
		return TRUE;

	}

	/* Let WebKit handle it. */
	return FALSE;
}
#if 0 /* FIXME WK2 */
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
	WebKitDOMElement *element = NULL;
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
			goto out;
	}
 out:
	g_object_unref (frames);

	return element;
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

	e_binding_bind_property (
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

	if (WEBKIT_DOM_IS_HTML_ELEMENT (element) && expanded &&
	    webkit_dom_element_get_child_element_count (element) == 0) {
		gchar *inner_html_data;

		inner_html_data = webkit_dom_element_get_attribute (element, "inner-html-data");
		if (inner_html_data && *inner_html_data) {
			WebKitDOMHTMLElement *html_element;

			html_element = WEBKIT_DOM_HTML_ELEMENT (element);
			webkit_dom_html_element_set_inner_html (html_element, inner_html_data, NULL);

			webkit_dom_element_remove_attribute (element, "inner-html-data");
		}

		g_free (inner_html_data);
	}

	/* Show or hide the DIV which contains
	 * the attachment (iframe, image...). */
	css = webkit_dom_element_get_style (element);
	webkit_dom_css_style_declaration_set_property (
		css, "display", expanded ? "block" : "none", "", NULL);
}

static void
attachment_button_zoom_to_window_cb (GObject *object,
				     GParamSpec *pspec,
				     gpointer user_data)
{
	EAttachmentButton *button = E_ATTACHMENT_BUTTON (object);
	EMailDisplay *display = user_data;
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *child;
	WebKitDOMCSSStyleDeclaration *css;
	const gchar *attachment_part_id;
	gchar *element_id;
	gboolean zoom_to_window;

	d (
		printf ("Attachment button %s has been set to %s!\n",
		(gchar *) g_object_get_data (object, "uri"),
		(e_attachment_botton_get_zoom_to_window (attachment) ? "zoom-to-window" : "zoom to 100%")));

	if (!gtk_widget_get_visible (GTK_WIDGET (button)))
		return;

	zoom_to_window = e_attachment_button_get_zoom_to_window (button);

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

	child = webkit_dom_element_get_first_element_child (element);
	if (!child || !WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (child)) {
		d (
			printf ("%s: Content <div> of attachment %s does not contain image, but %s\n",
			G_STRFUNC, (gchar *) g_object_get_data (object, "uri"),
			child ? G_OBJECT_TYPE_NAME (child) : "[null]"));
		g_clear_object (&child);
		return;
	}

	css = webkit_dom_element_get_style (child);
	if (zoom_to_window) {
		webkit_dom_css_style_declaration_set_property (css, "max-width", "100%", "", NULL);
	} else {
		g_free (webkit_dom_css_style_declaration_remove_property (css, "max-width", NULL));
	}
	g_object_unref (css);
	g_clear_object (&child);
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
	GWeakRef *weakref;
	gchar *part_id, *type, *object_uri;

	part_id = g_hash_table_lookup (param, "data");
	if (part_id == NULL || !g_str_has_prefix (uri, "mail://"))
		return NULL;

	type = g_hash_table_lookup (param, "type");
	if (type == NULL)
		return NULL;

	display = E_MAIL_DISPLAY (web_view);

	weakref = g_hash_table_lookup (display->priv->widgets, part_id);
	if (weakref)
		widget = g_weak_ref_get (weakref);

	if (widget != NULL) {
		/* This cannot be the last reference; thread-safety is assured,
		   because this runs in the main thread only. */
		g_object_unref (widget);
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
		EMailPartAttachment *empa = (EMailPartAttachment *) part;

		e_attachment_button_set_expandable (E_ATTACHMENT_BUTTON (widget),
			e_mail_part_attachment_get_expandable (empa));

		if (e_mail_part_attachment_get_expandable (empa)) {
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
			g_signal_connect (
				widget, "notify::zoom-to-window",
				G_CALLBACK (attachment_button_zoom_to_window_cb),
				display);

			if (e_mail_part_should_show_inline (part)) {
				e_attachment_button_set_expanded (
					E_ATTACHMENT_BUTTON (widget), TRUE);
			} else {
				e_attachment_button_set_expanded (
					E_ATTACHMENT_BUTTON (widget), FALSE);
				attachment_button_expanded (
					G_OBJECT (widget), NULL, display);
			}
		}
	}

	g_hash_table_insert (
		display->priv->widgets,
		g_strdup (object_uri), e_weak_ref_new (widget));

exit:
	if (part != NULL)
		g_object_unref (part);

	return widget;
}
#endif
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
			"border-color: ", color_value, NULL);
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
	GtkTextDirection direction;
	const gchar *style;
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

	e_web_view_add_css_rule_into_style_sheet (
		E_WEB_VIEW (display),
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-none",
		"border-width: 1px; border-style: solid");

	/* the rgba values below were copied from e-formatter-secure-button */
	direction = gtk_widget_get_default_direction ();

	if (direction == GTK_TEXT_DIR_RTL)
		style = "border-width: 1px 1px 1px 4px; border-style: solid; border-color: rgba(53%, 73%, 53%, 1.0)";
	else
		style = "border-width: 1px 4px 1px 1px; border-style: solid; border-color: rgba(53%, 73%, 53%, 1.0)";
	e_web_view_add_css_rule_into_style_sheet (
		E_WEB_VIEW (display),
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-good",
		style);

	if (direction == GTK_TEXT_DIR_RTL)
		style = "border-width: 1px 1px 1px 4px; border-style: solid; border-color: rgba(73%, 53%, 53%, 1.0)";
	else
		style = "border-width: 1px 4px 1px 1px; border-style: solid; border-color: rgba(73%, 53%, 53%, 1.0)";
	e_web_view_add_css_rule_into_style_sheet (
		E_WEB_VIEW (display),
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-bad",
		style);

	if (direction == GTK_TEXT_DIR_RTL)
		style = "border-width: 1px 1px 1px 4px; border-style: solid; border-color: rgba(91%, 82%, 13%, 1.0)";
	else
		style = "border-width: 1px 4px 1px 1px; border-style: solid; border-color: rgba(91%, 82%, 13%, 1.0)";
	e_web_view_add_css_rule_into_style_sheet (
		E_WEB_VIEW (display),
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-unknown",
		style);

	if (direction == GTK_TEXT_DIR_RTL)
		style = "border-width: 1px 1px 1px 4px; border-style: solid; border-color: rgba(91%, 82%, 13%, 1.0)";
	else
		style = "border-width: 1px 4px 1px 1px; border-style: solid; border-color: rgba(91%, 82%, 13%, 1.0)";
	e_web_view_add_css_rule_into_style_sheet (
		E_WEB_VIEW (display),
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-need-key",
		style);
}

static void
headers_collapsed_signal_cb (GDBusConnection *connection,
                             const gchar *sender_name,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             EMailDisplay *display)
{
	gboolean expanded;

	if (g_strcmp0 (signal_name, "HeadersCollapsed") != 0)
		return;

	if (parameters)
		g_variant_get (parameters, "(b)", &expanded);

	e_mail_display_set_headers_collapsed (display, expanded);
}

static void
setup_dom_bindings (WebKitWebView *web_view,
                    WebKitLoadEvent load_event,
                    gpointer user_data)
{
	GDBusProxy *web_extension;
	EMailDisplay *display;

	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	display = E_MAIL_DISPLAY (web_view);

	web_extension = e_web_view_get_web_extension_proxy (E_WEB_VIEW (web_view));


	if (web_extension) {
		if (display->priv->web_extension_headers_collapsed_signal_id == 0) {
			display->priv->web_extension_headers_collapsed_signal_id =
				g_dbus_connection_signal_subscribe (
					g_dbus_proxy_get_connection (web_extension),
					g_dbus_proxy_get_name (web_extension),
					E_WEB_EXTENSION_INTERFACE,
					"RecurToggled",
					E_WEB_EXTENSION_OBJECT_PATH,
					NULL,
					G_DBUS_SIGNAL_FLAGS_NONE,
					(GDBusSignalCallback) headers_collapsed_signal_cb,
					display,
					NULL);
		}

		g_dbus_proxy_call (
			web_extension,
			"EMailDisplayBindDOM",
			g_variant_new (
				"(t)",
				webkit_web_view_get_page_id (web_view)),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
 			NULL);
	}
}

static void
mail_element_exists_cb (GDBusProxy *web_extension,
                        GAsyncResult *result,
                        EMailPart *part)
{
	gboolean element_exists = FALSE;
	GVariant *result_variant;
	guint64 page_id;

	result_variant = g_dbus_proxy_call_finish (web_extension, result, NULL);
	if (result_variant) {
		g_variant_get (result_variant, "(bt)", &element_exists, &page_id);
		g_variant_unref (result_variant);
	}

	if (element_exists)
		e_mail_part_bind_dom_element (
			part,
			web_extension,
			page_id,
			e_mail_part_get_id (part));

	g_object_unref (part);
}

static void
mail_parts_bind_dom (WebKitWebView *web_view,
                     WebKitLoadEvent load_event,
                     gpointer user_data)
{
	EMailDisplay *display;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	GDBusProxy *web_extension;

	display = E_MAIL_DISPLAY (web_view);

	if (load_event == WEBKIT_LOAD_STARTED) {
		e_mail_display_cleanup_skipped_uris (display);
		return;
	}

	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	if (display->priv->part_list == NULL)
		return;

	initialize_web_view_colors (display);

	web_extension = e_web_view_get_web_extension_proxy (E_WEB_VIEW (display));
	if (!web_extension)
		return;

	e_mail_part_list_queue_parts (
		display->priv->part_list, NULL, &queue);
	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = E_MAIL_PART (link->data);
		const gchar *part_id;

		part_id = e_mail_part_get_id (part);

		g_dbus_proxy_call (
			web_extension,
			"ElementExists",
			g_variant_new (
				"(ts)",
				webkit_web_view_get_page_id (
					WEBKIT_WEB_VIEW (display)),
				part_id),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			(GAsyncReadyCallback)mail_element_exists_cb,
			g_object_ref (part));
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));
}
#if 0
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
		(GDestroyNotify) e_weak_ref_free);
}
#endif
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

		case PROP_REMOTE_CONTENT:
			e_mail_display_set_remote_content (
				E_MAIL_DISPLAY (object),
				g_value_get_object (value));
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

		case PROP_REMOTE_CONTENT:
			g_value_take_object (
				value,
				e_mail_display_ref_remote_content (
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
#if 0
	if (priv->widgets != NULL) {
		g_hash_table_foreach (
			priv->widgets,
			mail_display_plugin_widget_disconnect, object);
		g_hash_table_destroy (priv->widgets);
		priv->widgets = NULL;
	}
#endif
	if (priv->settings != NULL)
		g_signal_handlers_disconnect_matched (
			priv->settings, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);

	if (priv->web_extension_headers_collapsed_signal_id > 0) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (
				e_web_view_get_web_extension_proxy (E_WEB_VIEW (object))),
			priv->web_extension_headers_collapsed_signal_id);
		priv->web_extension_headers_collapsed_signal_id = 0;
	}

	g_clear_object (&priv->part_list);
	g_clear_object (&priv->formatter);
	g_clear_object (&priv->settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_display_parent_class)->dispose (object);
}

static void
mail_display_finalize (GObject *object)
{
	EMailDisplayPrivate *priv;

	priv = E_MAIL_DISPLAY_GET_PRIVATE (object);

	if (priv->old_settings) {
		g_hash_table_destroy (priv->old_settings);
		priv->old_settings = NULL;
	}

	g_mutex_lock (&priv->remote_content_lock);
	if (priv->skipped_remote_content_sites) {
		g_hash_table_destroy (priv->skipped_remote_content_sites);
		priv->skipped_remote_content_sites = NULL;
	}

	g_clear_object (&priv->remote_content);
	g_mutex_unlock (&priv->remote_content_lock);
	g_mutex_clear (&priv->remote_content_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_display_parent_class)->finalize (object);
}

static void
mail_display_get_font_settings (GSettings *settings,
                                PangoFontDescription **monospace,
                                PangoFontDescription **variable)
{
	gboolean use_custom_font;
	gchar *monospace_font;
	gchar *variable_font;

	use_custom_font = g_settings_get_boolean (settings, "use-custom-font");

	if (!use_custom_font) {
		*monospace = NULL;
		*variable = NULL;
		return;
	}

	monospace_font = g_settings_get_string (settings, "monospace-font");
	variable_font = g_settings_get_string (settings, "variable-width-font");

	*monospace = (monospace_font != NULL) ?
		pango_font_description_from_string (monospace_font) : NULL;
	*variable = (variable_font != NULL) ?
		pango_font_description_from_string (variable_font) : NULL;

	g_free (monospace_font);
	g_free (variable_font);
}

static void
mail_display_set_fonts (EWebView *web_view,
                        PangoFontDescription **monospace,
                        PangoFontDescription **variable)
{
	EMailDisplay *display = E_MAIL_DISPLAY (web_view);

	mail_display_get_font_settings (display->priv->settings, monospace, variable);
}

static void
mail_display_web_view_initialize (WebKitWebView *web_view)
{
	WebKitSettings *webkit_settings;

	webkit_settings = webkit_web_view_get_settings (web_view);

	g_object_set (webkit_settings,
		"enable-frame-flattening", TRUE,
		NULL);
}

static void
mail_display_constructed (GObject *object)
{
	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_display_parent_class)->constructed (object);

	mail_display_web_view_initialize (WEBKIT_WEB_VIEW (object));

	e_web_view_update_fonts (E_WEB_VIEW (object));
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
	GList *list, *link;

	if (event->button != 3)
		goto chainup;

	list = e_extensible_list_extensions (
		E_EXTENSIBLE (web_view), E_TYPE_EXTENSION);
	for (link = list; link != NULL; link = g_list_next (link)) {
		EExtension *extension = link->data;

		if (!E_IS_MAIL_DISPLAY_POPUP_EXTENSION (extension))
			continue;

		e_mail_display_popup_extension_update_actions (
			E_MAIL_DISPLAY_POPUP_EXTENSION (extension));
	}
	g_list_free (list);

chainup:
	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_mail_display_parent_class)->
		button_press_event (widget, event);
}
#if 0 /* FIXME WK2 */
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
		gboolean can_download_uri;
		EImageLoadingPolicy image_policy;

		can_download_uri = e_mail_display_can_download_uri (display, uri);
		if (!can_download_uri) {
			/* Check Evolution's cache */
			can_download_uri = mail_display_image_exists_in_cache (uri);
		}

		/* If the URI is not cached and we are not allowed to load it
		 * then redirect to invalid URI, so that webkit would display
		 * a native placeholder for it. */
		image_policy = e_mail_formatter_get_image_loading_policy (
			display->priv->formatter);
		if (!can_download_uri && !display->priv->force_image_load &&
		    (image_policy == E_IMAGE_LOADING_POLICY_NEVER)) {
			e_mail_display_claim_skipped_uri (display, uri);
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

		if (display->priv->force_image_load || can_download_uri) {
			g_hash_table_insert (
				query,
				g_strdup ("__evo-load-images"),
				g_strdup ("true"));
		} else if (image_policy != E_IMAGE_LOADING_POLICY_ALWAYS) {
			e_mail_display_claim_skipped_uri (display, uri);
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
#endif

static void
e_mail_display_test_change_and_update_fonts_cb (EMailDisplay *mail_display,
						const gchar *key,
						GSettings *settings)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (mail_display->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (mail_display->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (mail_display->priv->old_settings, key);

		e_web_view_update_fonts (E_WEB_VIEW (mail_display));
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static void
mail_display_process_uri_scheme_finished_cb (EMailDisplay *display,
                                             GAsyncResult *result,
                                             WebKitURISchemeRequest *request)
{
	GError *error = NULL;

	if (!g_task_propagate_boolean (G_TASK (result), &error)) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_warning ("URI %s cannot be processed: %s",
				webkit_uri_scheme_request_get_uri (request),
				error ? error->message : "Unknown error");
		}
		g_object_unref (request);
		if (error)
			g_error_free (error);
	}
}

static void
mail_display_cid_uri_scheme_appeared_cb (WebKitURISchemeRequest *request)
{
	EMailDisplay *display;
	EMailPartList *part_list;
	EMailPart *part;
	GInputStream *stream;
	const gchar *uri;
	const gchar *mime_type;
	GByteArray *byte_array;
	CamelStream *output_stream;
	GCancellable *cancellable = NULL;
	CamelDataWrapper *dw;
	CamelMimePart *mime_part;

	display = E_MAIL_DISPLAY (webkit_uri_scheme_request_get_web_view (request));

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	part_list = e_mail_display_get_part_list (display);
	uri = webkit_uri_scheme_request_get_uri (request);
	part = e_mail_part_list_ref_part (part_list, uri);
	mime_type = e_mail_part_get_mime_type (part);
	mime_part = e_mail_part_ref_mime_part (part);
	dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	g_return_if_fail (dw);

	byte_array = g_byte_array_new ();
	output_stream = camel_stream_mem_new ();

	/* We retain ownership of the byte array. */
	camel_stream_mem_set_byte_array (
		CAMEL_STREAM_MEM (output_stream), byte_array);

	camel_data_wrapper_decode_to_stream_sync (
		dw, output_stream, cancellable, NULL);

	stream = g_memory_input_stream_new_from_bytes (
		g_byte_array_free_to_bytes (byte_array));

	webkit_uri_scheme_request_finish (request, stream, -1, mime_type);

	g_object_unref (mime_part);
	g_object_unref (stream);
	g_object_unref (part);
}

static gssize
copy_stream_to_stream (GIOStream *file_io_stream,
                       GMemoryInputStream *output,
                       GCancellable *cancellable)
{
	GInputStream *input_stream;
	gchar *buff;
	gssize read_len = 0;
	gssize total_len = 0;
	const gsize buff_size = 4096;

	g_seekable_seek (
		G_SEEKABLE (file_io_stream), 0,
		G_SEEK_SET, cancellable, NULL);

	input_stream = g_io_stream_get_input_stream (file_io_stream);

	buff = g_malloc (buff_size);
	read_len = g_input_stream_read (
		input_stream, buff, buff_size, cancellable, NULL);
	while (read_len > 0) {
		g_memory_input_stream_add_data (
			output, buff, read_len, g_free);

		total_len += read_len;

		buff = g_malloc (buff_size);
		read_len = g_input_stream_read (
			input_stream, buff, buff_size, cancellable, NULL);
	}

	/* Free the last unused buffer */
	g_free (buff);

	return total_len;
}

static void
redirect_handler (SoupMessage *msg,
                  gpointer user_data)
{
	if (SOUP_STATUS_IS_REDIRECTION (msg->status_code)) {
		SoupSession *soup_session = user_data;
		SoupURI *new_uri;
		const gchar *new_loc;

		new_loc = soup_message_headers_get_list (msg->response_headers, "Location");
		if (!new_loc)
			return;

		new_uri = soup_uri_new_with_base (soup_message_get_uri (msg), new_loc);
		if (!new_uri) {
			soup_message_set_status_full (
				msg,
				SOUP_STATUS_MALFORMED,
				"Invalid Redirect URL");
			return;
		}

		soup_message_set_uri (msg, new_uri);
		soup_session_requeue_message (soup_session, msg);

		soup_uri_free (new_uri);
	}
}

static void
send_and_handle_redirection (SoupSession *session,
                             SoupMessage *message,
                             gchar **new_location)
{
	gchar *old_uri = NULL;

	g_return_if_fail (message != NULL);

	if (new_location) {
		old_uri = soup_uri_to_string (soup_message_get_uri (message), FALSE);
	}

	soup_message_set_flags (message, SOUP_MESSAGE_NO_REDIRECT);
	soup_message_add_header_handler (
		message, "got_body", "Location",
		G_CALLBACK (redirect_handler), session);
	soup_message_headers_append (message->request_headers, "Connection", "close");
	soup_session_send_message (session, message);

	if (new_location) {
		gchar *new_loc = soup_uri_to_string (soup_message_get_uri (message), FALSE);

		if (new_loc && old_uri && !g_str_equal (new_loc, old_uri)) {
			*new_location = new_loc;
		} else {
			g_free (new_loc);
		}
	}

	g_free (old_uri);
}

static void
web_view_process_http_uri_scheme_request (GTask *task,
                                          gpointer source_object,
                                          gpointer task_data,
                                          GCancellable *cancellable)
{
	SoupURI *soup_uri;
	gchar *evo_uri, *uri;
	gchar *mail_uri;
	const gchar *user_cache_dir;
	const gchar *content_type;
	GInputStream *stream = NULL;
	gboolean force_load_images = FALSE;
	gboolean ret_val = FALSE;
	EImageLoadingPolicy image_policy;
	gchar *uri_md5;
	EShell *shell;
	GSettings *settings;
	CamelDataCache *cache;
	GIOStream *cache_stream;
	GHashTable *query;
	gint uri_len;
	WebKitURISchemeRequest *request = WEBKIT_URI_SCHEME_REQUEST (task_data);

	/* Remove the __evo-mail query */
	soup_uri = soup_uri_new (webkit_uri_scheme_request_get_uri (request));

	if (!soup_uri_get_query (soup_uri)) {
		g_task_return_boolean (task, FALSE);
		soup_uri_free (soup_uri);
		return;
	}

	query = soup_form_decode (soup_uri_get_query (soup_uri));
	mail_uri = g_hash_table_lookup (query, "__evo-mail");
	if (mail_uri)
		mail_uri = g_strdup (mail_uri);

	g_hash_table_remove (query, "__evo-mail");

	/* Remove __evo-load-images if present (and in such case set
	 * force_load_images to TRUE) */
	force_load_images = g_hash_table_remove (query, "__evo-load-images");

	soup_uri_set_query_from_form (soup_uri, query);
	g_hash_table_unref (query);

	evo_uri = soup_uri_to_string (soup_uri, FALSE);

	if (camel_debug_start ("emformat:requests")) {
		printf ("%s: looking for '%s'\n", G_STRFUNC, evo_uri);
		camel_debug_end ();
	}

	/* Remove the "evo-" prefix from scheme */
	uri_len = strlen (evo_uri);
	uri = NULL;
	if (evo_uri && (uri_len > 5)) {
		/* Remove trailing "?" if there is no URI query */
		if (evo_uri[uri_len - 1] == '?') {
			uri = g_strndup (evo_uri + 4, uri_len - 5);
		} else {
			uri = g_strdup (evo_uri + 4);
		}
		g_free (evo_uri);
	}

	if (!uri || !*uri)
		goto cleanup;

	/* Use MD5 hash of the URI as a filname of the resourec cache file.
	 * We were previously using the URI as a filename but the URI is
	 * sometimes too long for a filename. */
	uri_md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);

	/* Open Evolution's cache */
	user_cache_dir = e_get_user_cache_dir ();
	cache = camel_data_cache_new (user_cache_dir, NULL);
	camel_data_cache_set_expire_age (cache, 24 * 60 * 60);
	camel_data_cache_set_expire_access (cache, 2 * 60 * 60);

	/* Found item in cache! */
	cache_stream = camel_data_cache_get (cache, "http", uri_md5, NULL);
	if (cache_stream)
		goto process;

	/* If the item is not in the cache and Evolution is in offline mode then
	 * quit regardless any image loading policy */
	shell = e_shell_get_default ();
	if (!e_shell_get_online (shell)) {
		goto cleanup;
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	image_policy = g_settings_get_enum (settings, "image-loading-policy");
	g_object_unref (settings);

	/* Item not found in cache, but image loading policy allows us to fetch
	 * it from the interwebs */
	if (!force_load_images && mail_uri &&
	    (image_policy == E_IMAGE_LOADING_POLICY_SOMETIMES)) {
		CamelObjectBag *registry;
		gchar *decoded_uri;
		EMailPartList *part_list;

		registry = e_mail_part_list_get_registry ();
		decoded_uri = soup_uri_decode (mail_uri);

		part_list = camel_object_bag_get (registry, decoded_uri);
		if (part_list) {
			EShellBackend *shell_backend;
			EMailBackend *backend;
			EMailSession *session;
			CamelInternetAddress *addr;
			CamelMimeMessage *message;
			gboolean known_address = FALSE;
			GError *error = NULL;

			shell_backend =
				e_shell_get_backend_by_name (shell, "mail");
			backend = E_MAIL_BACKEND (shell_backend);
			session = e_mail_backend_get_session (backend);

			message = e_mail_part_list_get_message (part_list);
			addr = camel_mime_message_get_from (message);

			e_mail_ui_session_check_known_address_sync (
				E_MAIL_UI_SESSION (session),
				addr, FALSE, cancellable,
				&known_address, &error);

			if (error != NULL) {
				g_warning ("%s: %s", G_STRFUNC, error->message);
				g_error_free (error);
			}

			if (known_address)
				force_load_images = TRUE;

			g_object_unref (part_list);
		}

		g_free (decoded_uri);
	}

	if ((image_policy == E_IMAGE_LOADING_POLICY_ALWAYS) ||
	    force_load_images) {

		SoupSession *temp_session;
		SoupMessage *message;
		GIOStream *cache_stream;
		GError *error;
		GMainContext *context;

		message = soup_message_new (SOUP_METHOD_GET, uri);
		if (!message) {
			g_debug ("%s: Skipping invalid URI '%s'", G_STRFUNC, uri);
			goto cleanup;
		}

		context = g_main_context_new ();
		g_main_context_push_thread_default (context);

		temp_session = soup_session_new_with_options (
			SOUP_SESSION_TIMEOUT, 90, NULL);
#if 0
/* FIXME WK2 */
		e_binding_bind_property (
			soup_session, "proxy-resolver",
			temp_session, "proxy-resolver",
			G_BINDING_SYNC_CREATE);

#endif
		soup_message_headers_append (
			message->request_headers, "User-Agent", "Evolution/" VERSION);
/* FIXME WK2
		send_and_handle_redirection (session, message, NULL);*/
		send_and_handle_redirection (temp_session, message, NULL);

		if (!SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
			g_warning ("Failed to request %s (code %d)", uri, message->status_code);
			goto cleanup;
		}

		/* Write the response body to cache */
		error = NULL;
		cache_stream = camel_data_cache_add (cache, "http", uri_md5, &error);
		if (error != NULL) {
			g_warning (
				"Failed to create cache file for '%s': %s",
				uri, error->message);
			g_clear_error (&error);
		} else {
			GOutputStream *output_stream;

			output_stream =
				g_io_stream_get_output_stream (cache_stream);

			g_output_stream_write_all (
				output_stream,
				message->response_body->data,
				message->response_body->length,
				NULL, cancellable, &error);

			g_io_stream_close (cache_stream, NULL, NULL);
			g_object_unref (cache_stream);

			if (error != NULL) {
				if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
					g_warning (
						"Failed to write data to cache stream: %s",
						error->message);
				g_clear_error (&error);
				g_object_unref (message);
				g_object_unref (temp_session);
				g_main_context_unref (context);
				goto cleanup;
			}
		}

		g_object_unref (message);
/* FIXME WK2
		g_object_unref (session);*/
		g_object_unref (temp_session);
	}

 process:
	if (cache_stream) {
		gssize len;

		stream = g_memory_input_stream_new ();

		len = copy_stream_to_stream (
			cache_stream,
			G_MEMORY_INPUT_STREAM (stream), cancellable);

		g_io_stream_close (cache_stream, NULL, NULL);
		g_object_unref (cache_stream);

		/* When succesfully read some data from cache then
		 * get mimetype and return the stream to WebKit.
		 * Otherwise try to fetch the resource again from the network. */
		if (len > 0) {
			GFile *file;
			GFileInfo *info;
			gchar *path;

			path = camel_data_cache_get_filename (cache, "http", uri_md5);
			file = g_file_new_for_path (path);

			info = g_file_query_info (
				file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				0, cancellable, NULL);

			content_type = g_file_info_get_content_type (info);

			/* FIXME WK2 */
			webkit_uri_scheme_request_finish (
				request, stream, len, content_type);

			ret_val = TRUE;

			g_object_unref (request);
			g_object_unref (stream);

			d (
				printf ("'%s' found in cache (%d bytes, %s)\n",
				uri, len, content_type));

			g_object_unref (info);
			g_object_unref (file);
			g_free (path);
		} else {
			d (printf ("Failed to load '%s' from cache.\n", uri));
		}
	}
 cleanup:
	if (cache)
		g_clear_object (&cache);

	if (soup_uri)
		soup_uri_free (soup_uri);

	g_free (uri);
	g_free (uri_md5);
	g_free (mail_uri);

	g_task_return_boolean (task, ret_val);
}

static void
mail_display_http_uri_scheme_appeared_cb (WebKitURISchemeRequest *request)
{
	EMailDisplay *display;
	GTask *task;
	GCancellable *cancellable;

	display = E_MAIL_DISPLAY (webkit_uri_scheme_request_get_web_view (request));

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	cancellable = g_cancellable_new ();

	task = g_task_new (
		display, cancellable,
		(GAsyncReadyCallback) mail_display_process_uri_scheme_finished_cb,
		request);

	g_task_set_task_data (task, g_object_ref (request), NULL);
	g_task_run_in_thread (task, web_view_process_http_uri_scheme_request);

	g_object_unref (task);
	g_object_unref (cancellable);
}

static void
mail_display_process_mail_uri_scheme_request (GTask *task,
                                              gpointer source_object,
                                              gpointer task_data,
                                              GCancellable *cancellable)
{
	GInputStream *input_stream = NULL;
	EMailFormatter *formatter;
	EMailPartList *part_list;
	GOutputStream *output_stream;
	GHashTable *query;
	const gchar *val, *uri;
	const gchar *default_charset, *charset;
	SoupURI *soup_uri;
	EMailFormatterContext context = { 0 };
	EMailDisplay *display = E_MAIL_DISPLAY (source_object);
	WebKitURISchemeRequest *request = WEBKIT_URI_SCHEME_REQUEST (task_data);
	GBytes *bytes;

	uri = webkit_uri_scheme_request_get_uri (request);

	part_list = display->priv->part_list;

	if (camel_debug_start ("emformat:requests")) {
		printf ("%s: found part-list %p for full_uri '%s'\n", G_STRFUNC, part_list, uri);
		camel_debug_end ();
	}

	if (!part_list) {
		g_task_return_boolean (task, FALSE);
		return;
	}

	soup_uri = soup_uri_new (uri);
	if (!soup_uri || !soup_uri->query) {
		if (soup_uri)
			soup_uri_free (soup_uri);
		g_task_return_boolean (task, FALSE);
		return;
	}
	query = soup_form_decode (soup_uri->query);

	val = g_hash_table_lookup (query, "headers_collapsed");
	if (val && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED;

	val = g_hash_table_lookup (query, "headers_collapsable");
	if (val && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE;

	val = g_hash_table_lookup (query, "mode");
	if (val)
		context.mode = atoi (val);

	default_charset = g_hash_table_lookup (query, "formatter_default_charset");
	charset = g_hash_table_lookup (query, "formatter_charset");

	context.part_list = g_object_ref (part_list);
	context.uri = g_strdup (uri);

	formatter = display->priv->formatter;

	if (default_charset && *default_charset != '\0')
		e_mail_formatter_set_default_charset (formatter, default_charset);
	if (charset && *charset != '\0')
		e_mail_formatter_set_charset (formatter, charset);

	output_stream = g_memory_output_stream_new_resizable ();

	val = g_hash_table_lookup (query, "part_id");
	if (val) {
		EMailPart *part;
		const gchar *mime_type;
		gchar *part_id;

		part_id = soup_uri_decode (val);
		part = e_mail_part_list_ref_part (part_list, part_id);
		if (!part) {
			if (camel_debug_start ("emformat:requests")) {
				printf ("%s: part with id '%s' not found\n", G_STRFUNC, val);
				camel_debug_end ();
			}

			g_free (part_id);
			goto no_part;
		}
		g_free (part_id);

		mime_type = g_hash_table_lookup (query, "mime_type");

		if (context.mode == E_MAIL_FORMATTER_MODE_SOURCE)
			mime_type = "application/vnd.evolution.source";

		if (context.mode == E_MAIL_FORMATTER_MODE_CID) {
			CamelDataWrapper *dw;
			CamelMimePart *mime_part;

			mime_part = e_mail_part_ref_mime_part (part);
			dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
			if (!dw)
				goto no_part;

			camel_data_wrapper_decode_to_output_stream_sync (
				dw, output_stream, cancellable, NULL);

			g_object_unref (mime_part);
		} else {
			if (!mime_type)
				mime_type = e_mail_part_get_mime_type (part);

			e_mail_formatter_format_as (
				formatter, &context, part,
				output_stream, mime_type,
				cancellable);
		}

		g_object_unref (part);
	} else {
		e_mail_formatter_format_sync (
			formatter, part_list, output_stream,
			context.flags, context.mode, cancellable);
	}

 no_part:
	g_clear_object (&context.part_list);

	g_output_stream_close (output_stream, NULL, NULL);

	bytes = g_memory_output_stream_steal_as_bytes (
		G_MEMORY_OUTPUT_STREAM (output_stream));

	if (g_bytes_get_size (bytes) == 0) {
		gchar *data;

		g_bytes_unref (bytes);

		data = g_strdup_printf (
			"<p align='center'>%s</p>",
			_("The message has no text content."));

		/* Takes ownership of the string. */
		bytes = g_bytes_new_take (
			data, strlen (data) + 1);
	}

	input_stream =
		g_memory_input_stream_new_from_bytes (bytes);

	webkit_uri_scheme_request_finish (request, input_stream, -1, "text/html");

	g_object_unref (input_stream);
	g_object_unref (output_stream);

	if (query)
		g_hash_table_destroy (query);

	if (soup_uri)
		soup_uri_free (soup_uri);

	g_task_return_boolean (task, TRUE);
}

static GInputStream *
get_empty_image_stream (void)
{
	GdkPixbuf *pixbuf;
	gchar *buffer;
	gsize length;

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
	gdk_pixbuf_fill (pixbuf, 0x00000000); /* transparent black */
	gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &length, "png", NULL, NULL);
	g_object_unref (pixbuf);

	return g_memory_input_stream_new_from_data (buffer, length, g_free);
}

static void
mail_display_process_contact_photo_uri_scheme_request (GTask *task,
                                                       gpointer source_object,
                                                       gpointer task_data,
                                                       GCancellable *cancellable)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *mail_backend;
	EMailSession *mail_session;
	EPhotoCache *photo_cache;
	CamelInternetAddress *cia;
	GInputStream *stream = NULL;
	const gchar *uri;
	const gchar *email_address;
	const gchar *escaped_string;
	gchar *unescaped_string;
	GError *error = NULL;
	SoupURI *soup_uri;
	GHashTable *uri_query;
	WebKitURISchemeRequest *request = WEBKIT_URI_SCHEME_REQUEST (task_data);

	/* XXX Is this really the only way to obtain
	 *     the mail session instance from here? */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	mail_backend = E_MAIL_BACKEND (shell_backend);
	mail_session = e_mail_backend_get_session (mail_backend);

	photo_cache = e_mail_ui_session_get_photo_cache (
		E_MAIL_UI_SESSION (mail_session));

	uri = webkit_uri_scheme_request_get_uri (request);

	soup_uri = soup_uri_new (uri);
	if (!soup_uri || !soup_uri->query)
		goto exit;

	uri_query = soup_form_decode (soup_uri->query);
	escaped_string = g_hash_table_lookup (uri_query, "mailaddr");
	if (escaped_string == NULL || *escaped_string == '\0')
		goto exit;

	cia = camel_internet_address_new ();

	unescaped_string = g_uri_unescape_string (escaped_string, NULL);
	camel_address_decode (CAMEL_ADDRESS (cia), unescaped_string);
	g_free (unescaped_string);

	if (camel_internet_address_get (cia, 0, NULL, &email_address))
		e_photo_cache_get_photo_sync (
			photo_cache, email_address,
			cancellable, &stream, &error);

	g_object_unref (cia);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
	} else if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	}

exit:
	if (!stream)
		stream = get_empty_image_stream ();

	webkit_uri_scheme_request_finish (request, stream, -1, "image/*");

	g_object_unref (request);
	g_object_unref (stream);

	if (uri_query)
		g_hash_table_destroy (uri_query);

	if (soup_uri)
		soup_uri_free (soup_uri);

	g_task_return_boolean (task, TRUE);
}

static void
mail_display_mail_uri_scheme_appeared_cb (WebKitURISchemeRequest *request)
{
	EMailDisplay *display;
	GTask *task;
	GCancellable *cancellable;
	const gchar *uri;

	display = E_MAIL_DISPLAY (webkit_uri_scheme_request_get_web_view (request));

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	cancellable = g_cancellable_new ();

	uri = webkit_uri_scheme_request_get_uri (request);

	task = g_task_new (
		display, cancellable,
		(GAsyncReadyCallback) mail_display_process_uri_scheme_finished_cb,
		request);

	g_task_set_task_data (task, g_object_ref (request), NULL);

	if (g_ascii_strncasecmp (uri, "mail://contact-photo", 20) == 0)
		g_task_run_in_thread (task, mail_display_process_contact_photo_uri_scheme_request);
	else
		g_task_run_in_thread (task, mail_display_process_mail_uri_scheme_request);

	g_object_unref (task);
	g_object_unref (cancellable);
}

static void
mail_display_initialize_web_context (void)
{
	WebKitWebContext *web_context = webkit_web_context_get_default ();

	webkit_web_context_register_uri_scheme (
		web_context,
		"evo-http",
		(WebKitURISchemeRequestCallback) mail_display_http_uri_scheme_appeared_cb,
		NULL,
		NULL);

	webkit_web_context_register_uri_scheme (
		web_context,
		"evo-https",
		(WebKitURISchemeRequestCallback) mail_display_http_uri_scheme_appeared_cb,
		NULL,
		NULL);

	webkit_web_context_register_uri_scheme (
		web_context,
		"mail",
		(WebKitURISchemeRequestCallback) mail_display_mail_uri_scheme_appeared_cb,
		NULL,
		NULL);

	webkit_web_context_register_uri_scheme (
		web_context,
		"cid",
		(WebKitURISchemeRequestCallback) mail_display_cid_uri_scheme_appeared_cb,
		NULL,
		NULL);
}

static void
e_mail_display_class_init (EMailDisplayClass *class)
{
	GObjectClass *object_class;
	EWebViewClass *web_view_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EMailDisplayPrivate));

	mail_display_initialize_web_context ();

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_display_constructed;
	object_class->set_property = mail_display_set_property;
	object_class->get_property = mail_display_get_property;
	object_class->dispose = mail_display_dispose;
	object_class->finalize = mail_display_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = mail_display_realize;
	widget_class->style_updated = mail_display_style_updated;
	widget_class->button_press_event = mail_display_button_press_event;

	web_view_class = E_WEB_VIEW_CLASS (class);
#if 0
	web_view_class->redirect_uri = mail_display_redirect_uri;
	web_view_class->suggest_filename = mail_display_suggest_filename;
#endif
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

	g_object_class_install_property (
		object_class,
		PROP_REMOTE_CONTENT,
		g_param_spec_object (
			"remote-content",
			"Mail Remote Content",
			NULL,
			E_TYPE_MAIL_REMOTE_CONTENT,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_display_init (EMailDisplay *display)
{
	GtkUIManager *ui_manager;
	GtkActionGroup *actions;

	display->priv = E_MAIL_DISPLAY_GET_PRIVATE (display);

	display->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	/* Set invalid mode so that MODE property initialization is run
	 * completely (see e_mail_display_set_mode) */
	display->priv->mode = E_MAIL_FORMATTER_MODE_INVALID;
	e_mail_display_set_mode (display, E_MAIL_FORMATTER_MODE_NORMAL);
	display->priv->force_image_load = FALSE;
	display->priv->scheduled_reload = 0;

	g_signal_connect (
		display, "decide-policy",
		G_CALLBACK (decide_policy_cb), NULL);
#if 0
	g_signal_connect (
		display, "resource-request-starting",
		G_CALLBACK (mail_display_resource_requested), NULL);
#endif
	g_signal_connect (
		display, "process-mailto",
		G_CALLBACK (mail_display_process_mailto), NULL);
#if 0
	g_signal_connect (
		display, "create-plugin-widget",
		G_CALLBACK (mail_display_plugin_widget_requested), NULL);
	e_signal_connect_notify (
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
#endif
	display->priv->settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_signal_connect_swapped (
		display->priv->settings , "changed::monospace-font",
		G_CALLBACK (e_mail_display_test_change_and_update_fonts_cb), display);
	g_signal_connect_swapped (
		display->priv->settings , "changed::variable-width-font",
		G_CALLBACK (e_mail_display_test_change_and_update_fonts_cb), display);
	g_signal_connect_swapped (
		display->priv->settings , "changed::use-custom-font",
		G_CALLBACK (e_mail_display_test_change_and_update_fonts_cb), display);

	g_signal_connect (
		display, "load-changed",
		G_CALLBACK (setup_dom_bindings), NULL);
	g_signal_connect (
		display, "load-changed",
		G_CALLBACK (mail_parts_bind_dom), NULL);

	actions = e_web_view_get_action_group (E_WEB_VIEW (display), "mailto");
	gtk_action_group_add_actions (
		actions, mailto_entries,
		G_N_ELEMENTS (mailto_entries), display);
	ui_manager = e_web_view_get_ui_manager (E_WEB_VIEW (display));
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	g_mutex_init (&display->priv->remote_content_lock);
	display->priv->remote_content = NULL;
	display->priv->skipped_remote_content_sites = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);
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
e_mail_display_new (EMailRemoteContent *remote_content)
{
	return g_object_new (E_TYPE_MAIL_DISPLAY,
		"remote-content", remote_content,
		NULL);
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

	e_signal_connect_notify (
		formatter, "notify::image-loading-policy",
		G_CALLBACK (formatter_image_loading_policy_changed_cb),
		display);

	e_signal_connect_notify_object (
		formatter, "notify::charset",
		G_CALLBACK (e_mail_display_reload), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::image-loading-policy",
		G_CALLBACK (e_mail_display_reload), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::mark-citations",
		G_CALLBACK (e_mail_display_reload), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::show-sender-photo",
		G_CALLBACK (e_mail_display_reload), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::show-real-date",
		G_CALLBACK (e_mail_display_reload), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::animate-images",
		G_CALLBACK (e_mail_display_reload), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::body-color",
		G_CALLBACK (e_mail_display_update_colors), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::citation-color",
		G_CALLBACK (e_mail_display_update_colors), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::frame-color",
		G_CALLBACK (e_mail_display_update_colors), display, G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		formatter, "notify::header-color",
		G_CALLBACK (e_mail_display_update_colors), display, G_CONNECT_SWAPPED);

	g_object_connect (formatter,
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

	e_mail_display_set_force_load_images (display, FALSE);

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

const gchar *
e_mail_display_get_selection_plain_text_sync (EMailDisplay *display,
                                              GCancellable *cancellable,
                                              GError **error)
{
	GDBusProxy *web_extension;

 	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);
/* FIXME XXX
 	if (!webkit_web_view_has_selection (WEBKIT_WEB_VIEW (display)))
 		return NULL;
*/
	web_extension = e_web_view_get_web_extension_proxy (E_WEB_VIEW (display));
	if (web_extension) {
		GVariant *result;
		const gchar *text_content = NULL;

		result = g_dbus_proxy_call_sync (
				web_extension,
				"GetDocumentContentText",
				g_variant_new (
					"(t)",
					webkit_web_view_get_page_id (
						WEBKIT_WEB_VIEW (display))),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				cancellable,
				error);

		if (result) {
			g_variant_get (result, "(&s)", &text_content);
			g_variant_unref (result);
			return text_content;
		}
	}

 	return NULL;
}

void
e_mail_display_load_images (EMailDisplay *display)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	e_mail_display_set_force_load_images (display, TRUE);
	e_web_view_reload (E_WEB_VIEW (display));
}

void
e_mail_display_set_force_load_images (EMailDisplay *display,
                                      gboolean force_load_images)
{
	GDBusProxy *web_extension;

 	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	web_extension = e_web_view_get_web_extension_proxy (E_WEB_VIEW (display));
	if (web_extension) {
		g_dbus_connection_call (
			g_dbus_proxy_get_connection (web_extension),
			g_dbus_proxy_get_name (web_extension),
			E_WEB_EXTENSION_OBJECT_PATH,
			"org.freedesktop.DBus.Properties",
			"Set",
			g_variant_new (
				"(ssv)",
				E_WEB_EXTENSION_INTERFACE,
				"ForceImageLoad",
				g_variant_new_boolean (force_load_images)),
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	}
}

gboolean
e_mail_display_has_skipped_remote_content_sites (EMailDisplay *display)
{
	gboolean has_any;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), FALSE);

	g_mutex_lock (&display->priv->remote_content_lock);

	has_any = g_hash_table_size (display->priv->skipped_remote_content_sites) > 0;

	g_mutex_unlock (&display->priv->remote_content_lock);

	return has_any;
}

/* Free with g_list_free_full (uris, g_free); */
GList *
e_mail_display_get_skipped_remote_content_sites (EMailDisplay *display)
{
	GList *uris, *link;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	g_mutex_lock (&display->priv->remote_content_lock);

	uris = g_hash_table_get_keys (display->priv->skipped_remote_content_sites);

	for (link = uris; link; link = g_list_next (link)) {
		link->data = g_strdup (link->data);
	}

	g_mutex_unlock (&display->priv->remote_content_lock);

	return uris;
}

EMailRemoteContent *
e_mail_display_ref_remote_content (EMailDisplay *display)
{
	EMailRemoteContent *remote_content;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	g_mutex_lock (&display->priv->remote_content_lock);

	remote_content = display->priv->remote_content;
	if (remote_content)
		g_object_ref (remote_content);

	g_mutex_unlock (&display->priv->remote_content_lock);

	return remote_content;
}

void
e_mail_display_set_remote_content (EMailDisplay *display,
				   EMailRemoteContent *remote_content)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));
	if (remote_content)
		g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (remote_content));

	g_mutex_lock (&display->priv->remote_content_lock);

	if (display->priv->remote_content == remote_content) {
		g_mutex_unlock (&display->priv->remote_content_lock);
		return;
	}

	g_clear_object (&display->priv->remote_content);
	display->priv->remote_content = remote_content ? g_object_ref (remote_content) : NULL;

	g_mutex_unlock (&display->priv->remote_content_lock);
}
