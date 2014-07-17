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
	GtkWidget *label;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
	PROP_SOURCE
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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_chooser_button_dispose (GObject *object)
{
	EGoogleChooserButtonPrivate *priv;

	priv = E_GOOGLE_CHOOSER_BUTTON_GET_PRIVATE (object);

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	if (priv->label != NULL) {
		g_object_unref (priv->label);
		priv->label = NULL;
	}

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

static void
google_chooser_button_clicked (GtkButton *button)
{
	EGoogleChooserButtonPrivate *priv;
	GtkWidget *chooser;
	GtkWidget *dialog;
	gpointer parent;

	priv = E_GOOGLE_CHOOSER_BUTTON_GET_PRIVATE (button);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	chooser = e_google_chooser_new (priv->source);

	dialog = e_google_chooser_dialog_new (
		E_GOOGLE_CHOOSER (chooser), parent);

	if (parent != NULL)
		g_object_bind_property (
			parent, "icon-name",
			dialog, "icon-name",
			G_BINDING_SYNC_CREATE);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
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
e_google_chooser_button_new (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return g_object_new (
		E_TYPE_GOOGLE_CHOOSER_BUTTON,
		"source", source, NULL);
}

ESource *
e_google_chooser_button_get_source (EGoogleChooserButton *button)
{
	g_return_val_if_fail (E_IS_GOOGLE_CHOOSER_BUTTON (button), NULL);

	return button->priv->source;
}

