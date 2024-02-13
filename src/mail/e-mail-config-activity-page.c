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

#include "evolution-config.h"

#include "e-mail-config-activity-page.h"

#include <camel/camel.h>

struct _EMailConfigActivityPagePrivate {
	GtkWidget *box;			/* not referenced */
	GtkWidget *activity_bar;	/* not referenced */
	GtkWidget *alert_bar;		/* not referenced */
};

/* Forward Declarations */
static void	e_mail_config_activity_page_alert_sink_init
					(EAlertSinkInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (EMailConfigActivityPage, e_mail_config_activity_page, GTK_TYPE_SCROLLED_WINDOW,
	G_ADD_PRIVATE (EMailConfigActivityPage)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_mail_config_activity_page_alert_sink_init))

static void
mail_config_activity_page_constructed (GObject *object)
{
	EMailConfigActivityPage *page;
	GtkWidget *frame;
	GtkWidget *widget;

	page = E_MAIL_CONFIG_ACTIVITY_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_activity_page_parent_class)->constructed (object);

	page->priv->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	/* Does not matter what order the EActivityBar and EAlertBar are
	 * packed.  They should never both be visible at the same time. */

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_end (GTK_BOX (page->priv->box), frame, FALSE, FALSE, 0);
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
	gtk_box_pack_end (GTK_BOX (page->priv->box), frame, FALSE, FALSE, 0);
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
	EMailConfigActivityPage *self = E_MAIL_CONFIG_ACTIVITY_PAGE (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (self->priv->alert_bar), alert);
}

static void
e_mail_config_activity_page_class_init (EMailConfigActivityPageClass *class)
{
	GObjectClass *object_class;

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
	page->priv = e_mail_config_activity_page_get_instance_private (page);
}

GtkWidget *
e_mail_config_activity_page_get_internal_box (EMailConfigActivityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_ACTIVITY_PAGE (page), NULL);

	return page->priv->box;
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

