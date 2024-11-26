/*
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
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "eab-contact-display.h"

#include <string.h>
#include <glib/gi18n.h>

#include <webkit2/webkit2.h>

#include "shell/e-shell-utils.h"

#include "e-contact-map.h"
#include "eab-contact-formatter.h"
#include "eab-gui-util.h"

#define TEXT_IS_RIGHT_TO_LEFT \
	(gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)

struct _EABContactDisplayPrivate {
	EContact *contact;

	EABContactDisplayMode mode;
	gboolean show_maps;
	gboolean home_before_work;
};

enum {
	PROP_0,
	PROP_CONTACT,
	PROP_MODE,
	PROP_SHOW_MAPS
};

enum {
	SEND_MESSAGE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EABContactDisplay, eab_contact_display, E_TYPE_WEB_VIEW)

static void
contact_display_emit_send_message (EABContactDisplay *display,
                                   gint email_num)
{
	EDestination *destination;
	EContact *contact;

	g_return_if_fail (email_num >= 0);

	destination = e_destination_new ();
	contact = eab_contact_display_get_contact (display);
	e_destination_set_contact (destination, contact, email_num);
	g_signal_emit (display, signals[SEND_MESSAGE], 0, destination);
	g_object_unref (destination);
}

static void
action_contact_mailto_copy_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EABContactDisplay *display = user_data;
	GtkClipboard *clipboard;
	EWebView *web_view;
	EContact *contact;
	GList *list;
	const gchar *text;
	const gchar *uri;
	gint index;

	web_view = E_WEB_VIEW (display);
	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	index = atoi (uri + strlen ("internal-mailto:"));
	g_return_if_fail (index >= 0);

	contact = eab_contact_display_get_contact (display);
	list = e_contact_get (contact, E_CONTACT_EMAIL);
	text = g_list_nth_data (list, index);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, text, -1);
	gtk_clipboard_store (clipboard);

	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);
}

static void
action_contact_send_message_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EABContactDisplay *display = user_data;
	EWebView *web_view;
	const gchar *uri;
	gint index;

	web_view = E_WEB_VIEW (display);
	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	index = atoi (uri + strlen ("internal-mailto:"));
	contact_display_emit_send_message (display, index);
}

static void
load_contact (EABContactDisplay *display)
{
	EABContactFormatter *formatter;
	GString *buffer;

	if (!display->priv->contact) {
		e_web_view_clear (E_WEB_VIEW (display));
		return;
	}

	formatter = eab_contact_formatter_new ();
	g_object_set (
		G_OBJECT (formatter),
		"display-mode", display->priv->mode,
		"render-maps", display->priv->show_maps,
		NULL);

	buffer = g_string_sized_new (1024);

	eab_contact_formatter_format_contact (
		formatter, display->priv->contact, buffer);
	e_web_view_load_string (E_WEB_VIEW (display), buffer->str);

	g_string_free (buffer, TRUE);

	g_object_unref (formatter);
}

static void
contact_display_web_process_terminated_cb (EABContactDisplay *display,
					   WebKitWebProcessTerminationReason reason)
{
	EAlertSink *alert_sink;

	g_return_if_fail (EAB_IS_CONTACT_DISPLAY (display));

	/* Cannot use the EWebView, because it places the alerts inside itself */
	alert_sink = e_shell_utils_find_alternate_alert_sink (GTK_WIDGET (display));
	if (alert_sink)
		e_alert_submit (alert_sink, "addressbook:webkit-web-process-crashed", NULL);
}

static void
contact_display_content_loaded_cb (EWebView *web_view,
				   const gchar *iframe_id,
				   gpointer user_data)
{
	g_return_if_fail (EAB_IS_CONTACT_DISPLAY (web_view));

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
		"Evo.VCardBind(%s);", iframe_id);
}

static void
eab_contact_display_settings_changed_cb (GSettings *settings,
					 const gchar *key,
					 gpointer user_data)
{
	EABContactDisplay *display = user_data;
	gboolean home_before_work;

	g_return_if_fail (EAB_IS_CONTACT_DISPLAY (display));

	home_before_work = g_settings_get_boolean (settings, "preview-home-before-work");

	if (display->priv->contact && (home_before_work ? 1 : 0) != (display->priv->home_before_work ? 1 : 0)) {
		display->priv->home_before_work = home_before_work;
		load_contact (display);
	}
}

static void
contact_display_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONTACT:
			eab_contact_display_set_contact (
				EAB_CONTACT_DISPLAY (object),
				g_value_get_object (value));
			return;

		case PROP_MODE:
			eab_contact_display_set_mode (
				EAB_CONTACT_DISPLAY (object),
				g_value_get_int (value));
			return;

		case PROP_SHOW_MAPS:
			eab_contact_display_set_show_maps (
				EAB_CONTACT_DISPLAY (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
contact_display_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONTACT:
			g_value_set_object (
				value, eab_contact_display_get_contact (
				EAB_CONTACT_DISPLAY (object)));
			return;

		case PROP_MODE:
			g_value_set_int (
				value, eab_contact_display_get_mode (
				EAB_CONTACT_DISPLAY (object)));
			return;

		case PROP_SHOW_MAPS:
			g_value_set_boolean (
				value, eab_contact_display_get_show_maps (
				EAB_CONTACT_DISPLAY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
contact_display_contructed (GObject *object)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='context'>"
		    "<placeholder id='custom-actions-1'>"
		      "<item action='contact-send-message'/>"
		    "</placeholder>"
		    "<placeholder id='custom-actions-2'>"
		      "<item action='contact-mailto-copy'/>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry internal_mailto_entries[] = {

		{ "contact-mailto-copy",
		  "edit-copy",
		  N_("Copy _Email Address"),
		  NULL,
		  N_("Copy the email address to the clipboard"),
		  action_contact_mailto_copy_cb, NULL, NULL, NULL },

		{ "contact-send-message",
		  "mail-message-new",
		  N_("_Send New Message To…"),
		  NULL,
		  N_("Send a mail message to this address"),
		  action_contact_send_message_cb, NULL, NULL, NULL }
	};

	EABContactDisplay *display = EAB_CONTACT_DISPLAY (object);
	EWebView *web_view;
	EUIManager *ui_manager;
	GSettings *settings;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (eab_contact_display_parent_class)->constructed (object);

	web_view = E_WEB_VIEW (display);
	ui_manager = e_web_view_get_ui_manager (web_view);

	g_signal_connect (
		display, "web-process-terminated",
		G_CALLBACK (contact_display_web_process_terminated_cb), NULL);

	g_signal_connect (
		web_view, "content-loaded",
		G_CALLBACK (contact_display_content_loaded_cb), NULL);
	g_signal_connect (
		web_view, "style-updated",
		G_CALLBACK (load_contact), NULL);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "internal-mailto", NULL,
		internal_mailto_entries, G_N_ELEMENTS (internal_mailto_entries), display, eui);

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");
	g_signal_connect_object (settings, "changed::preview-home-before-work",
		G_CALLBACK (eab_contact_display_settings_changed_cb), display, 0);
	display->priv->home_before_work = g_settings_get_boolean (settings, "preview-home-before-work");
	g_clear_object (&settings);
}

static void
contact_display_dispose (GObject *object)
{
	EABContactDisplay *self = EAB_CONTACT_DISPLAY (object);

	g_clear_object (&self->priv->contact);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (eab_contact_display_parent_class)->dispose (object);
}

static void
contact_display_hovering_over_link (EWebView *web_view,
                                    const gchar *title,
                                    const gchar *uri)
{
	EABContactDisplay *display;
	EContact *contact;
	const gchar *name;
	gchar *message;
	gboolean handled = FALSE;

	if (uri && g_str_has_prefix (uri, "internal-mailto:")) {
		display = EAB_CONTACT_DISPLAY (web_view);
		contact = eab_contact_display_get_contact (display);

		name = e_contact_get_const (contact, E_CONTACT_FILE_AS);
		if (name == NULL)
			e_contact_get_const (contact, E_CONTACT_FULL_NAME);
		if (name) {
			message = g_strdup_printf (_("Click to mail %s"), name);
			e_web_view_status_message (web_view, message);
			g_free (message);
		}

		handled = TRUE;
	} else if (uri && g_str_has_prefix (uri, "open-map:")) {
		GUri *guri;

		guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		if (guri) {
			gchar *decoded;

			decoded = g_uri_unescape_string (g_uri_get_path (guri), NULL);

			if (decoded) {
				message = g_strdup_printf (_("Click to open map for %s"), decoded);
				e_web_view_status_message (web_view, message);
				g_free (message);

				handled = TRUE;
			}

			g_uri_unref (guri);
			g_free (decoded);
		}
	}

	if (!handled) {
		/* Chain up to parent's method. */
		E_WEB_VIEW_CLASS (eab_contact_display_parent_class)->hovering_over_link (web_view, title, uri);
	}
}

static void
contact_display_link_clicked (EWebView *web_view,
                              const gchar *uri)
{
	EABContactDisplay *display;
	gsize length;

	display = EAB_CONTACT_DISPLAY (web_view);

	length = strlen ("internal-mailto:");
	if (g_ascii_strncasecmp (uri, "internal-mailto:", length) == 0) {
		gint index;

		index = atoi (uri + length);
		contact_display_emit_send_message (display, index);
		return;
	}

	/* Chain up to parent's method. */
	E_WEB_VIEW_CLASS (eab_contact_display_parent_class)->link_clicked (web_view, uri);
}

static void
contact_display_update_actions (EWebView *web_view)
{
	EUIActionGroup *action_group;
	gboolean scheme_is_internal_mailto;
	gboolean visible;
	const gchar *group_name;
	const gchar *uri;

	/* Chain up to parent's update_actions() method. */
	E_WEB_VIEW_CLASS (eab_contact_display_parent_class)->update_actions (web_view);

	uri = e_web_view_get_selected_uri (web_view);

	scheme_is_internal_mailto = (uri == NULL) ? FALSE :
		(g_ascii_strncasecmp (uri, "internal-mailto:", 16) == 0);

	/* Override how EWebView treats internal-mailto URIs. */
	group_name = "uri";
	action_group = e_web_view_get_action_group (web_view, group_name);
	visible = e_ui_action_group_get_visible (action_group);
	visible &= !scheme_is_internal_mailto;
	e_ui_action_group_set_visible (action_group, visible);

	group_name = "internal-mailto";
	visible = scheme_is_internal_mailto;
	action_group = e_web_view_get_action_group (web_view, group_name);
	e_ui_action_group_set_visible (action_group, visible);
}

static void
eab_contact_display_class_init (EABContactDisplayClass *class)
{
	GObjectClass *object_class;
	EWebViewClass *web_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = contact_display_set_property;
	object_class->get_property = contact_display_get_property;
	object_class->constructed = contact_display_contructed;
	object_class->dispose = contact_display_dispose;

	web_view_class = E_WEB_VIEW_CLASS (class);
	web_view_class->hovering_over_link = contact_display_hovering_over_link;
	web_view_class->link_clicked = contact_display_link_clicked;
	web_view_class->update_actions = contact_display_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_CONTACT,
		g_param_spec_object (
			"contact",
			NULL,
			NULL,
			E_TYPE_CONTACT,
			G_PARAM_READWRITE));

	/* XXX Make this a real enum property. */
	g_object_class_install_property (
		object_class,
		PROP_MODE,
		g_param_spec_int (
			"mode",
			NULL,
			NULL,
			EAB_CONTACT_DISPLAY_RENDER_NORMAL,
			EAB_CONTACT_DISPLAY_RENDER_COMPACT,
			EAB_CONTACT_DISPLAY_RENDER_NORMAL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_MAPS,
		g_param_spec_boolean (
			"show-maps",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	signals[SEND_MESSAGE] = g_signal_new (
		"send-message",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EABContactDisplayClass, send_message),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_DESTINATION);
}

static void
eab_contact_display_init (EABContactDisplay *display)
{
	display->priv = eab_contact_display_get_instance_private (display);
}

GtkWidget *
eab_contact_display_new (void)
{
	return g_object_new (EAB_TYPE_CONTACT_DISPLAY, NULL);
}

EContact *
eab_contact_display_get_contact (EABContactDisplay *display)
{
	g_return_val_if_fail (EAB_IS_CONTACT_DISPLAY (display), NULL);

	return display->priv->contact;
}

void
eab_contact_display_set_contact (EABContactDisplay *display,
                                 EContact *contact)
{
	g_return_if_fail (EAB_IS_CONTACT_DISPLAY (display));

	if (display->priv->contact == contact)
		return;

	if (contact != NULL)
		g_object_ref (contact);

	if (display->priv->contact != NULL)
		g_object_unref (display->priv->contact);

	display->priv->contact = contact;

	load_contact (display);

	g_object_notify (G_OBJECT (display), "contact");
}

EABContactDisplayMode
eab_contact_display_get_mode (EABContactDisplay *display)
{
	g_return_val_if_fail (EAB_IS_CONTACT_DISPLAY (display), 0);

	return display->priv->mode;
}

void
eab_contact_display_set_mode (EABContactDisplay *display,
                              EABContactDisplayMode mode)
{
	g_return_if_fail (EAB_IS_CONTACT_DISPLAY (display));

	if (display->priv->mode == mode)
		return;

	display->priv->mode = mode;

	load_contact (display);

	g_object_notify (G_OBJECT (display), "mode");
}

gboolean
eab_contact_display_get_show_maps (EABContactDisplay *display)
{
	g_return_val_if_fail (EAB_IS_CONTACT_DISPLAY (display), FALSE);

	return display->priv->show_maps;
}

void
eab_contact_display_set_show_maps (EABContactDisplay *display,
                                   gboolean show_maps)
{
	g_return_if_fail (EAB_IS_CONTACT_DISPLAY (display));

	if (display->priv->show_maps == show_maps)
		return;

	display->priv->show_maps = show_maps;

	load_contact (display);

	g_object_notify (G_OBJECT (display), "show-maps");
}
