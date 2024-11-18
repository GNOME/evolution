/*
 * e-activity-bar.c
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
#include <libedataserver/libedataserver.h>

#include "e-dialog-widgets.h"
#include "e-misc-utils.h"
#include "e-spinner.h"

#include "e-activity-bar.h"

#define FEEDBACK_PERIOD		1 /* seconds */
#define COMPLETED_ICON_NAME	"emblem-default"

struct _EActivityBarPrivate {
	EActivity *activity;	/* weak reference */
	GtkWidget *image;	/* not referenced */
	GtkWidget *label;	/* not referenced */
	GtkWidget *cancel;	/* not referenced */
	GtkWidget *spinner;	/* not referenced */

	/* If the user clicks the Cancel button, keep the cancelled
	 * EActivity object alive for a short duration so the user
	 * gets some visual feedback that cancellation worked. */
	guint timeout_id;
};

enum {
	PROP_0,
	PROP_ACTIVITY
};

G_DEFINE_TYPE_WITH_PRIVATE (EActivityBar, e_activity_bar, GTK_TYPE_INFO_BAR)

typedef struct _EActivityBarTimeoutData {
	EActivityBar *bar;
	EActivity *activity;
} EActivityBarTimeoutData;

/* This is needed, because the scheduled timeout can hold the last
   reference to the 'activity', which means removing the source will
   free it, which in turn calls e_activity_bar_set_activity() with NULL,
   which would call g_source_remove() again, with the same timeout id.
*/
static void
activity_bar_unset_timeout_id (EActivityBar *bar)
{
	guint timeout_id;

	g_return_if_fail (E_IS_ACTIVITY_BAR (bar));

	timeout_id = bar->priv->timeout_id;
	bar->priv->timeout_id = 0;

	if (timeout_id)
		g_source_remove (timeout_id);
}

static void
activity_bar_timeout_data_free (gpointer ptr)
{
	EActivityBarTimeoutData *data = ptr;

	if (data) {
		g_object_unref (data->activity);
		g_slice_free (EActivityBarTimeoutData, data);
	}
}

static gboolean
activity_bar_timeout_reached (gpointer user_data)
{
	EActivityBarTimeoutData *data = user_data;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (E_IS_ACTIVITY_BAR (data->bar), FALSE);

	if (!g_source_is_destroyed (g_main_current_source ()) &&
	    g_source_get_id (g_main_current_source ()) == data->bar->priv->timeout_id)
		data->bar->priv->timeout_id = 0;

	return FALSE;
}

static void
activity_bar_feedback (EActivityBar *bar)
{
	EActivity *activity;
	EActivityState state;
	EActivityBarTimeoutData *data;

	activity = e_activity_bar_get_activity (bar);
	g_return_if_fail (E_IS_ACTIVITY (activity));

	state = e_activity_get_state (activity);
	if (state != E_ACTIVITY_CANCELLED && state != E_ACTIVITY_COMPLETED)
		return;

	activity_bar_unset_timeout_id (bar);

	data = g_slice_new0 (EActivityBarTimeoutData);

	data->bar = bar;
	data->activity = g_object_ref (activity);

	/* Hold a reference on the EActivity for a short
	 * period so the activity bar stays visible. */
	bar->priv->timeout_id = e_named_timeout_add_seconds_full (
		G_PRIORITY_LOW, FEEDBACK_PERIOD, activity_bar_timeout_reached,
		data, activity_bar_timeout_data_free);
}

static void
activity_bar_update (EActivityBar *bar)
{
	EActivity *activity;
	EActivityState state;
	GCancellable *cancellable;
	const gchar *icon_name;
	gboolean sensitive;
	gboolean visible;
	gchar *description;

	activity = e_activity_bar_get_activity (bar);

	if (activity == NULL) {
		gtk_widget_hide (GTK_WIDGET (bar));
		return;
	}

	cancellable = e_activity_get_cancellable (activity);
	icon_name = e_activity_get_icon_name (activity);
	state = e_activity_get_state (activity);

	description = e_activity_describe (activity);
	gtk_label_set_text (GTK_LABEL (bar->priv->label), description);

	if (state == E_ACTIVITY_CANCELLED) {
		PangoAttribute *attr;
		PangoAttrList *attr_list;

		attr_list = pango_attr_list_new ();

		attr = pango_attr_strikethrough_new (TRUE);
		pango_attr_list_insert (attr_list, attr);

		gtk_label_set_attributes (
			GTK_LABEL (bar->priv->label), attr_list);

		pango_attr_list_unref (attr_list);
	} else
		gtk_label_set_attributes (
			GTK_LABEL (bar->priv->label), NULL);

	if (state == E_ACTIVITY_COMPLETED)
		icon_name = COMPLETED_ICON_NAME;

	if (state == E_ACTIVITY_CANCELLED) {
		gtk_image_set_from_icon_name (
			GTK_IMAGE (bar->priv->image),
			"process-stop", GTK_ICON_SIZE_BUTTON);
		gtk_widget_show (bar->priv->image);
	} else if (icon_name != NULL) {
		gtk_image_set_from_icon_name (
			GTK_IMAGE (bar->priv->image),
			icon_name, GTK_ICON_SIZE_BUTTON);
		gtk_widget_show (bar->priv->image);
	} else {
		gtk_widget_hide (bar->priv->image);
	}

	visible = (cancellable != NULL);
	gtk_widget_set_visible (bar->priv->cancel, visible);

	sensitive = (state == E_ACTIVITY_RUNNING);
	gtk_widget_set_sensitive (bar->priv->cancel, sensitive);

	visible = (description != NULL && *description != '\0');
	gtk_widget_set_visible (GTK_WIDGET (bar), visible);

	g_free (description);
}

static void
activity_bar_cancel (EActivityBar *bar)
{
	EActivity *activity;

	activity = e_activity_bar_get_activity (bar);
	g_return_if_fail (E_IS_ACTIVITY (activity));

	e_activity_cancel (activity);

	activity_bar_update (bar);
}

static void
activity_bar_weak_notify_cb (EActivityBar *bar,
                             GObject *where_the_object_was)
{
	g_return_if_fail (E_IS_ACTIVITY_BAR (bar));

	bar->priv->activity = NULL;
	e_activity_bar_set_activity (bar, NULL);
}

static void
activity_bar_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVITY:
			e_activity_bar_set_activity (
				E_ACTIVITY_BAR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
activity_bar_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVITY:
			g_value_set_object (
				value, e_activity_bar_get_activity (
				E_ACTIVITY_BAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
activity_bar_constructed (GObject *object)
{
	GObject *revealer;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_activity_bar_parent_class)->constructed (object);

	/* Disable animation of the revealer, until GtkInfoBar's bug #710888 is fixed */
	revealer = gtk_widget_get_template_child (GTK_WIDGET (object), GTK_TYPE_INFO_BAR, "revealer");
	if (revealer) {
		gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
		gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 0);
	}
}

static void
activity_bar_dispose (GObject *object)
{
	EActivityBar *self = E_ACTIVITY_BAR (object);

	activity_bar_unset_timeout_id (self);

	if (self->priv->activity != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->activity, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_weak_unref (
			G_OBJECT (self->priv->activity), (GWeakNotify)
			activity_bar_weak_notify_cb, object);
		self->priv->activity = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_activity_bar_parent_class)->dispose (object);
}

static void
e_activity_bar_class_init (EActivityBarClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = activity_bar_set_property;
	object_class->get_property = activity_bar_get_property;
	object_class->constructed = activity_bar_constructed;
	object_class->dispose = activity_bar_dispose;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVITY,
		g_param_spec_object (
			"activity",
			NULL,
			NULL,
			E_TYPE_ACTIVITY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_activity_bar_init (EActivityBar *bar)
{
	GtkWidget *container;
	GtkWidget *widget;

	bar->priv = e_activity_bar_get_instance_private (bar);

	container = gtk_info_bar_get_content_area (GTK_INFO_BAR (bar));

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->image = widget;

	widget = e_spinner_new ();
	e_spinner_start (E_SPINNER (widget));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->spinner = widget;

	/* The spinner is only visible when the image is not. */
	e_binding_bind_property (
		bar->priv->image, "visible",
		bar->priv->spinner, "visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	bar->priv->label = widget;
	gtk_widget_show (widget);

	/* This is only shown if the EActivity has a GCancellable. */
	widget = e_dialog_button_new_with_icon ("process-stop", _("_Cancel"));
	gtk_info_bar_add_action_widget (
		GTK_INFO_BAR (bar), widget, GTK_RESPONSE_CANCEL);
	bar->priv->cancel = widget;
	gtk_widget_hide (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (activity_bar_cancel), bar);
}

GtkWidget *
e_activity_bar_new (void)
{
	return g_object_new (E_TYPE_ACTIVITY_BAR, NULL);
}

EActivity *
e_activity_bar_get_activity (EActivityBar *bar)
{
	g_return_val_if_fail (E_IS_ACTIVITY_BAR (bar), NULL);

	return bar->priv->activity;
}

void
e_activity_bar_set_activity (EActivityBar *bar,
                             EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY_BAR (bar));

	if (activity != NULL)
		g_return_if_fail (E_IS_ACTIVITY (activity));

	activity_bar_unset_timeout_id (bar);

	if (bar->priv->activity != NULL) {
		g_signal_handlers_disconnect_matched (
			bar->priv->activity, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, bar);
		g_object_weak_unref (
			G_OBJECT (bar->priv->activity),
			(GWeakNotify) activity_bar_weak_notify_cb, bar);
	}

	bar->priv->activity = activity;

	if (activity != NULL) {
		g_object_weak_ref (
			G_OBJECT (activity), (GWeakNotify)
			activity_bar_weak_notify_cb, bar);

		g_signal_connect_swapped (
			activity, "notify::state",
			G_CALLBACK (activity_bar_feedback), bar);

		g_signal_connect_swapped (
			activity, "notify",
			G_CALLBACK (activity_bar_update), bar);
	}

	activity_bar_update (bar);

	g_object_notify (G_OBJECT (bar), "activity");
}
