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

#include "e-google-chooser-button.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include "e-google-chooser-dialog.h"

#define E_GOOGLE_CHOOSER_BUTTON_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_GOOGLE_CHOOSER_BUTTON, EGoogleChooserButtonPrivate))

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

G_DEFINE_DYNAMIC_TYPE (
	EGoogleChooserButton,
	e_google_chooser_button,
	GTK_TYPE_BUTTON)

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
	EGoogleChooserButtonPrivate *priv;

	priv = E_GOOGLE_CHOOSER_BUTTON_GET_PRIVATE (object);

	g_clear_object (&priv->source);
	g_clear_object (&priv->config);
	g_clear_object (&priv->label);

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
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_container_add (GTK_CONTAINER (button), widget);
	button->priv->label = g_object_ref (widget);
	gtk_widget_show (widget);

	webdav_extension = e_source_get_extension (
		button->priv->source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
	display_name = e_source_webdav_get_display_name (webdav_extension);

	binding_flags = G_BINDING_DEFAULT;
	if (display_name != NULL && *display_name != '\0')
		binding_flags |= G_BINDING_SYNC_CREATE;

	g_object_bind_property (
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
	EGoogleChooserButtonPrivate *priv;
	gpointer parent;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	ECredentialsPrompter *prompter;
	ESourceWebdav *webdav_extension;
	ESourceAuthentication *authentication_extension;
	SoupURI *uri;
	GtkWidget *dialog;
	GtkWidget *widget;
	gulong handler_id;

	priv = E_GOOGLE_CHOOSER_BUTTON_GET_PRIVATE (button);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	registry = e_source_config_get_registry (priv->config);

	source_type = e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (priv->config));

	authentication_extension = e_source_get_extension (priv->source, E_SOURCE_EXTENSION_AUTHENTICATION);
	webdav_extension = e_source_get_extension (priv->source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	uri = e_source_webdav_dup_soup_uri (webdav_extension);

	e_google_chooser_construct_default_uri (uri, e_source_authentication_get_user (authentication_extension));

	/* The host name is fixed, obviously. */
	soup_uri_set_host (uri, "www.google.com");

	/* Google's CalDAV interface requires a secure connection. */
	soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTPS);

	e_source_webdav_set_soup_uri (webdav_extension, uri);

	widget = e_google_chooser_new (registry, priv->source, source_type);

	dialog = e_google_chooser_dialog_new (E_GOOGLE_CHOOSER (widget), parent);

	if (parent != NULL)
		g_object_bind_property (
			parent, "icon-name",
			dialog, "icon-name",
			G_BINDING_SYNC_CREATE);

	prompter = e_google_chooser_get_prompter (E_GOOGLE_CHOOSER (widget));

	handler_id = g_signal_connect (prompter, "get-dialog-parent",
		G_CALLBACK (google_config_get_dialog_parent_cb), dialog);

	gtk_dialog_run (GTK_DIALOG (dialog));

	g_signal_handler_disconnect (prompter, handler_id);

	gtk_widget_destroy (dialog);

	soup_uri_free (uri);
}

static void
e_google_chooser_button_class_init (EGoogleChooserButtonClass *class)
{
	GObjectClass *object_class;
	GtkButtonClass *button_class;

	g_type_class_add_private (class, sizeof (EGoogleChooserButtonPrivate));

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
	button->priv = E_GOOGLE_CHOOSER_BUTTON_GET_PRIVATE (button);
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
