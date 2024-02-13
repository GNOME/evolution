/*
 * e-source-config-dialog.c
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

#include "e-source-config-dialog.h"

#include "e-alert-bar.h"
#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-misc-utils.h"

struct _ESourceConfigDialogPrivate {
	ESourceConfig *config;
	ESourceRegistry *registry;

	GtkWidget *alert_bar;
	gulong alert_bar_visible_handler_id;
};

enum {
	PROP_0,
	PROP_CONFIG
};

/* Forward Declarations */
static void	e_source_config_dialog_alert_sink_init
					(EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ESourceConfigDialog, e_source_config_dialog, GTK_TYPE_DIALOG,
	G_ADD_PRIVATE (ESourceConfigDialog)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_source_config_dialog_alert_sink_init))

static void
source_config_dialog_commit_cb (GObject *object,
                                GAsyncResult *result,
                                gpointer user_data)
{
	ESourceConfig *config;
	ESourceConfigDialog *dialog;
	GdkWindow *gdk_window;
	GError *error = NULL;

	config = E_SOURCE_CONFIG (object);
	dialog = E_SOURCE_CONFIG_DIALOG (user_data);

	/* Set the cursor back to normal. */
	gdk_window = gtk_widget_get_window (GTK_WIDGET (dialog));
	gdk_window_set_cursor (gdk_window, NULL);

	/* Allow user interaction with window content. */
	gtk_widget_set_sensitive (GTK_WIDGET (dialog), TRUE);

	e_source_config_commit_finish (config, result, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_object_unref (dialog);
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			E_ALERT_SINK (dialog),
			"system:simple-error",
			error->message, NULL);
		g_object_unref (dialog);
		g_error_free (error);

	} else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

static void
source_config_dialog_commit (ESourceConfigDialog *dialog)
{
	GdkCursor *gdk_cursor;
	ESourceConfig *config;

	config = e_source_config_dialog_get_config (dialog);

	/* Clear any previous alerts. */
	e_alert_bar_clear (E_ALERT_BAR (dialog->priv->alert_bar));

	/* Make the cursor appear busy. */
	gdk_cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (dialog)), "wait");
	if (gdk_cursor) {
		GdkWindow *gdk_window;

		gdk_window = gtk_widget_get_window (GTK_WIDGET (dialog));
		gdk_window_set_cursor (gdk_window, gdk_cursor);
		g_object_unref (gdk_cursor);
	}

	/* Prevent user interaction with window content. */
	gtk_widget_set_sensitive (GTK_WIDGET (dialog), FALSE);

	/* XXX This operation is not cancellable. */
	e_source_config_commit (
		config, NULL,
		source_config_dialog_commit_cb,
		g_object_ref (dialog));
}

static void
source_config_dialog_source_removed_cb (ESourceRegistry *registry,
                                        ESource *removed_source,
                                        ESourceConfigDialog *dialog)
{
	ESourceConfig *config;
	ESource *original_source;

	/* If the ESource being edited is removed, cancel the dialog. */

	config = e_source_config_dialog_get_config (dialog);
	original_source = e_source_config_get_original_source (config);

	if (original_source == NULL)
		return;

	if (!e_source_equal (original_source, removed_source))
		return;

	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}

static void
source_config_alert_bar_visible_cb (EAlertBar *alert_bar,
                                    GParamSpec *pspec,
                                    ESourceConfigDialog *dialog)
{
	e_source_config_resize_window (dialog->priv->config);
}

static void
source_config_dialog_set_config (ESourceConfigDialog *dialog,
                                 ESourceConfig *config)
{
	ESourceRegistry *registry;

	g_return_if_fail (E_IS_SOURCE_CONFIG (config));
	g_return_if_fail (dialog->priv->config == NULL);

	dialog->priv->config = g_object_ref (config);

	registry = e_source_config_get_registry (config);
	dialog->priv->registry = g_object_ref (registry);

	g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (source_config_dialog_source_removed_cb), dialog);
}

static void
source_config_dialog_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONFIG:
			source_config_dialog_set_config (
				E_SOURCE_CONFIG_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_config_dialog_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONFIG:
			g_value_set_object (
				value,
				e_source_config_dialog_get_config (
				E_SOURCE_CONFIG_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_config_dialog_dispose (GObject *object)
{
	ESourceConfigDialog *self = E_SOURCE_CONFIG_DIALOG (object);

	g_clear_object (&self->priv->config);

	if (self->priv->registry) {
		g_signal_handlers_disconnect_matched (
			self->priv->registry, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->registry);
	}

	if (self->priv->alert_bar) {
		g_signal_handler_disconnect (
			self->priv->alert_bar,
			self->priv->alert_bar_visible_handler_id);
		g_clear_object (&self->priv->alert_bar);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_source_config_dialog_parent_class)->dispose (object);
}

static void
source_config_dialog_constructed (GObject *object)
{
	ESourceConfigDialog *self = E_SOURCE_CONFIG_DIALOG (object);
	GtkWidget *content_area;
	GtkWidget *config;
	GtkWidget *widget;
	gulong handler_id;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_source_config_dialog_parent_class)->constructed (object);

	config = GTK_WIDGET (self->priv->config);

	widget = gtk_dialog_get_widget_for_response (
		GTK_DIALOG (object), GTK_RESPONSE_OK);

	gtk_container_set_border_width (GTK_CONTAINER (object), 5);
	gtk_container_set_border_width (GTK_CONTAINER (config), 5);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (object));
	gtk_box_pack_start (GTK_BOX (content_area), config, TRUE, TRUE, 0);
	gtk_widget_show (config);

	/* Don't use G_BINDING_SYNC_CREATE here.  The ESourceConfig widget
	 * is not ready to run check_complete() until after it's realized. */
	e_binding_bind_property (
		config, "complete",
		widget, "sensitive",
		G_BINDING_DEFAULT);

	widget = e_alert_bar_new ();
	gtk_box_pack_start (GTK_BOX (content_area), widget, FALSE, FALSE, 0);
	self->priv->alert_bar = g_object_ref (widget);
	/* EAlertBar controls its own visibility. */

	handler_id = e_signal_connect_notify (
		self->priv->alert_bar, "notify::visible",
		G_CALLBACK (source_config_alert_bar_visible_cb), object);

	self->priv->alert_bar_visible_handler_id = handler_id;
}

static void
source_config_dialog_response (GtkDialog *dialog,
                               gint response_id)
{
	/* Do not chain up.  GtkDialog does not implement this method. */

	switch (response_id) {
		case GTK_RESPONSE_OK:
			source_config_dialog_commit (
				E_SOURCE_CONFIG_DIALOG (dialog));
			break;
		case GTK_RESPONSE_CANCEL:
			gtk_widget_destroy (GTK_WIDGET (dialog));
			break;
		default:
			break;
	}
}

static void
source_config_dialog_submit_alert (EAlertSink *alert_sink,
                                   EAlert *alert)
{
	ESourceConfigDialog *self = E_SOURCE_CONFIG_DIALOG (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (self->priv->alert_bar), alert);
}

static void
e_source_config_dialog_class_init (ESourceConfigDialogClass *class)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = source_config_dialog_set_property;
	object_class->get_property = source_config_dialog_get_property;
	object_class->dispose = source_config_dialog_dispose;
	object_class->constructed = source_config_dialog_constructed;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = source_config_dialog_response;

	g_object_class_install_property (
		object_class,
		PROP_CONFIG,
		g_param_spec_object (
			"config",
			"Config",
			"The ESourceConfig instance",
			E_TYPE_SOURCE_CONFIG,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_source_config_dialog_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = source_config_dialog_submit_alert;
}

static void
e_source_config_dialog_init (ESourceConfigDialog *dialog)
{
	dialog->priv = e_source_config_dialog_get_instance_private (dialog);

	gtk_dialog_add_buttons (
		GTK_DIALOG (dialog),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

GtkWidget *
e_source_config_dialog_new (ESourceConfig *config)
{
	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), NULL);

	return g_object_new (
		E_TYPE_SOURCE_CONFIG_DIALOG,
		"config", config, NULL);
}

ESourceConfig *
e_source_config_dialog_get_config (ESourceConfigDialog *dialog)
{
	g_return_val_if_fail (E_IS_SOURCE_CONFIG_DIALOG (dialog), NULL);

	return dialog->priv->config;
}
