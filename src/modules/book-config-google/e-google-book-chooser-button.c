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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libedataserverui/libedataserverui.h>

#include "e-google-book-chooser-button.h"

struct _EGoogleBookChooserButtonPrivate {
	ESource *source;
	ESourceConfig *config;
	GtkWidget *label;
};

enum {
	PROP_0,
	PROP_SOURCE,
	PROP_CONFIG
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EGoogleBookChooserButton, e_google_book_chooser_button, GTK_TYPE_BUTTON, 0,
	G_ADD_PRIVATE_DYNAMIC (EGoogleBookChooserButton))

static void
google_book_chooser_button_set_source (EGoogleBookChooserButton *button,
				       ESource *source)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (button->priv->source == NULL);

	button->priv->source = g_object_ref (source);
}

static void
google_book_chooser_button_set_config (EGoogleBookChooserButton *button,
				       ESourceConfig *config)
{
	g_return_if_fail (E_IS_SOURCE_CONFIG (config));
	g_return_if_fail (button->priv->config == NULL);

	button->priv->config = g_object_ref (config);
}

static void
google_book_chooser_button_set_property (GObject *object,
					 guint property_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE:
			google_book_chooser_button_set_source (
				E_GOOGLE_BOOK_CHOOSER_BUTTON (object),
				g_value_get_object (value));
			return;

		case PROP_CONFIG:
			google_book_chooser_button_set_config (
				E_GOOGLE_BOOK_CHOOSER_BUTTON (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_book_chooser_button_get_property (GObject *object,
					 guint property_id,
					 GValue *value,
					 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE:
			g_value_set_object (
				value,
				e_google_book_chooser_button_get_source (
				E_GOOGLE_BOOK_CHOOSER_BUTTON (object)));
			return;

		case PROP_CONFIG:
			g_value_set_object (
				value,
				e_google_book_chooser_button_get_config (
				E_GOOGLE_BOOK_CHOOSER_BUTTON (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_book_chooser_button_dispose (GObject *object)
{
	EGoogleBookChooserButton *button = E_GOOGLE_BOOK_CHOOSER_BUTTON (object);

	g_clear_object (&button->priv->source);
	g_clear_object (&button->priv->config);
	g_clear_object (&button->priv->label);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_google_book_chooser_button_parent_class)->dispose (object);
}

static void
google_book_chooser_button_constructed (GObject *object)
{
	EGoogleBookChooserButton *button;
	ESourceWebdav *webdav_extension;
	GBindingFlags binding_flags;
	GtkWidget *widget;
	const gchar *display_name;

	button = E_GOOGLE_BOOK_CHOOSER_BUTTON (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_google_book_chooser_button_parent_class)->constructed (object);

	widget = gtk_label_new (_("Default User Address Book"));
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
google_book_chooser_button_clicked (GtkButton *btn)
{
	EGoogleBookChooserButton *button;
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
	const gchar *title = NULL;

	button = E_GOOGLE_BOOK_CHOOSER_BUTTON (btn);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	registry = e_source_config_get_registry (button->priv->config);

	authentication_extension = e_source_get_extension (button->priv->source, E_SOURCE_EXTENSION_AUTHENTICATION);
	webdav_extension = e_source_get_extension (button->priv->source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	guri = e_source_webdav_dup_uri (webdav_extension);

	e_google_book_chooser_button_construct_default_uri (&guri, e_source_authentication_get_user (authentication_extension));

	/* Prefer 'Google', aka internal OAuth2, authentication method, if available */
	e_source_authentication_set_method (authentication_extension, "Google");

	/* See https://developers.google.com/people/carddav */
	e_util_change_uri_component (&guri, SOUP_URI_HOST, "www.googleapis.com");
	e_util_change_uri_component (&guri, SOUP_URI_PATH, "/.well-known/carddav");

	/* Google's CardDAV interface requires a secure connection. */
	e_util_change_uri_component (&guri, SOUP_URI_SCHEME, "https");

	e_source_webdav_set_uri (webdav_extension, guri);

	prompter = e_credentials_prompter_new (registry);
	e_credentials_prompter_set_auto_prompt (prompter, FALSE);

	supports_filter = E_WEBDAV_DISCOVER_SUPPORTS_CONTACTS;
	title = _("Choose an Address Book");
	base_url = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);

	dialog = e_webdav_discover_dialog_new (parent, title, prompter, button->priv->source, base_url, supports_filter);

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
				ESourceAddressBook *addressbook_extension;

				addressbook_extension = e_source_get_extension (button->priv->source, E_SOURCE_EXTENSION_ADDRESS_BOOK);

				e_source_set_display_name (button->priv->source, display_name);

				e_source_webdav_set_display_name (webdav_extension, display_name);
				e_source_webdav_set_uri (webdav_extension, guri);
				e_source_webdav_set_order (webdav_extension, order);

				e_source_address_book_set_order (addressbook_extension, order);
			}

			g_clear_pointer (&href, g_free);
			g_clear_pointer (&display_name, g_free);
			g_clear_pointer (&color, g_free);
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
e_google_book_chooser_button_class_init (EGoogleBookChooserButtonClass *class)
{
	GObjectClass *object_class;
	GtkButtonClass *button_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = google_book_chooser_button_set_property;
	object_class->get_property = google_book_chooser_button_get_property;
	object_class->dispose = google_book_chooser_button_dispose;
	object_class->constructed = google_book_chooser_button_constructed;

	button_class = GTK_BUTTON_CLASS (class);
	button_class->clicked = google_book_chooser_button_clicked;

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
e_google_book_chooser_button_class_finalize (EGoogleBookChooserButtonClass *class)
{
}

static void
e_google_book_chooser_button_init (EGoogleBookChooserButton *button)
{
	button->priv = e_google_book_chooser_button_get_instance_private (button);
}

void
e_google_book_chooser_button_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_google_book_chooser_button_register_type (type_module);
}

GtkWidget *
e_google_book_chooser_button_new (ESource *source,
				  ESourceConfig *config)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return g_object_new (
		E_TYPE_GOOGLE_BOOK_CHOOSER_BUTTON,
		"source", source,
		"config", config, NULL);
}

ESource *
e_google_book_chooser_button_get_source (EGoogleBookChooserButton *button)
{
	g_return_val_if_fail (E_IS_GOOGLE_BOOK_CHOOSER_BUTTON (button), NULL);

	return button->priv->source;
}

ESourceConfig *
e_google_book_chooser_button_get_config (EGoogleBookChooserButton *button)
{
	g_return_val_if_fail (E_IS_GOOGLE_BOOK_CHOOSER_BUTTON (button), NULL);

	return button->priv->config;
}

static gchar *
google_book_chooser_decode_user (const gchar *user)
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
e_google_book_chooser_button_construct_default_uri (GUri **inout_uri,
						    const gchar *username)
{
	gchar *decoded_user, *path;

	decoded_user = google_book_chooser_decode_user (username);
	if (!decoded_user)
		return;

	path = g_strdup_printf ("/carddav/v1/principals/%s/lists/default/", decoded_user);

	e_util_change_uri_component (inout_uri, SOUP_URI_SCHEME, "https");
	e_util_change_uri_component (inout_uri, SOUP_URI_USER, decoded_user);
	e_util_change_uri_component (inout_uri, SOUP_URI_HOST, "www.googleapis.com");
	e_util_change_uri_component (inout_uri, SOUP_URI_PATH, path);

	g_free (decoded_user);
	g_free (path);
}
