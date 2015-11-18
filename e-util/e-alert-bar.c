/*
 * e-alert-bar.c
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

#include "e-alert-bar.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include "e-dialog-widgets.h"

#define E_ALERT_BAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ALERT_BAR, EAlertBarPrivate))

#define E_ALERT_BAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ALERT_BAR, EAlertBarPrivate))

/* GTK_ICON_SIZE_DIALOG is a tad too big. */
#define ICON_SIZE GTK_ICON_SIZE_DND

/* Dismiss warnings automatically after 5 minutes. */
#define WARNING_TIMEOUT_SECONDS (5 * 60)

struct _EAlertBarPrivate {
	GQueue alerts;
	GtkWidget *image;		/* not referenced */
	GtkWidget *primary_label;	/* not referenced */
	GtkWidget *secondary_label;	/* not referenced */
};

G_DEFINE_TYPE (
	EAlertBar,
	e_alert_bar,
	GTK_TYPE_INFO_BAR)

static void
alert_bar_response_close (EAlert *alert)
{
	e_alert_response (alert, GTK_RESPONSE_CLOSE);
}

static void
alert_bar_show_alert (EAlertBar *alert_bar)
{
	GtkImage *image;
	GtkInfoBar *info_bar;
	GtkWidget *action_area;
	GtkWidget *widget;
	EAlert *alert;
	GList *link;
	GList *children;
	GtkMessageType message_type;
	const gchar *primary_text;
	const gchar *secondary_text;
	const gchar *icon_name;
	gboolean have_primary_text;
	gboolean have_secondary_text;
	gboolean visible;
	gint response_id;
	gchar *markup;

	info_bar = GTK_INFO_BAR (alert_bar);
	action_area = gtk_info_bar_get_action_area (info_bar);

	alert = g_queue_peek_head (&alert_bar->priv->alerts);
	g_return_if_fail (E_IS_ALERT (alert));

	/* Remove all buttons from the previous alert. */
	children = gtk_container_get_children (GTK_CONTAINER (action_area));
	while (children != NULL) {
		GtkWidget *child = GTK_WIDGET (children->data);
		gtk_container_remove (GTK_CONTAINER (action_area), child);
		children = g_list_delete_link (children, children);
	}

	/* Add alert-specific buttons. */
	link = e_alert_peek_actions (alert);
	while (link != NULL) {
		GtkAction *action = GTK_ACTION (link->data);

		/* These actions are already wired to trigger an
		 * EAlert::response signal when activated, which
		 * will in turn call gtk_info_bar_response(), so
		 * we can add buttons directly to the action
		 * area without knowning their response IDs. */

		widget = gtk_button_new ();

		gtk_activatable_set_related_action (GTK_ACTIVATABLE (widget), action);
		gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);

		link = g_list_next (link);
	}

	link = e_alert_peek_widgets (alert);
	while (link != NULL) {
		widget = link->data;

		gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);
		link = g_list_next (link);
	}

	/* Add a dismiss button. */
	widget = e_dialog_button_new_with_icon ("window-close", NULL);
	gtk_button_set_relief (
		GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (
		/* Translators: Escape is a keyboard binding. */
		widget, _("Close this message (Escape)"));
	gtk_box_pack_end (
		GTK_BOX (action_area), widget, FALSE, FALSE, 0);
	gtk_button_box_set_child_non_homogeneous (
		GTK_BUTTON_BOX (action_area), widget, TRUE);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (alert_bar_response_close), alert);

	primary_text = e_alert_get_primary_text (alert);
	secondary_text = e_alert_get_secondary_text (alert);

	if (primary_text == NULL)
		primary_text = "";

	if (secondary_text == NULL)
		secondary_text = "";

	have_primary_text = (*primary_text != '\0');
	have_secondary_text = (*secondary_text != '\0');

	response_id = e_alert_get_default_response (alert);
	gtk_info_bar_set_default_response (info_bar, response_id);

	message_type = e_alert_get_message_type (alert);
	gtk_info_bar_set_message_type (info_bar, message_type);

	widget = alert_bar->priv->primary_label;
	if (have_primary_text && have_secondary_text)
		markup = g_markup_printf_escaped (
			"<b>%s</b>", primary_text);
	else
		markup = g_markup_escape_text (primary_text, -1);
	gtk_label_set_markup (GTK_LABEL (widget), markup);
	gtk_widget_set_visible (widget, have_primary_text);
	g_free (markup);

	widget = alert_bar->priv->secondary_label;
	if (have_primary_text && have_secondary_text)
		markup = g_markup_printf_escaped (
			"<small>%s</small>", secondary_text);
	else
		markup = g_markup_escape_text (secondary_text, -1);
	gtk_label_set_markup (GTK_LABEL (widget), markup);
	gtk_widget_set_visible (widget, have_secondary_text);
	g_free (markup);

	icon_name = e_alert_get_icon_name (alert);
	image = GTK_IMAGE (alert_bar->priv->image);
	gtk_image_set_from_icon_name (image, icon_name, ICON_SIZE);

	/* Avoid showing an image for empty alerts. */
	visible = have_primary_text || have_secondary_text;
	gtk_widget_set_visible (alert_bar->priv->image, visible);

	gtk_widget_show (GTK_WIDGET (alert_bar));

	/* Warnings are generally meant for transient errors.
	 * No need to leave them up indefinitely.  Close them
	 * automatically if the user hasn't responded after a
	 * reasonable period of time has elapsed. */
	if (message_type == GTK_MESSAGE_WARNING)
		e_alert_start_timer (alert, WARNING_TIMEOUT_SECONDS);
}

static void
alert_bar_response_cb (EAlert *alert,
                       gint response_id,
                       EAlertBar *alert_bar)
{
	GQueue *queue;
	EAlert *head;
	gboolean was_head;

	queue = &alert_bar->priv->alerts;
	head = g_queue_peek_head (queue);
	was_head = (alert == head);

	g_signal_handlers_disconnect_by_func (
		alert, alert_bar_response_cb, alert_bar);

	if (g_queue_remove (queue, alert))
		g_object_unref (alert);

	if (g_queue_is_empty (queue)) {
		GtkWidget *action_area;
		GList *children;

		gtk_widget_hide (GTK_WIDGET (alert_bar));

		action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (alert_bar));

		/* Remove all buttons from the previous alert. */
		children = gtk_container_get_children (GTK_CONTAINER (action_area));
		while (children != NULL) {
			GtkWidget *child = GTK_WIDGET (children->data);
			gtk_container_remove (GTK_CONTAINER (action_area), child);
			children = g_list_delete_link (children, children);
		}
	} else if (was_head) {
		GtkInfoBar *info_bar = GTK_INFO_BAR (alert_bar);
		gtk_info_bar_response (info_bar, response_id);
		alert_bar_show_alert (alert_bar);
	}
}

static void
alert_bar_dispose (GObject *object)
{
	EAlertBarPrivate *priv;

	priv = E_ALERT_BAR_GET_PRIVATE (object);

	while (!g_queue_is_empty (&priv->alerts)) {
		EAlert *alert = g_queue_pop_head (&priv->alerts);
		g_signal_handlers_disconnect_by_func (
			alert, alert_bar_response_cb, object);
		g_object_unref (alert);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_alert_bar_parent_class)->dispose (object);
}

static void
alert_bar_constructed (GObject *object)
{
	EAlertBarPrivate *priv;
	GtkInfoBar *info_bar;
	GtkWidget *action_area;
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;
	GObject *revealer;

	priv = E_ALERT_BAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_alert_bar_parent_class)->constructed (object);

	g_queue_init (&priv->alerts);

	info_bar = GTK_INFO_BAR (object);
	action_area = gtk_info_bar_get_action_area (info_bar);
	content_area = gtk_info_bar_get_content_area (info_bar);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (action_area), GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_valign (action_area, GTK_ALIGN_START);

	container = content_area;

	widget = gtk_image_new ();
	gtk_misc_set_alignment (GTK_MISC (widget), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->image = widget;
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->primary_label = widget;
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->secondary_label = widget;
	gtk_widget_show (widget);

	container = action_area;

	/* Disable animation of the revealer, until GtkInfoBar's bug #710888 is fixed */
	revealer = gtk_widget_get_template_child (GTK_WIDGET (object), GTK_TYPE_INFO_BAR, "revealer");
	if (revealer) {
		gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
		gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 0);
	}
}

static GtkSizeRequestMode
alert_bar_get_request_mode (GtkWidget *widget)
{
	/* GtkBox does width-for-height by default.  But we
	 * want the alert bar to be as short as possible. */
	return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
alert_bar_close (GtkInfoBar *info_bar)
{
	/* GtkInfoBar's close() method looks for a button with a response
	 * code of GTK_RESPONSE_CANCEL.  But that does not apply here, so
	 * we have to override the method. */
	e_alert_bar_close_alert (E_ALERT_BAR (info_bar));
}

static void
e_alert_bar_class_init (EAlertBarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkInfoBarClass *info_bar_class;

	g_type_class_add_private (class, sizeof (EAlertBarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = alert_bar_dispose;
	object_class->constructed = alert_bar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_request_mode = alert_bar_get_request_mode;

	info_bar_class = GTK_INFO_BAR_CLASS (class);
	info_bar_class->close = alert_bar_close;
}

static void
e_alert_bar_init (EAlertBar *alert_bar)
{
	alert_bar->priv = E_ALERT_BAR_GET_PRIVATE (alert_bar);
}

GtkWidget *
e_alert_bar_new (void)
{
	return g_object_new (E_TYPE_ALERT_BAR, NULL);
}

void
e_alert_bar_clear (EAlertBar *alert_bar)
{
	GQueue *queue;
	EAlert *alert;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));

	queue = &alert_bar->priv->alerts;

	while ((alert = g_queue_pop_head (queue)) != NULL)
		alert_bar_response_close (alert);
}

typedef struct {
	gboolean found;
	EAlert *looking_for;
} DuplicateData;

static void
alert_bar_find_duplicate_cb (EAlert *alert,
                             DuplicateData *dd)
{
	g_return_if_fail (dd->looking_for != NULL);

	dd->found |= (
		e_alert_get_message_type (alert) ==
		e_alert_get_message_type (dd->looking_for) &&
		g_strcmp0 (
			e_alert_get_primary_text (alert),
			e_alert_get_primary_text (dd->looking_for)) == 0 &&
		g_strcmp0 (
			e_alert_get_secondary_text (alert),
			e_alert_get_secondary_text (dd->looking_for)) == 0);
}

void
e_alert_bar_add_alert (EAlertBar *alert_bar,
                       EAlert *alert)
{
	DuplicateData dd;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));
	g_return_if_fail (E_IS_ALERT (alert));

	dd.found = FALSE;
	dd.looking_for = alert;

	g_queue_foreach (
		&alert_bar->priv->alerts,
		(GFunc) alert_bar_find_duplicate_cb, &dd);

	if (dd.found)
		return;

	g_signal_connect (
		alert, "response",
		G_CALLBACK (alert_bar_response_cb), alert_bar);

	g_queue_push_head (&alert_bar->priv->alerts, g_object_ref (alert));

	alert_bar_show_alert (alert_bar);
}

/**
 * e_alert_bar_close_alert:
 * @alert_bar: an #EAlertBar
 *
 * Closes the active #EAlert and returns %TRUE, or else returns %FALSE if
 * there is no active #EAlert.
 *
 * Returns: whether an #EAlert was closed
 **/
gboolean
e_alert_bar_close_alert (EAlertBar *alert_bar)
{
	EAlert *alert;
	gboolean alert_closed = FALSE;

	g_return_val_if_fail (E_IS_ALERT_BAR (alert_bar), FALSE);

	alert = g_queue_peek_head (&alert_bar->priv->alerts);

	if (alert != NULL) {
		alert_bar_response_close (alert);
		alert_closed = TRUE;
	}

	return alert_closed;
}
