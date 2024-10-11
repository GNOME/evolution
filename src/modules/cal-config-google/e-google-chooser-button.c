/*
 * e-google-chooser-button.c
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

#include <libedataserverui/libedataserverui.h>

#include "module-cal-config-google.h"
#include "e-google-chooser-button.h"

struct _EGoogleChooserButtonPrivate {
	ESource *source;
	ESourceConfig *config;
	GtkWidget *label;
};

enum {
	PROP_0,
	PROP_SOURCE,
	PROP_CONFIG
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EGoogleChooserButton, e_google_chooser_button, GTK_TYPE_BUTTON, 0,
	G_ADD_PRIVATE_DYNAMIC (EGoogleChooserButton))

static void
google_chooser_button_set_source (EGoogleChooserButton *button,
                                  ESource *source)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (button->priv->source == NULL);

	button->priv->source = g_object_ref (source);
}

static void
google_chooser_button_set_config (EGoogleChooserButton *button,
                                  ESourceConfig *config)
{
	g_return_if_fail (E_IS_SOURCE_CONFIG (config));
	g_return_if_fail (button->priv->config == NULL);

	button->priv->config = g_object_ref (config);
}

static void
google_chooser_button_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE:
			google_chooser_button_set_source (
				E_GOOGLE_CHOOSER_BUTTON (object),
				g_value_get_object (value));
			return;

		case PROP_CONFIG:
			google_chooser_button_set_config (
				E_GOOGLE_CHOOSER_BUTTON (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_chooser_button_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE:
			g_value_set_object (
				value,
				e_google_chooser_button_get_source (
				E_GOOGLE_CHOOSER_BUTTON (object)));
			return;

		case PROP_CONFIG:
			g_value_set_object (
				value,
				e_google_chooser_button_get_config (
				E_GOOGLE_CHOOSER_BUTTON (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_chooser_button_dispose (GObject *object)
{
	EGoogleChooserButton *self = E_GOOGLE_CHOOSER_BUTTON (object);

	g_clear_object (&self->priv->source);
	g_clear_object (&self->priv->config);
	g_clear_object (&self->priv->label);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_google_chooser_button_parent_class)->dispose (object);
}

static void
google_chooser_button_constructed (GObject *object)
{
	EGoogleChooserButton *button;
	ESourceWebdav *webdav_extension;
	GBindingFlags binding_flags;
	GtkWidget *widget;
	const gchar *display_name;

	button = E_GOOGLE_CHOOSER_BUTTON (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_google_chooser_button_parent_class)->constructed (object);

	widget = gtk_label_new (_("Default User Calendar"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_container_add (GTK_CONTAINER (button), widget);
	button->priv->label = g_object_ref (widget);
	gtk_widget_show (widget);

	webdav_extension = e_source_get_extension (
		button->priv->source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
	display_name = e_source_webdav_get_display_name (webdav_extension);

	binding_flags = G_BINDING_DEFAULT;
	if (display_name != NULL && *display_name != '\0')
		binding_flags |= G_BINDING_SYNC_CREATE;

	e_binding_bind_property (
		webdav_extension, "display-name",
		button->priv->label, "label",
		binding_flags);
}

static GtkWindow *
google_config_get_dialog_parent_cb (ECredentialsPrompter *prompter,
				    GtkWindow *dialog)
{
	return dialog;
}

static void
google_chooser_button_clicked (GtkButton *button)
{
	EGoogleChooserButton *self = E_GOOGLE_CHOOSER_BUTTON (button);
	gpointer parent;
	ESourceRegistry *registry;
	ECredentialsPrompter *prompter;
	ESourceWebdav *webdav_extension;
	ESourceAuthentication *authentication_extension;
	GUri *guri;
	gchar *base_url;
	GtkDialog *dialog;
	gulong handler_id;
	guint supports_filter = 0;
	gboolean can_google_auth;
	const gchar *title = NULL;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	registry = e_source_config_get_registry (self->priv->config);

	authentication_extension = e_source_get_extension (self->priv->source, E_SOURCE_EXTENSION_AUTHENTICATION);
	webdav_extension = e_source_get_extension (self->priv->source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	guri = e_source_webdav_dup_uri (webdav_extension);
	can_google_auth = e_module_cal_config_google_is_supported (NULL, registry) &&
			  g_strcmp0 (e_source_authentication_get_method (authentication_extension), "OAuth2") != 0;

	e_google_chooser_button_construct_default_uri (&guri, e_source_authentication_get_user (authentication_extension));

	if (can_google_auth) {
		/* Prefer 'Google', aka internal OAuth2, authentication method, if available */
		e_source_authentication_set_method (authentication_extension, "Google");

		/* See https://developers.google.com/google-apps/calendar/caldav/v2/guide */
		e_util_change_uri_component (&guri, SOUP_URI_HOST, "apidata.googleusercontent.com");
		e_util_change_uri_component (&guri, SOUP_URI_PATH, "/caldav/v2/");
	} else {
		e_util_change_uri_component (&guri, SOUP_URI_HOST, "www.google.com");
		/* To find also calendar email, not only calendars */
		e_util_change_uri_component (&guri, SOUP_URI_PATH, "/calendar/dav/");
	}

	/* Google's CalDAV interface requires a secure connection. */
	e_util_change_uri_component (&guri, SOUP_URI_SCHEME, "https");

	e_source_webdav_set_uri (webdav_extension, guri);

	switch (e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (self->priv->config))) {
	case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		supports_filter = E_WEBDAV_DISCOVER_SUPPORTS_EVENTS;
		title = _("Choose a Calendar");
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		supports_filter = E_WEBDAV_DISCOVER_SUPPORTS_MEMOS;
		title = _("Choose a Memo List");
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		supports_filter = E_WEBDAV_DISCOVER_SUPPORTS_TASKS;
		title = _("Choose a Task List");
		break;
	default:
		g_return_if_reached ();
	}

	prompter = e_credentials_prompter_new (registry);
	e_credentials_prompter_set_auto_prompt (prompter, FALSE);

	base_url = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);

	dialog = e_webdav_discover_dialog_new (parent, title, prompter, self->priv->source, base_url, supports_filter);

	if (parent != NULL)
		e_binding_bind_property (
			parent, "icon-name",
			dialog, "icon-name",
			G_BINDING_SYNC_CREATE);

	handler_id = g_signal_connect (prompter, "get-dialog-parent",
		G_CALLBACK (google_config_get_dialog_parent_cb), dialog);

	e_webdav_discover_dialog_refresh (dialog);

	if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
		gchar *href = NULL, *display_name = NULL, *color = NULL, *email;
		guint supports = 0, order = 0;
		GtkWidget *content;

		content = e_webdav_discover_dialog_get_content (dialog);

		if (e_webdav_discover_content_get_selected (content, 0, &href, &supports, &display_name, &color, &order)) {
			g_uri_unref (guri);
			guri = g_uri_parse (href, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

			if (guri) {
				ESourceSelectable *selectable_extension;
				const gchar *extension_name;

				switch (e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (self->priv->config))) {
					case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
						extension_name = E_SOURCE_EXTENSION_CALENDAR;
						break;
					case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
						extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
						break;
					case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
						extension_name = E_SOURCE_EXTENSION_TASK_LIST;
						break;
					default:
						g_return_if_reached ();
				}

				selectable_extension = e_source_get_extension (self->priv->source, extension_name);

				e_source_set_display_name (self->priv->source, display_name);

				e_source_webdav_set_display_name (webdav_extension, display_name);
				e_source_webdav_set_uri (webdav_extension, guri);
				e_source_webdav_set_order (webdav_extension, order);

				if (color && *color)
					e_source_selectable_set_color (selectable_extension, color);

				e_source_selectable_set_order (selectable_extension, order);
			}

			g_free (href);
			g_free (display_name);
			g_free (color);

			href = NULL;
			display_name = NULL;
			color = NULL;
		}

		email = e_webdav_discover_content_get_user_address (content);
		if (email && *email)
			e_source_webdav_set_email_address (webdav_extension, email);
		g_free (email);
	}

	g_signal_handler_disconnect (prompter, handler_id);

	gtk_widget_destroy (GTK_WIDGET (dialog));

	g_object_unref (prompter);
	if (guri)
		g_uri_unref (guri);
	g_free (base_url);
}

static void
e_google_chooser_button_class_init (EGoogleChooserButtonClass *class)
{
	GObjectClass *object_class;
	GtkButtonClass *button_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = google_chooser_button_set_property;
	object_class->get_property = google_chooser_button_get_property;
	object_class->dispose = google_chooser_button_dispose;
	object_class->constructed = google_chooser_button_constructed;

	button_class = GTK_BUTTON_CLASS (class);
	button_class->clicked = google_chooser_button_clicked;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			NULL,
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_CONFIG,
		g_param_spec_object (
			"config",
			NULL,
			NULL,
			E_TYPE_SOURCE_CONFIG,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_google_chooser_button_class_finalize (EGoogleChooserButtonClass *class)
{
}

static void
e_google_chooser_button_init (EGoogleChooserButton *button)
{
	button->priv = e_google_chooser_button_get_instance_private (button);
}

void
e_google_chooser_button_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_google_chooser_button_register_type (type_module);
}

GtkWidget *
e_google_chooser_button_new (ESource *source,
			     ESourceConfig *config)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return g_object_new (
		E_TYPE_GOOGLE_CHOOSER_BUTTON,
		"source", source,
		"config", config, NULL);
}

ESource *
e_google_chooser_button_get_source (EGoogleChooserButton *button)
{
	g_return_val_if_fail (E_IS_GOOGLE_CHOOSER_BUTTON (button), NULL);

	return button->priv->source;
}

ESourceConfig *
e_google_chooser_button_get_config (EGoogleChooserButton *button)
{
	g_return_val_if_fail (E_IS_GOOGLE_CHOOSER_BUTTON (button), NULL);

	return button->priv->config;
}

static gchar *
google_chooser_decode_user (const gchar *user)
{
	gchar *decoded_user;

	if (user == NULL || *user == '\0')
		return NULL;

	/* Decode any encoded 'at' symbols ('%40' -> '@'). */
	if (strstr (user, "%40") != NULL) {
		gchar **segments;

		segments = g_strsplit (user, "%40", 0);
		decoded_user = g_strjoinv ("@", segments);
		g_strfreev (segments);

	/* If no domain is given, append "@gmail.com". */
	} else if (strstr (user, "@") == NULL) {
		decoded_user = g_strconcat (user, "@gmail.com", NULL);

	/* Otherwise the user name should be fine as is. */
	} else {
		decoded_user = g_strdup (user);
	}

	return decoded_user;
}

void
e_google_chooser_button_construct_default_uri (GUri **inout_uri,
					       const gchar *username)
{
	gchar *decoded_user, *path;

	decoded_user = google_chooser_decode_user (username);
	if (!decoded_user)
		return;

	if (g_strcmp0 (g_uri_get_host (*inout_uri), "apidata.googleusercontent.com") == 0)
		path = g_strdup_printf ("/caldav/v2/%s/events", decoded_user);
	else
		path = g_strdup_printf ("/calendar/dav/%s/events", decoded_user);

	e_util_change_uri_component (inout_uri, SOUP_URI_USER, decoded_user);
	e_util_change_uri_component (inout_uri, SOUP_URI_PATH, path);

	g_free (decoded_user);
	g_free (path);
}
