/*
 * e-mail-config-activity-page.c
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

#include "e-mail-config-activity-page.h"

#include <camel/camel.h>

#define E_MAIL_CONFIG_ACTIVITY_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE, EMailConfigActivityPagePrivate))

struct _EMailConfigActivityPagePrivate {
	GtkWidget *activity_bar;	/* not referenced */
	GtkWidget *alert_bar;		/* not referenced */
};

/* Forward Declarations */
static void	e_mail_config_activity_page_alert_sink_init
					(EAlertSinkInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (
	EMailConfigActivityPage,
	e_mail_config_activity_page,
	GTK_TYPE_BOX,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK,
		e_mail_config_activity_page_alert_sink_init))

static void
mail_config_activity_page_constructed (GObject *object)
{
	EMailConfigActivityPage *page;
	GtkWidget *frame;
	GtkWidget *widget;

	page = E_MAIL_CONFIG_ACTIVITY_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_activity_page_parent_class)->constructed (object);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (page), GTK_ORIENTATION_VERTICAL);

	/* Does not matter what order the EActivityBar and EAlertBar are
	 * packed.  They should never both be visible at the same time. */

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_end (GTK_BOX (page), frame, FALSE, FALSE, 0);
	/* Visibility is bound to the EActivityBar. */

	widget = e_activity_bar_new ();
	gtk_container_add (GTK_CONTAINER (frame), widget);
	page->priv->activity_bar = widget;  /* do not reference */
	/* EActivityBar controls its own visibility. */

	e_binding_bind_property (
		widget, "visible",
		frame, "visible",
		G_BINDING_SYNC_CREATE);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_end (GTK_BOX (page), frame, FALSE, FALSE, 0);
	/* Visibility is bound to the EAlertBar. */

	widget = e_alert_bar_new ();
	gtk_container_add (GTK_CONTAINER (frame), widget);
	page->priv->alert_bar = widget;  /* do not reference */
	/* EAlertBar controls its own visibility. */

	e_binding_bind_property (
		widget, "visible",
		frame, "visible",
		G_BINDING_SYNC_CREATE);
}

static void
mail_config_activity_page_submit_alert (EAlertSink *alert_sink,
                                        EAlert *alert)
{
	EMailConfigActivityPagePrivate *priv;
	EAlertBar *alert_bar;
	GtkWidget *dialog;
	gpointer parent;

	priv = E_MAIL_CONFIG_ACTIVITY_PAGE_GET_PRIVATE (alert_sink);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (alert_sink));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
		case GTK_MESSAGE_WARNING:
		case GTK_MESSAGE_ERROR:
			alert_bar = E_ALERT_BAR (priv->alert_bar);
			e_alert_bar_add_alert (alert_bar, alert);
			break;

		default:
			dialog = e_alert_dialog_new (parent, alert);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			break;
	}
}

static void
e_mail_config_activity_page_class_init (EMailConfigActivityPageClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailConfigActivityPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_config_activity_page_constructed;
}

static void
e_mail_config_activity_page_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = mail_config_activity_page_submit_alert;
}

static void
e_mail_config_activity_page_init (EMailConfigActivityPage *page)
{
	page->priv = E_MAIL_CONFIG_ACTIVITY_PAGE_GET_PRIVATE (page);
}

EActivity *
e_mail_config_activity_page_new_activity (EMailConfigActivityPage *page)
{
	EActivity *activity;
	EActivityBar *activity_bar;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_ACTIVITY_PAGE (page), NULL);

	/* Clear any previous alerts. */
	e_alert_bar_clear (E_ALERT_BAR (page->priv->alert_bar));

	activity = e_activity_new ();

	e_activity_set_alert_sink (activity, E_ALERT_SINK (page));

	cancellable = camel_operation_new ();
	e_activity_set_cancellable (activity, cancellable);
	g_object_unref (cancellable);

	activity_bar = E_ACTIVITY_BAR (page->priv->activity_bar);
	e_activity_bar_set_activity (activity_bar, activity);

	return activity;
}

