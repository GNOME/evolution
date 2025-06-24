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

#include "evolution-config.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gdk/gdk.h>
#include <camel/camel.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-formatter-enumtypes.h>
#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter-print.h>
#include <em-format/e-mail-formatter-utils.h>
#include <em-format/e-mail-part-attachment.h>
#include <em-format/e-mail-part-utils.h>

#include "shell/e-shell-utils.h"

#include "e-cid-request.h"
#include "e-http-request.h"
#include "e-mail-display-popup-extension.h"
#include "e-mail-notes.h"
#include "e-mail-reader.h"
#include "e-mail-request.h"
#include "e-mail-ui-session.h"
#include "em-composer-utils.h"
#include "em-utils.h"

#include "e-mail-display.h"

#define d(x)

typedef enum {
	E_ATTACHMENT_FLAG_VISIBLE	= (1 << 0),
	E_ATTACHMENT_FLAG_ZOOMED_TO_100	= (1 << 1)
} EAttachmentFlags;

typedef enum {
	E_MAGIC_SPACEBAR_CAN_GO_BOTTOM	= (1 << 0),
	E_MAGIC_SPACEBAR_CAN_GO_TOP	= (1 << 1)
} EMagicSpacebarFlags;

struct _EMailDisplayPrivate {
	EAttachmentStore *attachment_store;
	EAttachmentView *attachment_view;
	GHashTable *attachment_flags; /* EAttachment * ~> guint bit-or of EAttachmentFlags */
	GHashTable *cid_attachments; /* gchar *cid ~> EAttachment *; these are not part of the attachment store */

	GWeakRef mail_reader_weakref;

	EUIActionGroup *attachment_inline_group;
	EUIActionGroup *attachment_accel_action_group;
	GMenu *open_with_apps_menu;
	GHashTable *open_with_apps_hash; /* gchar *action_parameter ~> OpenWithData * */

	EMailPartList *part_list;
	EMailFormatterMode mode;
	EMailFormatter *formatter;

	gboolean headers_collapsable;
	gboolean headers_collapsed;
	gboolean force_image_load;
	gboolean has_secured_parts;
	gboolean skip_insecure_parts;

	GSList *insecure_part_ids;

	GSettings *settings;

	guint scheduled_reload;
	guint iframes_height_update_id;

	GHashTable *old_settings;

	GMutex remote_content_lock;
	EMailRemoteContent *remote_content;
	GHashTable *skipped_remote_content_sites;
	GHashTable *temporary_allow_remote_content; /* complete uri or site  */

	guint32 magic_spacebar_state; /* bit-or of EMagicSpacebarFlags */
	gboolean loaded;
};

enum {
	PROP_0,
	PROP_ATTACHMENT_STORE,
	PROP_ATTACHMENT_VIEW,
	PROP_FORMATTER,
	PROP_HEADERS_COLLAPSABLE,
	PROP_HEADERS_COLLAPSED,
	PROP_MODE,
	PROP_PART_LIST,
	PROP_REMOTE_CONTENT,
	PROP_MAIL_READER
};

enum {
	REMOTE_CONTENT_CLICKED,
	AUTOCRYPT_IMPORT_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static CamelDataCache *emd_global_http_cache = NULL;

static void e_mail_display_cid_resolver_init (ECidResolverInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailDisplay, e_mail_display, E_TYPE_WEB_VIEW,
	G_ADD_PRIVATE (EMailDisplay)
	G_IMPLEMENT_INTERFACE (E_TYPE_CID_RESOLVER, e_mail_display_cid_resolver_init))

typedef struct _OpenWithData {
	GAppInfo *app_info;
	EAttachment *attachment;
} OpenWithData;

static OpenWithData *
open_with_data_new (GAppInfo *app_info,
		    EAttachment *attachment)
{
	OpenWithData *data;

	data = g_new0 (OpenWithData, 1);
	data->app_info = app_info ? g_object_ref (app_info) : NULL;
	data->attachment = g_object_ref (attachment);

	return data;
}

static void
open_with_data_free (gpointer ptr)
{
	OpenWithData *data = ptr;

	if (data) {
		g_clear_object (&data->app_info);
		g_clear_object (&data->attachment);
		g_free (data);
	}
}

static void
mail_display_update_remote_content_buttons (EMailDisplay *self)
{
	if (e_mail_display_has_skipped_remote_content_sites (self)) {
		EWebView *web_view = E_WEB_VIEW (self);

		e_web_view_jsc_set_element_hidden (WEBKIT_WEB_VIEW (web_view),
			"", "__evo-remote-content-img-small", FALSE,
			e_web_view_get_cancellable (web_view));

		e_web_view_jsc_set_element_hidden (WEBKIT_WEB_VIEW (web_view),
			"", "__evo-remote-content-img-large", FALSE,
			e_web_view_get_cancellable (web_view));
	}
}

static void
e_mail_display_claim_skipped_uri (EMailDisplay *mail_display,
				  const gchar *uri)
{
	GUri *guri;
	const gchar *site;

	g_return_if_fail (E_IS_MAIL_DISPLAY (mail_display));
	g_return_if_fail (uri != NULL);

	/* Do not store anything if the user doesn't want to see the notification */
	if (!g_settings_get_boolean (mail_display->priv->settings, "notify-remote-content"))
		return;

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	if (!guri)
		return;

	site = g_uri_get_host (guri);
	if (site && *site) {
		gboolean update_buttons = FALSE;

		g_mutex_lock (&mail_display->priv->remote_content_lock);

		if (!g_hash_table_contains (mail_display->priv->skipped_remote_content_sites, site)) {
			g_hash_table_insert (mail_display->priv->skipped_remote_content_sites, g_strdup (site), NULL);

			update_buttons = mail_display->priv->loaded && g_hash_table_size (mail_display->priv->skipped_remote_content_sites) == 1;
		}

		g_mutex_unlock (&mail_display->priv->remote_content_lock);

		if (update_buttons)
			mail_display_update_remote_content_buttons (mail_display);
	}

	g_uri_unref (guri);
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
	GUri *guri;
	const gchar *site;
	gboolean can_download = FALSE;
	EMailRemoteContent *remote_content;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (mail_display), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	g_mutex_lock (&mail_display->priv->remote_content_lock);
	can_download = g_hash_table_contains (mail_display->priv->temporary_allow_remote_content, uri);
	if (!can_download && g_str_has_prefix (uri, "evo-"))
		can_download = g_hash_table_contains (mail_display->priv->temporary_allow_remote_content, uri + 4);
	g_mutex_unlock (&mail_display->priv->remote_content_lock);

	if (can_download)
		return can_download;

	remote_content = e_mail_display_ref_remote_content (mail_display);
	if (!remote_content)
		return FALSE;

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	if (!guri) {
		g_object_unref (remote_content);
		return FALSE;
	}

	site = g_uri_get_host (guri);
	if (site && *site) {
		can_download = e_mail_remote_content_has_site (remote_content, site);

		if (!can_download) {
			g_mutex_lock (&mail_display->priv->remote_content_lock);
			can_download = g_hash_table_contains (mail_display->priv->temporary_allow_remote_content, site);
			g_mutex_unlock (&mail_display->priv->remote_content_lock);
		}
	}

	g_uri_unref (guri);

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

		part_list = E_MAIL_DISPLAY (web_view)->priv->part_list;

		shell = e_shell_get_default ();
		em_utils_compose_new_message_with_mailto_and_selection (shell, mailto_uri,
			e_mail_part_list_get_folder (part_list),
			e_mail_part_list_get_message_uid (part_list));

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
		webkit_policy_decision_ignore (decision);
		return TRUE;
	}

	if (g_str_has_prefix (uri, "file://")) {
		gchar *filename;

		filename = g_filename_from_uri (uri, NULL, NULL);

		if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
			webkit_policy_decision_ignore (decision);
			/* FIXME WK2 Not sure if the request will be changed there */
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

static void
add_color_css_rule_for_web_view (EWebView *web_view,
				 const gchar *iframe_id,
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

	e_web_view_jsc_add_rule_into_style_sheet (
		WEBKIT_WEB_VIEW (web_view),
		iframe_id,
		"-e-mail-formatter-style-sheet",
		selector,
		style,
		e_web_view_get_cancellable (web_view));

	g_free (style);
	g_free (selector);
}

static void
initialize_web_view_colors (EMailDisplay *display,
			    const gchar *iframe_id)
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
			iframe_id,
			color_names[ii],
			color_value);

		gdk_rgba_free (color);
		g_free (color_value);
	}

	e_web_view_jsc_add_rule_into_style_sheet (
		WEBKIT_WEB_VIEW (display),
		iframe_id,
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-none",
		"border-width: 1px; border-style: solid",
		e_web_view_get_cancellable (E_WEB_VIEW (display)));

	/* the rgba values below were copied from e-formatter-secure-button */
	direction = gtk_widget_get_default_direction ();

	if (direction == GTK_TEXT_DIR_RTL)
		style = "border-width: 1px 1px 1px 4px; border-style: solid; border-color: rgba(53%, 73%, 53%, 1.0)";
	else
		style = "border-width: 1px 4px 1px 1px; border-style: solid; border-color: rgba(53%, 73%, 53%, 1.0)";
	e_web_view_jsc_add_rule_into_style_sheet (
		WEBKIT_WEB_VIEW (display),
		iframe_id,
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-good",
		style,
		e_web_view_get_cancellable (E_WEB_VIEW (display)));

	if (direction == GTK_TEXT_DIR_RTL)
		style = "border-width: 1px 1px 1px 4px; border-style: solid; border-color: rgba(73%, 53%, 53%, 1.0)";
	else
		style = "border-width: 1px 4px 1px 1px; border-style: solid; border-color: rgba(73%, 53%, 53%, 1.0)";
	e_web_view_jsc_add_rule_into_style_sheet (
		WEBKIT_WEB_VIEW (display),
		iframe_id,
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-bad",
		style,
		e_web_view_get_cancellable (E_WEB_VIEW (display)));

	if (direction == GTK_TEXT_DIR_RTL)
		style = "border-width: 1px 1px 1px 4px; border-style: solid; border-color: rgba(91%, 82%, 13%, 1.0)";
	else
		style = "border-width: 1px 4px 1px 1px; border-style: solid; border-color: rgba(91%, 82%, 13%, 1.0)";
	e_web_view_jsc_add_rule_into_style_sheet (
		WEBKIT_WEB_VIEW (display),
		iframe_id,
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-unknown",
		style,
		e_web_view_get_cancellable (E_WEB_VIEW (display)));

	if (direction == GTK_TEXT_DIR_RTL)
		style = "border-width: 1px 1px 1px 4px; border-style: solid; border-color: rgba(91%, 82%, 13%, 1.0)";
	else
		style = "border-width: 1px 4px 1px 1px; border-style: solid; border-color: rgba(91%, 82%, 13%, 1.0)";
	e_web_view_jsc_add_rule_into_style_sheet (
		WEBKIT_WEB_VIEW (display),
		iframe_id,
		"-e-mail-formatter-style-sheet",
		".-e-mail-formatter-frame-security-need-key",
		style,
		e_web_view_get_cancellable (E_WEB_VIEW (display)));
}

static gboolean
mail_display_can_use_frame_flattening (void)
{
	guint wk_major, wk_minor;

	wk_major = webkit_get_major_version ();
	wk_minor = webkit_get_minor_version ();

	/* The 2.38 is the last version, which supports frame-flattening;
	   prefer it over the manual and expensive calculations. */
	return (wk_major < 2) || (wk_major == 2 && wk_minor <= 38);
}

static gboolean
mail_display_iframes_height_update_cb (gpointer user_data)
{
	EMailDisplay *mail_display = user_data;

	mail_display->priv->iframes_height_update_id = 0;

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (mail_display), e_web_view_get_cancellable (E_WEB_VIEW (mail_display)),
		"Evo.MailDisplayUpdateIFramesHeight();");

	return G_SOURCE_REMOVE;
}

void
e_mail_display_schedule_iframes_height_update (EMailDisplay *mail_display)
{
	if (mail_display_can_use_frame_flattening ())
		return;

	if (mail_display->priv->iframes_height_update_id)
		g_source_remove (mail_display->priv->iframes_height_update_id);
	mail_display->priv->iframes_height_update_id = g_timeout_add (100, mail_display_iframes_height_update_cb, mail_display);
}

static void
mail_display_change_one_attachment_visibility (EMailDisplay *display,
					       EAttachment *attachment,
					       gboolean show,
					       gboolean flip)
{
	gchar *element_id;
	gchar *uri;
	guint flags;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (g_hash_table_contains (display->priv->attachment_flags, attachment));

	flags = GPOINTER_TO_UINT (g_hash_table_lookup (display->priv->attachment_flags, attachment));
	if (flip)
		show = !(flags & E_ATTACHMENT_FLAG_VISIBLE);

	if ((((flags & E_ATTACHMENT_FLAG_VISIBLE) != 0) ? 1 : 0) == (show ? 1 : 0))
		return;

	if (show)
		flags = flags | E_ATTACHMENT_FLAG_VISIBLE;
	else
		flags = flags & (~E_ATTACHMENT_FLAG_VISIBLE);
	g_hash_table_insert (display->priv->attachment_flags, attachment, GUINT_TO_POINTER (flags));

	element_id = g_strdup_printf ("attachment-wrapper-%p", attachment);
	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (display), e_web_view_get_cancellable (E_WEB_VIEW (display)),
		"Evo.MailDisplayShowAttachment(%s,%x);",
		element_id, show);
	g_free (element_id);

	element_id = g_strdup_printf ("attachment-expander-img-%p", attachment);
	uri = g_strdup_printf ("gtk-stock://%s?size=%d", show ? "go-down" : "go-next", GTK_ICON_SIZE_BUTTON);

	e_web_view_set_element_attribute (E_WEB_VIEW (display), element_id, NULL, "src", uri);

	g_free (element_id);
	g_free (uri);
}

static void
mail_display_change_attachment_visibility (EMailDisplay *display,
					   gboolean all,
					   gboolean show)
{
	EAttachmentView *view;
	GList *attachments, *link;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	view = e_mail_display_get_attachment_view (display);
	g_return_if_fail (view != NULL);

	if (all)
		attachments = e_attachment_store_get_attachments (display->priv->attachment_store);
	else
		attachments = view ? e_attachment_view_get_selected_attachments (view) : NULL;

	for (link = attachments; link; link = g_list_next (link)) {
		EAttachment *attachment = link->data;

		if (e_attachment_get_can_show (attachment))
			mail_display_change_one_attachment_visibility (display, attachment, show, FALSE);
	}

	g_list_free_full (attachments, g_object_unref);
}

static void
mail_attachment_change_zoom (EMailDisplay *display,
			     gboolean to_100_percent)
{
	EAttachmentView *view;
	GList *attachments, *link;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	view = e_mail_display_get_attachment_view (display);
	g_return_if_fail (view != NULL);

	attachments = view ? e_attachment_view_get_selected_attachments (view) : NULL;

	for (link = attachments; link; link = g_list_next (link)) {
		EAttachment *attachment = link->data;
		gchar *element_id;
		const gchar *max_width;
		guint flags;

		if (!E_IS_ATTACHMENT (attachment) ||
		    !g_hash_table_contains (display->priv->attachment_flags, attachment))
			continue;

		flags = GPOINTER_TO_UINT (g_hash_table_lookup (display->priv->attachment_flags, attachment));
		if ((((flags & E_ATTACHMENT_FLAG_ZOOMED_TO_100) != 0) ? 1 : 0) == (to_100_percent ? 1 : 0))
			continue;

		if (to_100_percent)
			flags = flags | E_ATTACHMENT_FLAG_ZOOMED_TO_100;
		else
			flags = flags & (~E_ATTACHMENT_FLAG_ZOOMED_TO_100);
		g_hash_table_insert (display->priv->attachment_flags, attachment, GUINT_TO_POINTER (flags));

		if (to_100_percent)
			max_width = NULL;
		else
			max_width = "100%";

		element_id = g_strdup_printf ("attachment-wrapper-%p::child", attachment);

		e_web_view_set_element_style_property (E_WEB_VIEW (display), element_id, "max-width", max_width);

		g_free (element_id);
	}

	g_list_free_full (attachments, g_object_unref);
}

static void
action_attachment_show_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailDisplay *display = user_data;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	mail_display_change_attachment_visibility (display, FALSE, TRUE);
}

static void
action_attachment_show_all_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailDisplay *display = user_data;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	mail_display_change_attachment_visibility (display, TRUE, TRUE);
}

static void
action_attachment_hide_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailDisplay *display = user_data;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	mail_display_change_attachment_visibility (display, FALSE, FALSE);
}

static void
action_attachment_hide_all_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailDisplay *display = user_data;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	mail_display_change_attachment_visibility (display, TRUE, FALSE);
}

static void
action_attachment_zoom_to_100_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailDisplay *display = user_data;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	mail_attachment_change_zoom (display, TRUE);
}

static void
action_attachment_zoom_to_window_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EMailDisplay *display = user_data;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	mail_attachment_change_zoom (display, FALSE);
}

static void
mail_display_allow_remote_content_site_cb (EUIAction *action,
					   GVariant *parameter,
					   gpointer user_data)
{
	EMailDisplay *display = user_data;
	EMailRemoteContent *remote_content;
	GUri *img_uri;
	const gchar *cursor_image_source;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	cursor_image_source = e_web_view_get_cursor_image_src (E_WEB_VIEW (display));
	if (!cursor_image_source)
		return;

	remote_content = e_mail_display_ref_remote_content (display);
	if (!remote_content)
		return;

	img_uri = g_uri_parse (cursor_image_source, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	if (img_uri && g_uri_get_host (img_uri)) {
		e_mail_remote_content_add_site (remote_content, g_uri_get_host (img_uri));
		e_mail_display_reload (display);
	}

	g_clear_pointer (&img_uri, g_uri_unref);
	g_clear_object (&remote_content);
}

static void
mail_display_load_remote_content_site_cb (EUIAction *action,
					  GVariant *parameter,
					  gpointer user_data)
{
	EMailDisplay *display = user_data;
	GUri *img_uri;
	const gchar *cursor_image_source;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	cursor_image_source = e_web_view_get_cursor_image_src (E_WEB_VIEW (display));
	if (!cursor_image_source)
		return;

	img_uri = g_uri_parse (cursor_image_source, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	if (img_uri && g_uri_get_host (img_uri)) {
		g_mutex_lock (&display->priv->remote_content_lock);
		g_hash_table_add (display->priv->temporary_allow_remote_content, g_strdup (g_uri_get_host (img_uri)));
		g_mutex_unlock (&display->priv->remote_content_lock);

		e_mail_display_reload (display);
	}

	g_clear_pointer (&img_uri, g_uri_unref);
}

static void
mail_display_load_remote_content_this_cb (EUIAction *action,
					  GVariant *parameter,
					  gpointer user_data)
{
	EMailDisplay *display = user_data;
	const gchar *cursor_image_source;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	cursor_image_source = e_web_view_get_cursor_image_src (E_WEB_VIEW (display));
	if (cursor_image_source) {
		g_mutex_lock (&display->priv->remote_content_lock);
		g_hash_table_add (display->priv->temporary_allow_remote_content, g_strdup (cursor_image_source));
		g_mutex_unlock (&display->priv->remote_content_lock);

		e_mail_display_reload (display);
	}
}

static void
call_attachment_save_handle_error (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	GtkWindow *window = user_data;

	g_return_if_fail (E_IS_ATTACHMENT (source_object));
	g_return_if_fail (!window || GTK_IS_WINDOW (window));

	e_attachment_save_handle_error (E_ATTACHMENT (source_object), result, window);

	g_clear_object (&window);
}

static void
mail_display_open_attachment (EMailDisplay *display,
			      EAttachment *attachment)
{
	GAppInfo *default_app;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (display));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	/* Either open in the default application... */
	default_app = e_attachment_ref_default_app (attachment);
	if (default_app || e_util_is_running_flatpak ()) {
		e_attachment_open_async (
			attachment, default_app, (GAsyncReadyCallback)
			e_attachment_open_handle_error, parent);

		g_clear_object (&default_app);
	} else {
		/* ...or save it */
		GList *attachments;
		EAttachmentStore *store;
		GFile *destination;

		store = e_mail_display_get_attachment_store (display);
		attachments = g_list_prepend (NULL, attachment);

		destination = e_attachment_store_run_save_dialog (store, attachments, parent);
		if (destination) {
			e_attachment_save_async (
				attachment, destination, (GAsyncReadyCallback)
				call_attachment_save_handle_error, parent ? g_object_ref (parent) : NULL);

			g_object_unref (destination);
		}

		g_list_free (attachments);
	}
}

static void
action_attachment_toggle_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailDisplay *display = user_data;
	EAttachmentStore *store;
	GList *attachments, *link;
	const gchar *name;
	guint index = ~0;
	guint len, ii;

	if (!gtk_widget_is_visible (GTK_WIDGET (display)))
		return;

	name = g_action_get_name (G_ACTION (action));
	g_return_if_fail (name != NULL);

	len = strlen (name);

	g_return_if_fail (len > 0);

	if (name[len - 1] >= '1' && name[len - 1] <= '9') {
		index = name[len - 1] - '1';
	}

	store = e_mail_display_get_attachment_store (display);

	if (index != ~0 && index >= e_attachment_store_get_num_attachments (store))
		return;

	attachments = e_attachment_store_get_attachments (display->priv->attachment_store);

	if (index == ~0) {
		guint n_shown = 0, n_can = 0, flags;

		for (link = attachments; link; link = g_list_next (link)) {
			EAttachment *attachment = link->data;

			if (e_attachment_get_can_show (attachment)) {
				n_can++;

				flags = GPOINTER_TO_UINT (g_hash_table_lookup (display->priv->attachment_flags, attachment));

				if ((flags & E_ATTACHMENT_FLAG_VISIBLE) != 0)
					n_shown++;

				if (n_can != n_shown)
					break;
			}
		}

		mail_display_change_attachment_visibility (display, TRUE, n_shown != n_can);
	} else {
		for (link = attachments, ii = 0; link; ii++, link = g_list_next (link)) {
			EAttachment *attachment = link->data;

			if (index == ii) {
				if (e_attachment_get_can_show (attachment))
					mail_display_change_one_attachment_visibility (display, attachment, FALSE, TRUE);
				else
					mail_display_open_attachment (display, attachment);
				break;
			}
		}
	}

	g_list_free_full (attachments, g_object_unref);
}

static EAttachment *
mail_display_ref_attachment_from_element (EMailDisplay *display,
					  const gchar *element_value)
{
	EAttachment *attachment = NULL;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);
	g_return_val_if_fail (element_value != NULL, NULL);

	e_mail_part_list_queue_parts (display->priv->part_list, NULL, &queue);
	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = E_MAIL_PART (link->data);

		if (E_IS_MAIL_PART_ATTACHMENT (part)) {
			EAttachment *adept;
			gboolean can_use;
			gchar *tmp;

			adept = e_mail_part_attachment_ref_attachment (E_MAIL_PART_ATTACHMENT (part));

			tmp = g_strdup_printf ("%p", adept);
			can_use = g_strcmp0 (tmp, element_value) == 0;
			g_free (tmp);

			if (can_use) {
				attachment = adept;
				break;
			}

			g_clear_object (&adept);
		}
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	return attachment;
}

static void
mail_display_attachment_expander_clicked_cb (EWebView *web_view,
					     const gchar *iframe_id,
					     const gchar *element_id,
					     const gchar *element_class,
					     const gchar *element_value,
					     const GtkAllocation *element_position,
					     gpointer user_data)
{
	EMailDisplay *display;
	EAttachment *attachment;

	g_return_if_fail (E_IS_MAIL_DISPLAY (web_view));
	g_return_if_fail (element_class != NULL);
	g_return_if_fail (element_value != NULL);
	g_return_if_fail (element_position != NULL);

	display = E_MAIL_DISPLAY (web_view);
	attachment = mail_display_ref_attachment_from_element (display, element_value);

	if (attachment) {
		if (e_attachment_get_can_show (attachment)) {
			/* Flip the current 'visible' state */
			mail_display_change_one_attachment_visibility (display, attachment, FALSE, TRUE);
		} else {
			mail_display_open_attachment (display, attachment);
		}
	}

	g_clear_object (&attachment);
}

static void
mail_display_attachment_inline_update_actions (EMailDisplay *display)
{
	EUIActionGroup *action_group;
	EUIAction *action;
	GList *attachments, *link;
	EAttachmentView *view;
	guint n_shown = 0;
	guint n_hidden = 0;
	gboolean can_show = FALSE;
	gboolean shown = FALSE;
	gboolean is_image = FALSE;
	gboolean zoomed_to_100 = FALSE;
	gboolean visible;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	action_group = display->priv->attachment_inline_group;
	g_return_if_fail (action_group != NULL);

	attachments = e_attachment_store_get_attachments (display->priv->attachment_store);

	for (link = attachments; link; link = g_list_next (link)) {
		EAttachment *attachment = link->data;
		guint32 flags;

		if (!e_attachment_get_can_show (attachment))
			continue;

		flags = GPOINTER_TO_UINT (g_hash_table_lookup (display->priv->attachment_flags, attachment));
		if ((flags & E_ATTACHMENT_FLAG_VISIBLE) != 0)
			n_shown++;
		else
			n_hidden++;
	}

	g_list_free_full (attachments, g_object_unref);

	view = e_mail_display_get_attachment_view (display);
	attachments = view ? e_attachment_view_get_selected_attachments (view) : NULL;

	if (attachments && attachments->data && !attachments->next) {
		EAttachment *attachment;
		guint32 flags;

		attachment = attachments->data;
		can_show = e_attachment_get_can_show (attachment);

		if (can_show) {
			gchar *mime_type;

			mime_type = e_attachment_dup_mime_type (attachment);
			is_image = mime_type && g_ascii_strncasecmp (mime_type, "image/", 6) == 0;

			g_free (mime_type);
		}

		flags = GPOINTER_TO_UINT (g_hash_table_lookup (display->priv->attachment_flags, attachment));
		shown = (flags & E_ATTACHMENT_FLAG_VISIBLE) != 0;
		zoomed_to_100 = (flags & E_ATTACHMENT_FLAG_ZOOMED_TO_100) != 0;
	}
	g_list_free_full (attachments, g_object_unref);

	action = e_ui_action_group_get_action (action_group, "show");
	e_ui_action_set_visible (action, can_show && !shown);

	/* Show this action if there are multiple viewable
	 * attachments, and at least one of them is hidden. */
	visible = (n_shown + n_hidden > 1) && (n_hidden > 0);
	action = e_ui_action_group_get_action (action_group, "show-all");
	e_ui_action_set_visible (action, visible);

	action = e_ui_action_group_get_action (action_group, "hide");
	e_ui_action_set_visible (action, can_show && shown);

	/* Show this action if there are multiple viewable
	 * attachments, and at least one of them is shown. */
	visible = (n_shown + n_hidden > 1) && (n_shown > 0);
	action = e_ui_action_group_get_action (action_group, "hide-all");
	e_ui_action_set_visible (action, visible);

	action = e_ui_action_group_get_action (action_group, "zoom-to-100");
	e_ui_action_set_visible (action, can_show && shown && is_image && !zoomed_to_100);

	action = e_ui_action_group_get_action (action_group, "zoom-to-window");
	e_ui_action_set_visible (action, can_show && shown && is_image && zoomed_to_100);
}

static void
mail_display_attachment_menu_freed_cb (gpointer user_data,
				       GObject *freed_menu)
{
	EUIActionGroup *group = user_data;

	g_return_if_fail (E_IS_UI_ACTION_GROUP (group));

	e_ui_action_group_set_visible (group, FALSE);

	g_clear_object (&group);
}

static void
mail_display_attachment_select_path (EAttachmentView *view,
				     EAttachment *attachment)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	EAttachmentStore *store;

	g_return_if_fail (E_IS_ATTACHMENT_VIEW (view));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	store = e_attachment_view_get_store (view);
	g_return_if_fail (e_attachment_store_find_attachment_iter (store, attachment, &iter));

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);

	e_attachment_view_unselect_all (view);
	e_attachment_view_select_path (view, path);

	gtk_tree_path_free (path);
}

static void
mail_display_attachment_menu_clicked_cb (EWebView *web_view,
					 const gchar *iframe_id,
					 const gchar *element_id,
					 const gchar *element_class,
					 const gchar *element_value,
					 const GtkAllocation *element_position,
					 gpointer user_data)
{
	EMailDisplay *display;
	EAttachmentView *view;
	EAttachment *attachment;

	g_return_if_fail (E_IS_MAIL_DISPLAY (web_view));
	g_return_if_fail (element_class != NULL);
	g_return_if_fail (element_value != NULL);
	g_return_if_fail (element_position != NULL);

	display = E_MAIL_DISPLAY (web_view);
	view = e_mail_display_get_attachment_view (display);
	attachment = mail_display_ref_attachment_from_element (display, element_value);

	if (view && attachment) {
		GtkWidget *popup_menu;

		mail_display_attachment_select_path (view, attachment);
		mail_display_attachment_inline_update_actions (display);
		e_ui_action_group_set_visible (display->priv->attachment_inline_group, TRUE);

		e_attachment_view_update_actions (view);

		popup_menu = e_attachment_view_get_popup_menu (view);

		e_ui_manager_add_action_groups_to_widget (e_web_view_get_ui_manager (web_view), popup_menu);
		e_ui_manager_add_action_groups_to_widget (e_attachment_view_get_ui_manager (view), popup_menu);

		g_object_weak_ref (G_OBJECT (popup_menu), mail_display_attachment_menu_freed_cb,
			g_object_ref (display->priv->attachment_inline_group));

		g_object_set (GTK_MENU (popup_menu),
		              "anchor-hints", (GDK_ANCHOR_FLIP_Y |
		                               GDK_ANCHOR_SLIDE |
		                               GDK_ANCHOR_RESIZE),
		              NULL);

		gtk_menu_popup_at_rect (GTK_MENU (popup_menu),
		                        gtk_widget_get_parent_window (GTK_WIDGET (display)),
		                        element_position,
		                        GDK_GRAVITY_SOUTH_WEST,
		                        GDK_GRAVITY_NORTH_WEST,
		                        NULL);
	}

	g_clear_object (&attachment);
}

static void
mail_display_attachment_added_cb (EAttachmentStore *store,
				  EAttachment *attachment,
				  gpointer user_data)
{
	EMailDisplay *display = user_data;
	guint flags;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	flags = e_attachment_get_initially_shown (attachment) ? E_ATTACHMENT_FLAG_VISIBLE : 0;

	g_hash_table_insert (display->priv->attachment_flags, attachment, GUINT_TO_POINTER (flags));
}

static void
mail_display_attachment_removed_cb (EAttachmentStore *store,
				    EAttachment *attachment,
				    gpointer user_data)
{
	EMailDisplay *display = user_data;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	g_hash_table_remove (display->priv->attachment_flags, attachment);
}

static void
mail_display_remote_content_clicked_cb (EWebView *web_view,
					const gchar *iframe_id,
					const gchar *element_id,
					const gchar *element_class,
					const gchar *element_value,
					const GtkAllocation *element_position,
					gpointer user_data)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (web_view));

	g_signal_emit (web_view, signals[REMOTE_CONTENT_CLICKED], 0, element_position, NULL);
}

static void
mail_display_autocrypt_import_clicked_cb (EWebView *web_view,
					  const gchar *iframe_id,
					  const gchar *element_id,
					  const gchar *element_class,
					  const gchar *element_value,
					  const GtkAllocation *element_position,
					  gpointer user_data)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (web_view));

	g_signal_emit (web_view, signals[AUTOCRYPT_IMPORT_CLICKED], 0, element_position, NULL);
}

static void
mail_display_manage_insecure_parts_clicked_cb (EWebView *web_view,
					       const gchar *iframe_id,
					       const gchar *element_id,
					       const gchar *element_class,
					       const gchar *element_value,
					       const GtkAllocation *element_position,
					       gpointer user_data)
{
	EMailDisplay *mail_display;
	const gchar *part_id_prefix = element_value;
	GSList *link;
	GString *jsc_cmd;

	g_return_if_fail (E_IS_MAIL_DISPLAY (web_view));
	g_return_if_fail (element_id != NULL);
	g_return_if_fail (element_value != NULL);

	mail_display = E_MAIL_DISPLAY (web_view);

	if (!mail_display->priv->insecure_part_ids)
		return;

	mail_display->priv->skip_insecure_parts = !g_str_has_prefix (element_id, "show:");

	jsc_cmd = g_string_new ("");

	e_web_view_jsc_printf_script_gstring (jsc_cmd,
		"Evo.MailDisplayManageInsecureParts(%s,%s,%x,[",
		iframe_id,
		part_id_prefix,
		!mail_display->priv->skip_insecure_parts);

	for (link = mail_display->priv->insecure_part_ids; link; link = g_slist_next (link)) {
		const gchar *part_id = link->data;

		if (link != mail_display->priv->insecure_part_ids)
			g_string_append_c (jsc_cmd, ',');

		e_web_view_jsc_printf_script_gstring (jsc_cmd, "%s", part_id);
	}

	g_string_append (jsc_cmd, "]);");

	e_web_view_jsc_run_script_take (WEBKIT_WEB_VIEW (web_view),
		g_string_free (jsc_cmd, FALSE),
		e_web_view_get_cancellable (web_view));
}

static void
mail_display_load_changed_cb (WebKitWebView *wk_web_view,
			      WebKitLoadEvent load_event,
			      gpointer user_data)
{
	EMailDisplay *display;

	g_return_if_fail (E_IS_MAIL_DISPLAY (wk_web_view));

	display = E_MAIL_DISPLAY (wk_web_view);

	if (load_event == WEBKIT_LOAD_STARTED) {
		display->priv->magic_spacebar_state = 0;
		e_mail_display_cleanup_skipped_uris (display);
		e_attachment_store_remove_all (display->priv->attachment_store);
		e_attachment_bar_clear_possible_attachments (E_ATTACHMENT_BAR (display->priv->attachment_view));
		g_hash_table_remove_all (display->priv->cid_attachments);
		display->priv->loaded = FALSE;
	} else if (load_event == WEBKIT_LOAD_FINISHED) {
		display->priv->loaded = TRUE;
		mail_display_update_remote_content_buttons (display);
	}
}

static void
mail_display_content_loaded_cb (EWebView *web_view,
				const gchar *iframe_id,
				gpointer user_data)
{
	EMailDisplay *mail_display;
	GList *attachments, *link;
	gchar *citation_color = NULL;

	g_return_if_fail (E_IS_MAIL_DISPLAY (web_view));

	mail_display = E_MAIL_DISPLAY (web_view);

	attachments = e_attachment_store_get_attachments (mail_display->priv->attachment_store);

	for (link = attachments; link; link = g_list_next (link)) {
		EAttachment *attachment = link->data;

		if (e_attachment_get_can_show (attachment)) {
			gchar *mime_type;

			mime_type = e_attachment_dup_mime_type (attachment);

			if (mime_type && g_ascii_strncasecmp (mime_type, "image/", 6) == 0 &&
			    !webkit_web_view_can_show_mime_type (WEBKIT_WEB_VIEW (web_view), mime_type))
				e_attachment_set_can_show (attachment, FALSE);

			g_free (mime_type);
		}
	}

	g_list_free_full (attachments, g_object_unref);

	initialize_web_view_colors (mail_display, iframe_id);

	if (!iframe_id || !*iframe_id) {
		e_web_view_register_element_clicked (web_view, "attachment-expander",
			mail_display_attachment_expander_clicked_cb, NULL);
		e_web_view_register_element_clicked (web_view, "attachment-menu",
			mail_display_attachment_menu_clicked_cb, NULL);
		e_web_view_register_element_clicked (web_view, "__evo-remote-content-img",
			mail_display_remote_content_clicked_cb, NULL);
		e_web_view_register_element_clicked (web_view, "manage-insecure-parts",
			mail_display_manage_insecure_parts_clicked_cb, NULL);
		e_web_view_register_element_clicked (web_view, "__evo-autocrypt-import-img",
			mail_display_autocrypt_import_clicked_cb, NULL);
	}

	if (g_settings_get_boolean (mail_display->priv->settings, "mark-citations")) {
		GdkRGBA rgba;

		citation_color = g_settings_get_string (mail_display->priv->settings, "citation-color");

		if (!citation_color || !gdk_rgba_parse (&rgba, citation_color)) {
			g_free (citation_color);
			citation_color = NULL;
		} else {
			g_free (citation_color);
			citation_color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));
		}
	}

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
		"Evo.MailDisplayBindDOM(%s, %s);", iframe_id, citation_color);

	g_free (citation_color);

	if (mail_display->priv->part_list) {
		if (!iframe_id || !*iframe_id) {
			GQueue queue = G_QUEUE_INIT;
			GList *head;

			e_mail_part_list_queue_parts (mail_display->priv->part_list, NULL, &queue);
			head = g_queue_peek_head_link (&queue);

			for (link = head; link; link = g_list_next (link)) {
				EMailPart *part = E_MAIL_PART (link->data);

				e_mail_part_content_loaded (part, web_view, NULL);
			}

			while (!g_queue_is_empty (&queue))
				g_object_unref (g_queue_pop_head (&queue));
		} else {
			EMailPart *part;

			part = e_mail_part_list_ref_part (mail_display->priv->part_list, iframe_id);

			if (part)
				e_mail_part_content_loaded (part, web_view, iframe_id);

			g_clear_object (&part);
		}

		if (mail_display->priv->has_secured_parts &&
		    mail_display->priv->skip_insecure_parts) {
			GSList *slink;

			for (slink = mail_display->priv->insecure_part_ids; slink; slink = g_slist_next (slink)) {
				e_web_view_jsc_set_element_hidden (WEBKIT_WEB_VIEW (web_view),
					"*", slink->data, TRUE,
					e_web_view_get_cancellable (web_view));
			}
		}

		if (e_mail_part_list_get_autocrypt_keys (mail_display->priv->part_list)) {
			e_web_view_jsc_set_element_hidden (WEBKIT_WEB_VIEW (web_view),
				"", "__evo-autocrypt-import-img-small", FALSE,
				e_web_view_get_cancellable (web_view));

			e_web_view_jsc_set_element_hidden (WEBKIT_WEB_VIEW (web_view),
				"", "__evo-autocrypt-import-img-large", FALSE,
				e_web_view_get_cancellable (web_view));
		}
	}

	mail_display_update_remote_content_buttons (mail_display);

	/* Re-grab the focus, which is needed for the caret mode to show the cursor */
	if (e_web_view_get_caret_mode (web_view) &&
	    gtk_widget_has_focus (GTK_WIDGET (web_view))) {
		GtkWidget *toplevel, *widget = GTK_WIDGET (web_view);

		toplevel = gtk_widget_get_toplevel (widget);

		if (GTK_IS_WINDOW (toplevel)) {
			gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
			gtk_widget_grab_focus (widget);
		}
	}

	e_mail_display_schedule_iframes_height_update (mail_display);
}

static void
action_open_with_app_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailDisplay *self = user_data;
	GAppInfo *app_info;
	OpenWithData *data;
	gpointer parent;

	data = g_hash_table_lookup (self->priv->open_with_apps_hash, GINT_TO_POINTER (g_variant_get_int32 (parameter)));
	g_return_if_fail (data != NULL);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (self));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	app_info = data->app_info;

	if (!app_info && !e_util_is_running_flatpak ()) {
		GtkWidget *dialog;
		GFileInfo *file_info;
		const gchar *content_type;

		file_info = e_attachment_ref_file_info (data->attachment);
		g_return_if_fail (file_info != NULL);

		content_type = g_file_info_get_content_type (file_info);

		dialog = gtk_app_chooser_dialog_new_for_content_type (parent, 0, content_type);
		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
			GtkAppChooser *app_chooser = GTK_APP_CHOOSER (dialog);
			app_info = gtk_app_chooser_get_app_info (app_chooser);
		}

		gtk_widget_destroy (dialog);
		g_object_unref (file_info);
	} else if (app_info) {
		g_object_ref (app_info);
	}

	if (app_info) {
		e_attachment_open_async (data->attachment, app_info,
			(GAsyncReadyCallback) e_attachment_open_handle_error, parent);
		g_object_unref (app_info);
	}
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

		case PROP_REMOTE_CONTENT:
			e_mail_display_set_remote_content (
				E_MAIL_DISPLAY (object),
				g_value_get_object (value));
			return;

		case PROP_MAIL_READER:
			g_weak_ref_set (&(E_MAIL_DISPLAY (object)->priv->mail_reader_weakref), g_value_get_object (value));
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
		case PROP_ATTACHMENT_STORE:
			g_value_set_object (
				value,
				e_mail_display_get_attachment_store (
				E_MAIL_DISPLAY (object)));
			return;

		case PROP_ATTACHMENT_VIEW:
			g_value_set_object (
				value,
				e_mail_display_get_attachment_view (
				E_MAIL_DISPLAY (object)));
			return;

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

		case PROP_MAIL_READER:
			g_value_take_object (value,
				e_mail_display_ref_mail_reader (E_MAIL_DISPLAY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_display_dispose (GObject *object)
{
	EMailDisplay *self = E_MAIL_DISPLAY (object);

	if (self->priv->scheduled_reload > 0) {
		g_source_remove (self->priv->scheduled_reload);
		self->priv->scheduled_reload = 0;
	}

	if (self->priv->iframes_height_update_id > 0) {
		g_source_remove (self->priv->iframes_height_update_id);
		self->priv->iframes_height_update_id = 0;
	}

	if (self->priv->settings != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->settings, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
	}

	if (self->priv->attachment_store) {
		/* To have called the mail_display_attachment_removed_cb() before it's disconnected */
		e_attachment_store_remove_all (self->priv->attachment_store);

		g_signal_handlers_disconnect_by_func (self->priv->attachment_store,
			G_CALLBACK (mail_display_attachment_added_cb), object);

		g_signal_handlers_disconnect_by_func (self->priv->attachment_store,
			G_CALLBACK (mail_display_attachment_removed_cb), object);
	}

	g_clear_object (&self->priv->part_list);
	g_clear_object (&self->priv->formatter);
	g_clear_object (&self->priv->settings);
	g_clear_object (&self->priv->attachment_store);
	g_clear_object (&self->priv->attachment_view);
	g_clear_object (&self->priv->attachment_inline_group);
	g_clear_object (&self->priv->attachment_accel_action_group);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_display_parent_class)->dispose (object);
}

static void
mail_display_finalize (GObject *object)
{
	EMailDisplay *self = E_MAIL_DISPLAY (object);

	g_clear_pointer (&self->priv->old_settings, g_hash_table_destroy);

	g_mutex_lock (&self->priv->remote_content_lock);
	g_clear_pointer (&self->priv->skipped_remote_content_sites, g_hash_table_destroy);
	g_clear_pointer (&self->priv->temporary_allow_remote_content, g_hash_table_destroy);
	g_clear_object (&self->priv->open_with_apps_menu);
	g_clear_pointer (&self->priv->open_with_apps_hash, g_hash_table_unref);
	g_slist_free_full (self->priv->insecure_part_ids, g_free);
	g_hash_table_destroy (self->priv->attachment_flags);
	g_hash_table_destroy (self->priv->cid_attachments);
	g_clear_object (&self->priv->remote_content);
	g_mutex_unlock (&self->priv->remote_content_lock);
	g_mutex_clear (&self->priv->remote_content_lock);
	g_weak_ref_clear (&self->priv->mail_reader_weakref);

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
		if (monospace)
			*monospace = NULL;
		if (variable)
			*variable = NULL;
		return;
	}

	monospace_font = g_settings_get_string (settings, "monospace-font");
	variable_font = g_settings_get_string (settings, "variable-width-font");

	if (monospace)
		*monospace = (monospace_font != NULL) ? pango_font_description_from_string (monospace_font) : NULL;
	if (variable)
		*variable = (variable_font != NULL) ? pango_font_description_from_string (variable_font) : NULL;

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
mail_display_headers_collapsed_cb (WebKitUserContentManager *manager,
				   WebKitJavascriptResult *js_result,
				   gpointer user_data)
{
	EMailDisplay *mail_display = user_data;
	JSCValue *jsc_value;

	g_return_if_fail (mail_display != NULL);
	g_return_if_fail (js_result != NULL);

	jsc_value = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_boolean (jsc_value));

	e_mail_display_set_headers_collapsed (mail_display, jsc_value_to_boolean (jsc_value));
}

static void
mail_display_magic_spacebar_state_changed_cb (WebKitUserContentManager *manager,
					      WebKitJavascriptResult *js_result,
					      gpointer user_data)
{
	EMailDisplay *mail_display = user_data;
	JSCValue *jsc_value;

	g_return_if_fail (mail_display != NULL);
	g_return_if_fail (js_result != NULL);

	jsc_value = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_number (jsc_value));

	mail_display->priv->magic_spacebar_state = jsc_value_to_int32 (jsc_value);
}

static void
mail_display_schedule_iframes_height_update_cb (WebKitUserContentManager *manager,
						WebKitJavascriptResult *js_result,
						gpointer user_data)
{
	EMailDisplay *mail_display = user_data;

	g_return_if_fail (mail_display != NULL);

	e_mail_display_schedule_iframes_height_update (mail_display);
}

static gboolean
e_mail_display_ui_manager_create_item_cb (EUIManager *manager,
					  EUIElement *elem,
					  EUIAction *action,
					  EUIElementKind for_kind,
					  GObject **out_item,
					  gpointer user_data)
{
	EMailDisplay *self = user_data;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (self), FALSE);

	if (for_kind != E_UI_ELEMENT_KIND_MENU ||
	    g_strcmp0 (g_action_get_name (G_ACTION (action)), "EMailDisplay::open-with-app") != 0)
		return FALSE;

	if (self->priv->open_with_apps_menu)
		*out_item = G_OBJECT (g_menu_item_new_section (NULL, G_MENU_MODEL (self->priv->open_with_apps_menu)));
	else
		*out_item = NULL;

	return TRUE;
}

static void
mail_display_constructed (GObject *object)
{
	EContentRequest *content_request;
	WebKitUserContentManager *manager;
	EWebView *web_view;
	EMailDisplay *display;
	EUIManager *ui_manager;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_display_parent_class)->constructed (object);

	if (mail_display_can_use_frame_flattening ()) {
		g_object_set (webkit_web_view_get_settings (WEBKIT_WEB_VIEW (object)),
			"enable-frame-flattening", TRUE,
			NULL);
	}

	display = E_MAIL_DISPLAY (object);
	web_view = E_WEB_VIEW (object);

	e_web_view_update_fonts (web_view);

	content_request = e_http_request_new ();
	e_web_view_register_content_request_for_scheme (web_view, "evo-http", content_request);
	e_web_view_register_content_request_for_scheme (web_view, "evo-https", content_request);
	g_object_unref (content_request);

	content_request = e_mail_request_new ();
	e_binding_bind_property (display, "scale-factor",
		content_request, "scale-factor",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
	e_web_view_register_content_request_for_scheme (web_view, "mail", content_request);
	g_object_unref (content_request);

	content_request = e_cid_request_new ();
	e_web_view_register_content_request_for_scheme (web_view, "cid", content_request);
	g_object_unref (content_request);

	display->priv->attachment_view = E_ATTACHMENT_VIEW (g_object_ref_sink (e_attachment_bar_new (display->priv->attachment_store)));

	ui_manager = e_attachment_view_get_ui_manager (display->priv->attachment_view);
	if (ui_manager) {
		static const gchar *attachment_popup_eui =
			"<eui>"
			  "<menu id='context'>"
			    "<placeholder id='inline-actions'>"
			      "<item action='zoom-to-100'/>"
			      "<item action='zoom-to-window'/>"
			      "<item action='show'/>"
			      "<item action='show-all'/>"
			      "<separator/>"
			      "<item action='hide'/>"
			      "<item action='hide-all'/>"
			    "</placeholder>"
			  "</menu>"
			"</eui>";

		static const EUIActionEntry attachment_inline_entries[] = {

			{ "hide",
			  NULL,
			  N_("_Hide"),
			  NULL,
			  NULL,
			  action_attachment_hide_cb, NULL, NULL, NULL },

			{ "hide-all",
			  NULL,
			  N_("Hid_e All"),
			  NULL,
			  NULL,
			  action_attachment_hide_all_cb, NULL, NULL, NULL },

			{ "show",
			  NULL,
			  N_("_View Inline"),
			  NULL,
			  NULL,
			  action_attachment_show_cb, NULL, NULL, NULL },

			{ "show-all",
			  NULL,
			  N_("Vie_w All Inline"),
			  NULL,
			  NULL,
			  action_attachment_show_all_cb, NULL, NULL, NULL },

			{ "zoom-to-100",
			  NULL,
			  N_("_Zoom to 100%"),
			  NULL,
			  N_("Zoom the image to its natural size"),
			  action_attachment_zoom_to_100_cb, NULL, NULL, NULL },

			{ "zoom-to-window",
			  NULL,
			  N_("_Zoom to window"),
			  NULL,
			  N_("Zoom large images to not be wider than the window width"),
			  action_attachment_zoom_to_window_cb, NULL, NULL, NULL }
		};

		e_ui_manager_add_actions_with_eui_data (ui_manager, "e-mail-display-attachment-inline", NULL,
			attachment_inline_entries, G_N_ELEMENTS (attachment_inline_entries), display, attachment_popup_eui);

		display->priv->attachment_inline_group = g_object_ref (e_ui_manager_get_action_group (ui_manager, "e-mail-display-attachment-inline"));
		e_ui_action_group_set_visible (display->priv->attachment_inline_group, FALSE);

		gtk_widget_insert_action_group (GTK_WIDGET (display), e_ui_action_group_get_name (display->priv->attachment_inline_group),
			G_ACTION_GROUP (display->priv->attachment_inline_group));
	}

	ui_manager = e_web_view_get_ui_manager (web_view);
	if (ui_manager) {
		static const gchar *eui =
			"<eui>"
			  "<menu id='context'>"
			    "<placeholder id='custom-actions-1'>"
			      "<item action='add-to-address-book'/>"
			      "<item action='send-reply'/>"
			    "</placeholder>"
			    "<placeholder id='custom-actions-3'>"
			      "<item action='allow-remote-content-site'/>"
			      "<item action='load-remote-content-site'/>"
			      "<item action='load-remote-content-this'/>"
			      "<submenu action='search-folder-menu'>"
				"<item action='search-folder-recipient'/>"
				"<item action='search-folder-sender'/>"
			      "</submenu>"
			      "<item action='EMailDisplay::open-with-app'/>"
			    "</placeholder>"
			  "</menu>"
			"</eui>";

		static const EUIActionEntry accel_entries[] = {

			{ "attachment-toggle-all",
			  NULL,
			  "Toggle Attachment All",
			  "<Primary><Alt>0",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-1",
			  NULL,
			  "Toggle Attachment 1",
			  "<Primary><Alt>1",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-2",
			  NULL,
			  "Toggle Attachment 2",
			  "<Primary><Alt>2",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-3",
			  NULL,
			  "Toggle Attachment 3",
			  "<Primary><Alt>3",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-4",
			  NULL,
			  "Toggle Attachment 4",
			  "<Primary><Alt>4",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-5",
			  NULL,
			  "Toggle Attachment 5",
			  "<Primary><Alt>5",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-6",
			  NULL,
			  "Toggle Attachment 6",
			  "<Primary><Alt>6",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-7",
			  NULL,
			  "Toggle Attachment 7",
			  "<Primary><Alt>7",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-8",
			  NULL,
			  "Toggle Attachment 8",
			  "<Primary><Alt>8",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL },

			{ "attachment-toggle-9",
			  NULL,
			  "Toggle Attachment 9",
			  "<Primary><Alt>9",
			  NULL,
			  action_attachment_toggle_cb, NULL, NULL, NULL }
		};

		static const EUIActionEntry image_entries[] = {

			{ "allow-remote-content-site",
			  NULL,
			  "Allow remote content from...", /* placeholder text, do not localize */
			  NULL,
			  NULL,
			  mail_display_allow_remote_content_site_cb, NULL, NULL, NULL },

			{ "load-remote-content-site",
			  NULL,
			  "Load remote content from...", /* placeholder text, do not localize */
			  NULL,
			  NULL,
			  mail_display_load_remote_content_site_cb, NULL, NULL, NULL },

			{ "load-remote-content-this",
			  NULL,
			  N_("Load this image"),
			  NULL,
			  NULL,
			  mail_display_load_remote_content_this_cb, NULL, NULL, NULL }
		};

		static const EUIActionEntry mailto_entries[] = {

			{ "add-to-address-book",
			  "contact-new",
			  N_("_Add to Address Book…"),
			  NULL,
			  NULL,
			  NULL /* Handled by EMailReader */, NULL, NULL, NULL },

			{ "search-folder-recipient",
			  NULL,
			  N_("_To This Address"),
			  NULL,
			  NULL,
			  NULL /* Handled by EMailReader */, NULL, NULL, NULL },

			{ "search-folder-sender",
			  NULL,
			  N_("_From This Address"),
			  NULL,
			  NULL,
			  NULL /* Handled by EMailReader */, NULL, NULL, NULL },

			{ "send-reply",
			  NULL,
			  N_("Send _Reply To…"),
			  NULL,
			  N_("Send a reply message to this address"),
			  NULL /* Handled by EMailReader */, NULL, NULL, NULL },

			/*** Menus ***/

			{ "search-folder-menu",
			  "folder-saved-search",
			  N_("Create Search _Folder"),
			  NULL,
			  NULL,
			  NULL, NULL, NULL, NULL },

			{ "EMailDisplay::open-with-app",
			  NULL,
			  N_("Open with…"),
			  NULL,
			  NULL,
			  action_open_with_app_cb, "i", NULL, NULL }
		};

		g_signal_connect (ui_manager, "create-item",
			G_CALLBACK (e_mail_display_ui_manager_create_item_cb), display);

		e_ui_manager_add_actions (ui_manager, "e-mail-display-attachment-accel", NULL,
			accel_entries, G_N_ELEMENTS (accel_entries), display);
		e_ui_manager_add_actions (ui_manager, "image", NULL,
			image_entries, G_N_ELEMENTS (image_entries), display);
		e_ui_manager_add_actions_with_eui_data (ui_manager, "mailto", NULL,
			mailto_entries, G_N_ELEMENTS (mailto_entries), display, eui);

		display->priv->attachment_accel_action_group = g_object_ref (e_ui_manager_get_action_group (ui_manager, "e-mail-display-attachment-accel"));

		gtk_widget_insert_action_group (GTK_WIDGET (display), e_ui_action_group_get_name (display->priv->attachment_accel_action_group),
			G_ACTION_GROUP (display->priv->attachment_accel_action_group));

		e_ui_manager_set_actions_usable_for_kinds (ui_manager, E_UI_ELEMENT_KIND_MENU,
			"EMailDisplay::open-with-app",
			"search-folder-menu",
			NULL);
	}

	manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (object));

	g_signal_connect_object (manager, "script-message-received::mailDisplayHeadersCollapsed",
		G_CALLBACK (mail_display_headers_collapsed_cb), display, 0);

	g_signal_connect_object (manager, "script-message-received::mailDisplayMagicSpacebarStateChanged",
		G_CALLBACK (mail_display_magic_spacebar_state_changed_cb), display, 0);

	g_signal_connect_object (manager, "script-message-received::scheduleIFramesHeightUpdate",
		G_CALLBACK (mail_display_schedule_iframes_height_update_cb), display, 0);

	webkit_user_content_manager_register_script_message_handler (manager, "mailDisplayHeadersCollapsed");
	webkit_user_content_manager_register_script_message_handler (manager, "mailDisplayMagicSpacebarStateChanged");
	webkit_user_content_manager_register_script_message_handler (manager, "scheduleIFramesHeightUpdate");

	e_extensible_load_extensions (E_EXTENSIBLE (object));
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
	GTK_WIDGET_CLASS (e_mail_display_parent_class)->style_updated (widget);
}

static gboolean
mail_display_image_exists_in_cache (const gchar *uri,
				    gchar **out_cache_filename)
{
	gchar *filename;
	gchar *hash;
	gboolean exists = FALSE;

	if (out_cache_filename)
		*out_cache_filename = NULL;

	if (!emd_global_http_cache | !uri)
		return FALSE;

	if (g_str_has_prefix (uri, "evo-"))
		uri += 4;

	hash = e_http_request_util_compute_uri_checksum (uri);
	filename = camel_data_cache_get_filename (emd_global_http_cache, "http", hash);

	if (filename != NULL) {
		struct stat st;

		exists = g_file_test (filename, G_FILE_TEST_EXISTS);
		if (exists && g_stat (filename, &st) == 0) {
			exists = st.st_size != 0;
			if (exists && out_cache_filename) {
				*out_cache_filename = filename;
				filename = NULL;
			}
		} else {
			exists = FALSE;
		}
		g_free (filename);
	}

	g_free (hash);

	return exists;
}

static void
mai_display_fill_open_with (EWebView *web_view,
			    const gchar *attachment_id)
{
	EMailDisplay *mail_display = E_MAIL_DISPLAY (web_view);
	EAttachment *attachment;
	GList *apps, *link;
	gint op_id = 0;

	attachment = g_hash_table_lookup (mail_display->priv->cid_attachments, attachment_id);
	if (attachment) {
		g_object_ref (attachment);
	} else {
		gchar *cache_filename = NULL;

		if (g_ascii_strncasecmp (attachment_id, "cid:", 4) == 0) {
			CamelMimePart *mime_part;

			mime_part = e_cid_resolver_ref_part (E_CID_RESOLVER (mail_display), attachment_id);
			if (!mime_part)
				return;

			attachment = e_attachment_new ();
			e_attachment_set_mime_part (attachment, mime_part);

			g_object_unref (mime_part);
		} else if (mail_display_image_exists_in_cache (attachment_id, &cache_filename)) {
			attachment = e_attachment_new_for_path (cache_filename);
			g_free (cache_filename);
		} else {
			return;
		}

		/* To have set the file_info, which is used to get the list of apps */
		e_attachment_load (attachment, NULL);

		g_hash_table_insert (mail_display->priv->cid_attachments, g_strdup (attachment_id), g_object_ref (attachment));
	}

	apps = e_attachment_list_apps (attachment);

	g_menu_remove_all (mail_display->priv->open_with_apps_menu);
	g_hash_table_remove_all (mail_display->priv->open_with_apps_hash);

	if (!apps && e_util_is_running_flatpak ())
		apps = g_list_prepend (apps, NULL);

	for (link = apps; link; link = g_list_next (link)) {
		GAppInfo *app_info = link->data;
		GMenuItem *menu_item;
		GIcon *app_icon;
		const gchar *app_id;
		const gchar *app_name;
		gchar *label;

		if (app_info) {
			app_id = g_app_info_get_id (app_info);
			app_icon = g_app_info_get_icon (app_info);
			app_name = g_app_info_get_name (app_info);
		} else {
			app_id = "org.gnome.evolution.flatpak.default-app";
			app_icon = NULL;
			app_name = NULL;
		}

		if (app_id == NULL)
			continue;

		/* Don't list 'Open With "Evolution"'. */
		if (g_str_equal (app_id, "org.gnome.Evolution.desktop"))
			continue;

		if (app_info)
			label = g_strdup_printf (_("Open With “%s”"), app_name);
		else
			label = g_strdup (_("Open With Default Application"));

		menu_item = g_menu_item_new (label, NULL);
		g_menu_item_set_action_and_target_value (menu_item, "e-mail-display-attachment-inline.EMailDisplay::open-with-app", g_variant_new_int32 (op_id));
		g_menu_item_set_icon (menu_item, app_icon);
		g_menu_append_item (mail_display->priv->open_with_apps_menu, menu_item);
		g_clear_object (&menu_item);

		g_hash_table_insert (mail_display->priv->open_with_apps_hash, GINT_TO_POINTER (op_id),  open_with_data_new (app_info, attachment));

		op_id++;

		g_free (label);

		if (!app_info) {
			apps = g_list_remove (apps, app_info);
			break;
		}
	}

	if (link != apps && !e_util_is_running_flatpak ()) {
		GMenuItem *menu_item;

		menu_item = g_menu_item_new (_("Open With Other Application…"), NULL);
		g_menu_item_set_action_and_target_value (menu_item, "e-mail-display-attachment-inline.EMailDisplay::open-with-app", g_variant_new_int32 (op_id));
		g_menu_append_item (mail_display->priv->open_with_apps_menu, menu_item);
		g_clear_object (&menu_item);

		g_hash_table_insert (mail_display->priv->open_with_apps_hash, GINT_TO_POINTER (op_id), open_with_data_new (NULL, attachment));

		op_id++;
	}

	g_list_free_full (apps, g_object_unref);
	g_object_unref (attachment);
}

static void
mail_display_before_popup_event (EWebView *web_view,
				 const gchar *uri)
{
	EMailDisplay *self = E_MAIL_DISPLAY (web_view);
	const gchar *cursor_image_source;
	gchar *popup_iframe_src = NULL, *popup_iframe_id = NULL;
	GList *list, *link;

	e_web_view_get_last_popup_place (web_view, &popup_iframe_src, &popup_iframe_id, NULL, NULL);

	g_menu_remove_all (self->priv->open_with_apps_menu);
	g_hash_table_remove_all (self->priv->open_with_apps_hash);

	list = e_extensible_list_extensions (E_EXTENSIBLE (web_view), E_TYPE_EXTENSION);

	for (link = list; link; link = g_list_next (link)) {
		EExtension *extension = link->data;

		if (!E_IS_MAIL_DISPLAY_POPUP_EXTENSION (extension))
			continue;

		e_mail_display_popup_extension_update_actions (E_MAIL_DISPLAY_POPUP_EXTENSION (extension), popup_iframe_src, popup_iframe_id);
	}

	cursor_image_source = e_web_view_get_cursor_image_src (web_view);
	if (cursor_image_source) {
		EUIAction *action;
		GUri *img_uri;
		gboolean img_is_available;
		gboolean can_show;

		mai_display_fill_open_with (web_view, cursor_image_source);

		img_is_available = mail_display_image_exists_in_cache (cursor_image_source, NULL) ||
			e_mail_display_can_download_uri (E_MAIL_DISPLAY (web_view), cursor_image_source);

		img_uri = g_uri_parse (cursor_image_source, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		can_show = !img_is_available && img_uri && g_uri_get_host (img_uri) && g_uri_get_scheme (img_uri) && (
			g_ascii_strcasecmp (g_uri_get_scheme (img_uri), "http") == 0 ||
			g_ascii_strcasecmp (g_uri_get_scheme (img_uri), "https") == 0 ||
			g_ascii_strcasecmp (g_uri_get_scheme (img_uri), "evo-http") == 0 ||
			g_ascii_strcasecmp (g_uri_get_scheme (img_uri), "evo-https") == 0);

		action = e_web_view_get_action (web_view, "allow-remote-content-site");
		e_ui_action_set_sensitive (action, can_show);
		e_ui_action_set_visible (action, can_show);

		if (can_show) {
			gchar *label;

			label = g_strdup_printf (_("Allow remote content from %s"), g_uri_get_host (img_uri));
			e_ui_action_set_label (action, label);
			g_free (label);
		}

		action = e_web_view_get_action (web_view, "load-remote-content-site");
		e_ui_action_set_sensitive (action, can_show);
		e_ui_action_set_visible (action, can_show);

		if (can_show) {
			gchar *label;

			label = g_strdup_printf (_("Load remote content from %s"), g_uri_get_host (img_uri));
			e_ui_action_set_label (action, label);
			g_free (label);
		}

		action = e_web_view_get_action (web_view, "load-remote-content-this");
		e_ui_action_set_sensitive (action, can_show);
		e_ui_action_set_visible (action, can_show);

		g_clear_pointer (&img_uri, g_uri_unref);
	}

	g_free (popup_iframe_src);
	g_free (popup_iframe_id);
	g_list_free (list);

	/* Chain up to parent's method. */
	E_WEB_VIEW_CLASS (e_mail_display_parent_class)->before_popup_event (web_view, uri);
}

static void
mail_display_uri_requested_cb (EWebView *web_view,
			       const gchar *uri,
			       gchar **redirect_to_uri)
{
	EMailDisplay *display;
	EMailPartList *part_list;
	gboolean uri_is_http;

	display = E_MAIL_DISPLAY (web_view);
	part_list = e_mail_display_get_part_list (display);

	if (part_list == NULL)
		return;

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
		gchar *new_uri, *mail_uri, *query_str;
		GUri *guri;
		GHashTable *query;
		gboolean can_download_uri;
		EImageLoadingPolicy image_policy;

		can_download_uri = e_mail_display_can_download_uri (display, uri);
		if (!can_download_uri) {
			/* Check Evolution's cache */
			can_download_uri = mail_display_image_exists_in_cache (uri, NULL);
		}

		/* If the URI is not cached and we are not allowed to load it
		 * then redirect to invalid URI, so that webkit would display
		 * a native placeholder for it. */
		image_policy = e_mail_formatter_get_image_loading_policy (
			display->priv->formatter);
		if (!can_download_uri && !display->priv->force_image_load &&
		    (image_policy == E_IMAGE_LOADING_POLICY_NEVER)) {
			e_mail_display_claim_skipped_uri (display, uri);
			g_free (*redirect_to_uri);
			*redirect_to_uri = g_strdup ("");
			return;
		}

		folder = e_mail_part_list_get_folder (part_list);
		message_uid = e_mail_part_list_get_message_uid (part_list);

		if (g_str_has_prefix (uri, "evo-")) {
			guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		} else {
			new_uri = g_strconcat ("evo-", uri, NULL);
			guri = g_uri_parse (new_uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

			g_free (new_uri);
		}

		mail_uri = e_mail_part_build_uri (
			folder, message_uid, NULL, NULL);

		query = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			g_free, g_free);

		if (g_uri_get_query (guri)) {
			GHashTable *uri_query;
			GHashTableIter iter;
			gpointer key, value;

			/* It's required to copy the hash table, because it's uncertain
			   which of the key/value pair is freed and which not, while the code
			   below expects to have freed both. */
			uri_query = soup_form_decode (g_uri_get_query (guri));

			g_hash_table_iter_init (&iter, uri_query);
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				g_hash_table_insert (query, g_strdup (key), g_strdup (value));
			}

			g_hash_table_unref (uri_query);
		}

		g_hash_table_insert (query, g_strdup ("__evo-mail"), g_uri_escape_string (mail_uri, NULL, FALSE));

		/* Required, because soup_form_encode_hash() can change
		   order of arguments, then the URL checksum doesn't match. */
		g_hash_table_insert (query, g_strdup ("__evo-original-uri"), g_strdup (uri));

		if (display->priv->force_image_load || can_download_uri) {
			g_hash_table_insert (
				query,
				g_strdup ("__evo-load-images"),
				g_strdup ("true"));
		} else if (image_policy != E_IMAGE_LOADING_POLICY_ALWAYS) {
			e_mail_display_claim_skipped_uri (display, uri);
		}

		query_str = soup_form_encode_hash (query);
		e_util_change_uri_component (&guri, SOUP_URI_QUERY, query_str);
		g_free (query_str);

		new_uri = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);

		g_uri_unref (guri);
		g_hash_table_unref (query);
		g_free (mail_uri);

		g_free (*redirect_to_uri);
		*redirect_to_uri = new_uri;
	}
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
	GUri *guri;

	/* Note, this assumes the URI comes
	 * from the currently loaded message. */
	display = E_MAIL_DISPLAY (web_view);

	mime_part = camel_mime_part_from_cid (display, uri);

	if (mime_part)
		return g_strdup (camel_mime_part_get_filename (mime_part));

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	if (guri) {
		gchar *filename = NULL;

		if (g_uri_get_query (guri)) {
			GHashTable *uri_query;

			uri_query = soup_form_decode (g_uri_get_query (guri));
			if (uri_query && g_hash_table_contains (uri_query, "filename"))
				filename = g_strdup (g_hash_table_lookup (uri_query, "filename"));

			if (uri_query)
				g_hash_table_destroy (uri_query);
		}

		g_uri_unref (guri);

		if (filename && *filename)
			return filename;

		g_free (filename);
	}

	/* Chain up to parent's suggest_filename() method. */
	return E_WEB_VIEW_CLASS (e_mail_display_parent_class)->
		suggest_filename (web_view, uri);
}

static void
mail_display_save_part_for_drop (CamelMimePart *mime_part,
				 GtkSelectionData *data)
{
	gchar *tmp, *path, *filename;
	const gchar *part_filename;
	CamelDataWrapper *dw;

	g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));
	g_return_if_fail (data != NULL);

	tmp = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (tmp);
	g_free (tmp);

	g_return_if_fail (path != NULL);

	part_filename = camel_mime_part_get_filename (mime_part);
	if (!part_filename || !*part_filename) {
		CamelDataWrapper *content;

		content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

		if (CAMEL_IS_MIME_MESSAGE (content))
			part_filename = camel_mime_message_get_subject (CAMEL_MIME_MESSAGE (content));
	}

	if (!part_filename || !*part_filename)
		part_filename = "mail-part";

	tmp = g_strdup (part_filename);
	e_util_make_safe_filename (tmp);

	filename = g_build_filename (path, tmp, NULL);
	g_free (tmp);

	dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	g_warn_if_fail (dw);

	if (dw) {
		CamelStream *stream;

		stream = camel_stream_fs_new_with_name (filename, O_CREAT | O_TRUNC | O_WRONLY, 0666, NULL);
		if (stream) {
			if (camel_data_wrapper_decode_to_stream_sync (dw, stream, NULL, NULL)) {
				tmp = g_filename_to_uri (filename, NULL, NULL);
				if (tmp) {
					gtk_selection_data_set (
						data,
						gtk_selection_data_get_data_type (data),
						gtk_selection_data_get_format (data),
						(const guchar *) tmp, strlen (tmp));
					g_free (tmp);
				}
			}

			camel_stream_close (stream, NULL, NULL);
			g_object_unref (stream);
		}
	}

	g_free (filename);
	g_free (path);
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

	if (!mime_part && g_str_has_prefix (uri, "mail:")) {
		GUri *guri;
		const gchar *query_str;

		guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		if (guri) {
			query_str = g_uri_get_query (guri);
			if (query_str) {
				GHashTable *query;
				const gchar *part_id_raw;

				query = soup_form_decode (query_str);
				part_id_raw = query ? g_hash_table_lookup (query, "part_id") : NULL;
				if (part_id_raw && *part_id_raw) {
					EMailPartList *part_list;
					EMailPart *mail_part;

					part_list = e_mail_display_get_part_list (display);
					if (part_list) {
						gchar *part_id = g_uri_unescape_string (part_id_raw, NULL);

						mail_part = part_id ? e_mail_part_list_ref_part (part_list, part_id) : NULL;
						g_free (part_id);

						if (mail_part) {
							CamelMimePart *part;

							part = e_mail_part_ref_mime_part (mail_part);
							if (part) {
								mail_display_save_part_for_drop (part, data);
							}

							g_clear_object (&part);
							g_object_unref (mail_part);
						}
					}
				}

				if (query)
					g_hash_table_unref (query);
			}

			g_uri_unref (guri);
		}
	}

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
		g_free (mime_type);
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

static gboolean
e_mail_display_test_key_changed (EMailDisplay *mail_display,
				 const gchar *key,
				 GSettings *settings)
{
	GVariant *new_value, *old_value;
	gboolean changed = FALSE;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (mail_display->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (mail_display->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (mail_display->priv->old_settings, key);

		changed = TRUE;
	} else if (new_value) {
		g_variant_unref (new_value);
	}

	return changed;
}

static void
e_mail_display_test_change_and_update_fonts_cb (EMailDisplay *mail_display,
						const gchar *key,
						GSettings *settings)
{
	if (e_mail_display_test_key_changed (mail_display, key, settings))
		e_web_view_update_fonts (E_WEB_VIEW (mail_display));
}

static void
e_mail_display_test_change_and_reload_cb (EMailDisplay *mail_display,
					  const gchar *key,
					  GSettings *settings)
{
	if (e_mail_display_test_key_changed (mail_display, key, settings))
		e_mail_display_reload (mail_display);
}

static void
mail_display_web_process_terminated_cb (EMailDisplay *display,
					WebKitWebProcessTerminationReason reason)
{
	EAlertSink *alert_sink;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	/* Cannot use the EWebView, because it places the alerts inside itself */
	alert_sink = e_shell_utils_find_alternate_alert_sink (GTK_WIDGET (display));
	if (alert_sink)
		e_alert_submit (alert_sink, "mail:webkit-web-process-crashed", NULL);
}

static EMailPart *
e_mail_display_ref_mail_part (EMailDisplay *mail_display,
			      const gchar *uri)
{
	EMailPartList *part_list;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (mail_display), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	part_list = e_mail_display_get_part_list (mail_display);
	if (!part_list)
		return NULL;

	return e_mail_part_list_ref_part (part_list, uri);
}

static CamelMimePart *
e_mail_display_cid_resolver_ref_part (ECidResolver *resolver,
				      const gchar *uri)
{
	EMailPart *mail_part;
	CamelMimePart *mime_part;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (resolver), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	mail_part = e_mail_display_ref_mail_part (E_MAIL_DISPLAY (resolver), uri);
	if (!mail_part)
		return NULL;

	mime_part = e_mail_part_ref_mime_part (mail_part);

	g_object_unref (mail_part);

	return mime_part;
}

static gchar *
e_mail_display_cid_resolver_dup_mime_type (ECidResolver *resolver,
					   const gchar *uri)
{
	EMailPart *mail_part;
	gchar *mime_type;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (resolver), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	mail_part = e_mail_display_ref_mail_part (E_MAIL_DISPLAY (resolver), uri);
	if (!mail_part)
		return NULL;

	mime_type = g_strdup (e_mail_part_get_mime_type (mail_part));

	g_object_unref (mail_part);

	return mime_type;
}

static void
e_mail_display_cid_resolver_init (ECidResolverInterface *iface)
{
	iface->ref_part = e_mail_display_cid_resolver_ref_part;
	iface->dup_mime_type = e_mail_display_cid_resolver_dup_mime_type;
}

static void
e_mail_display_class_init (EMailDisplayClass *class)
{
	GObjectClass *object_class;
	EWebViewClass *web_view_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_display_constructed;
	object_class->set_property = mail_display_set_property;
	object_class->get_property = mail_display_get_property;
	object_class->dispose = mail_display_dispose;
	object_class->finalize = mail_display_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = mail_display_realize;
	widget_class->style_updated = mail_display_style_updated;

	web_view_class = E_WEB_VIEW_CLASS (class);
	web_view_class->suggest_filename = mail_display_suggest_filename;
	web_view_class->set_fonts = mail_display_set_fonts;
	web_view_class->before_popup_event = mail_display_before_popup_event;

	g_object_class_install_property (
		object_class,
		PROP_ATTACHMENT_STORE,
		g_param_spec_object (
			"attachment-store",
			"Attachment Store",
			NULL,
			E_TYPE_ATTACHMENT_STORE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ATTACHMENT_VIEW,
		g_param_spec_object (
			"attachment-view",
			"Attachment View",
			NULL,
			E_TYPE_ATTACHMENT_VIEW,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_FORMATTER,
		g_param_spec_object (
			"formatter",
			"Mail Formatter",
			NULL,
			E_TYPE_MAIL_FORMATTER,
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

	g_object_class_install_property (
		object_class,
		PROP_MAIL_READER,
		g_param_spec_object (
			"mail-reader",
			"a mail reader this instance is part of",
			NULL,
			E_TYPE_MAIL_READER,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[REMOTE_CONTENT_CLICKED] = g_signal_new (
		"remote-content-clicked",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_RECTANGLE);

	signals[AUTOCRYPT_IMPORT_CLICKED] = g_signal_new (
		"autocrypt-import-clicked",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_RECTANGLE);
}

static void
e_mail_display_init (EMailDisplay *display)
{
	GSettings *settings;

	display->priv = e_mail_display_get_instance_private (display);

	g_weak_ref_init (&display->priv->mail_reader_weakref, NULL);

	display->priv->attachment_store = E_ATTACHMENT_STORE (e_attachment_store_new ());
	display->priv->attachment_flags = g_hash_table_new (g_direct_hash, g_direct_equal);
	display->priv->cid_attachments = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	display->priv->open_with_apps_menu = g_menu_new ();
	display->priv->open_with_apps_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, open_with_data_free);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	display->priv->skip_insecure_parts = !g_settings_get_boolean (settings, "show-insecure-parts");
	g_object_unref (settings);

	g_signal_connect (display->priv->attachment_store, "attachment-added",
		G_CALLBACK (mail_display_attachment_added_cb), display);
	g_signal_connect (display->priv->attachment_store, "attachment-removed",
		G_CALLBACK (mail_display_attachment_removed_cb), display);

	display->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	/* Set invalid mode so that MODE property initialization is run
	 * completely (see e_mail_display_set_mode) */
	display->priv->mode = E_MAIL_FORMATTER_MODE_INVALID;
	e_mail_display_set_mode (display, E_MAIL_FORMATTER_MODE_NORMAL);
	display->priv->force_image_load = FALSE;
	display->priv->scheduled_reload = 0;

	g_signal_connect (
		display, "web-process-terminated",
		G_CALLBACK (mail_display_web_process_terminated_cb), NULL);

	g_signal_connect (
		display, "decide-policy",
		G_CALLBACK (decide_policy_cb), NULL);

	g_signal_connect (
		display, "process-mailto",
		G_CALLBACK (mail_display_process_mailto), NULL);

	g_signal_connect (
		display, "resource-loaded",
		G_CALLBACK (e_mail_display_schedule_iframes_height_update), NULL);

	g_signal_connect_after (
		display, "drag-data-get",
		G_CALLBACK (mail_display_drag_data_get), display);

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
	g_signal_connect_swapped (
		display->priv->settings , "changed::preview-unset-html-colors",
		G_CALLBACK (e_mail_display_test_change_and_reload_cb), display);

	g_signal_connect (
		display, "load-changed",
		G_CALLBACK (mail_display_load_changed_cb), NULL);

	g_signal_connect (
		display, "content-loaded",
		G_CALLBACK (mail_display_content_loaded_cb), NULL);

	g_mutex_init (&display->priv->remote_content_lock);
	display->priv->remote_content = NULL;
	display->priv->skipped_remote_content_sites = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);
	display->priv->temporary_allow_remote_content = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);

	g_signal_connect (display, "uri-requested", G_CALLBACK (mail_display_uri_requested_cb), NULL);

	if (emd_global_http_cache == NULL) {
		const gchar *user_cache_dir;
		GError *error = NULL;

		user_cache_dir = e_get_user_cache_dir ();
		emd_global_http_cache = camel_data_cache_new (user_cache_dir, &error);

		if (emd_global_http_cache) {
			/* cache expiry - 2 hour access, 1 day max */
			camel_data_cache_set_expire_age (
				emd_global_http_cache, 24 * 60 * 60);
			camel_data_cache_set_expire_access (
				emd_global_http_cache, 2 * 60 * 60);
		} else {
			e_alert_submit (
				E_ALERT_SINK (display), "mail:folder-open",
				error ? error->message : _("Unknown error"), NULL);
			g_clear_error (&error);
		}
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
		"*",
		param_spec->name,
		color_value);

	gdk_rgba_free (color);
	g_free (color_value);
}

static void
e_mail_display_claim_attachment (EMailFormatter *formatter,
				 EAttachment *attachment,
				 gpointer user_data)
{
	EMailDisplay *display = user_data;
	GList *attachments;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if (e_attachment_get_is_possible (attachment)) {
		e_attachment_bar_add_possible_attachment (E_ATTACHMENT_BAR (display->priv->attachment_view), attachment);
		return;
	}

	attachments = e_attachment_store_get_attachments (display->priv->attachment_store);

	if (!g_list_find (attachments, attachment)) {
		e_attachment_store_add_attachment (display->priv->attachment_store, attachment);

		if (e_attachment_is_mail_note (attachment)) {
			CamelFolder *folder;
			const gchar *message_uid;

			folder = e_mail_part_list_get_folder (display->priv->part_list);
			message_uid = e_mail_part_list_get_message_uid (display->priv->part_list);

			if (folder && message_uid) {
				CamelMessageInfo *info;

				info = camel_folder_get_message_info (folder, message_uid);
				if (info) {
					if (!camel_message_info_get_user_flag (info, E_MAIL_NOTES_USER_FLAG))
						camel_message_info_set_user_flag (info, E_MAIL_NOTES_USER_FLAG, TRUE);
					g_clear_object (&info);
				}
			}
		}
	}

	g_list_free_full (attachments, g_object_unref);
}

GtkWidget *
e_mail_display_new (EMailRemoteContent *remote_content,
		    EMailReader *mail_reader)
{
	return g_object_new (E_TYPE_MAIL_DISPLAY,
		"remote-content", remote_content,
		"mail-reader", mail_reader,
		NULL);
}

EMailReader *
e_mail_display_ref_mail_reader (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	return g_weak_ref_get (&display->priv->mail_reader_weakref);
}

EAttachmentStore *
e_mail_display_get_attachment_store (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	return display->priv->attachment_store;
}

EAttachmentView *
e_mail_display_get_attachment_view (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	return display->priv->attachment_view;
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

	g_signal_connect (formatter, "claim-attachment", G_CALLBACK (e_mail_display_claim_attachment), display);

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
	GSettings *settings;
	GSList *insecure_part_ids = NULL;
	gboolean has_secured_parts = FALSE;

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

	if (part_list) {
		GQueue queue = G_QUEUE_INIT;
		GHashTable *secured_message_ids;
		GList *link;

		e_mail_part_list_queue_parts (part_list, NULL, &queue);

		secured_message_ids = e_mail_formatter_utils_extract_secured_message_ids (g_queue_peek_head_link (&queue));
		has_secured_parts = secured_message_ids != NULL;

		if (has_secured_parts) {
			gboolean has_encypted_part = FALSE;

			for (link = g_queue_peek_head_link (&queue); link; link = g_list_next (link)) {
				EMailPart *part = link->data;

				if (!e_mail_formatter_utils_consider_as_secured_part (part, secured_message_ids))
					continue;

				if (!e_mail_part_has_validity (part)) {
					insecure_part_ids = g_slist_prepend (insecure_part_ids, g_strdup (e_mail_part_get_id (part)));
				} else if (e_mail_part_get_validity (part, E_MAIL_PART_VALIDITY_ENCRYPTED)) {
					if (has_encypted_part) {
						/* consider the second and following encrypted parts as evil */
						insecure_part_ids = g_slist_prepend (insecure_part_ids, g_strdup (e_mail_part_get_id (part)));
					} else {
						has_encypted_part = TRUE;
					}
				}
			}
		}

		while (!g_queue_is_empty (&queue))
			g_object_unref (g_queue_pop_head (&queue));

		g_clear_pointer (&secured_message_ids, g_hash_table_destroy);
	}

	g_slist_free_full (display->priv->insecure_part_ids, g_free);
	display->priv->insecure_part_ids = insecure_part_ids;
	display->priv->has_secured_parts = has_secured_parts;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	display->priv->skip_insecure_parts = !g_settings_get_boolean (settings, "show-insecure-parts");
	g_object_unref (settings);

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

	g_mutex_lock (&display->priv->remote_content_lock);
	g_hash_table_remove_all (display->priv->temporary_allow_remote_content);
	g_mutex_unlock (&display->priv->remote_content_lock);

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
	GUri *guri;
	gchar *mode, *collapsable, *collapsed;
	const gchar *default_charset, *charset;

	web_view = E_WEB_VIEW (display);
	uri = (gchar *) webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view));

	display->priv->scheduled_reload = 0;

	if (!uri || !*uri || g_ascii_strcasecmp (uri, "about:blank") == 0)
		return FALSE;

	if (strstr (uri, "?") == NULL) {
		e_web_view_reload (web_view);
		return FALSE;
	}

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

	mode = g_strdup_printf ("%d", display->priv->mode);
	collapsable = g_strdup_printf ("%d", display->priv->headers_collapsable);
	collapsed = g_strdup_printf ("%d", display->priv->headers_collapsed);
	default_charset = e_mail_formatter_get_default_charset (display->priv->formatter);
	charset = e_mail_formatter_get_charset (display->priv->formatter);

	if (!default_charset)
		default_charset = "";
	if (!charset)
		charset = "";

	table = soup_form_decode (g_uri_get_query (guri));
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

	e_util_change_uri_component (&guri, SOUP_URI_QUERY, query);
	g_free (query);

	uri = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);
	e_web_view_load_uri (web_view, uri);
	g_free (uri);
	g_uri_unref (guri);

	return FALSE;
}

void
e_mail_display_reload (EMailDisplay *display)
{
	const gchar *uri;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (display));

	if (!uri || !*uri || g_ascii_strcasecmp (uri, "about:blank") == 0 ||
	    display->priv->scheduled_reload > 0)
		return;

	/* Schedule reloading if neccessary.
	 * Prioritize ahead of GTK+ redraws. */
	display->priv->scheduled_reload = g_idle_add_full (
		G_PRIORITY_HIGH_IDLE,
		(GSourceFunc) do_reload_display, display, NULL);
}

EUIAction *
e_mail_display_get_action (EMailDisplay *display,
                           const gchar *action_name)
{
	EUIAction *action;

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
		"<meta name=\"color-scheme\" content=\"light dark\">\n"
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
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	if ((display->priv->force_image_load ? 1 : 0) == (force_load_images ? 1 : 0))
		return;

	display->priv->force_image_load = force_load_images;
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

gboolean
e_mail_display_process_magic_spacebar (EMailDisplay *display,
				       gboolean towards_bottom)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), FALSE);

	if ((towards_bottom && !(display->priv->magic_spacebar_state & E_MAGIC_SPACEBAR_CAN_GO_BOTTOM)) ||
	    (!towards_bottom && !(display->priv->magic_spacebar_state & E_MAGIC_SPACEBAR_CAN_GO_TOP)))
		return FALSE;

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (display), e_web_view_get_cancellable (E_WEB_VIEW (display)),
		"Evo.MailDisplayProcessMagicSpacebar(%x);",
		towards_bottom);

	return TRUE;
}

gboolean
e_mail_display_get_skip_insecure_parts (EMailDisplay *mail_display)
{
	return !mail_display ||
	       !gtk_widget_is_visible (GTK_WIDGET (mail_display)) ||
	       !mail_display->priv->insecure_part_ids ||
	       mail_display->priv->skip_insecure_parts;
}
