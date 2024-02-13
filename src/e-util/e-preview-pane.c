/*
 * e-preview-pane.c
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

#include "e-preview-pane.h"

#include <gdk/gdkkeysyms.h>

#include "e-alert-bar.h"
#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-misc-utils.h"

struct _EPreviewPanePrivate {
	GtkWidget *alert_bar;
	GtkWidget *web_view;
	GtkWidget *search_bar;

	gulong web_view_new_activity_handler_id;
};

enum {
	PROP_0,
	PROP_SEARCH_BAR,
	PROP_WEB_VIEW
};

enum {
	SHOW_SEARCH_BAR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_preview_pane_alert_sink_init
					(EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EPreviewPane, e_preview_pane, GTK_TYPE_BOX,
	G_ADD_PRIVATE (EPreviewPane)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_preview_pane_alert_sink_init))

static void
preview_pane_web_view_new_activity_cb (EWebView *web_view,
                                       EActivity *activity,
                                       EPreviewPane *preview_pane)
{
	e_activity_set_alert_sink (activity, E_ALERT_SINK (preview_pane));
}

static void
preview_pane_alert_bar_visible_notify_cb (GtkWidget *alert_bar,
					  GParamSpec *param,
					  EPreviewPane *preview_pane)
{
	GtkWidget *toplevel, *focused;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));
	g_return_if_fail (E_IS_PREVIEW_PANE (preview_pane));

	if (gtk_widget_get_visible (alert_bar))
		return;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (preview_pane));
	focused = GTK_IS_WINDOW (toplevel) ? gtk_window_get_focus (GTK_WINDOW (toplevel)) : NULL;

	if (!focused && preview_pane->priv->web_view &&
	    gtk_widget_is_visible (preview_pane->priv->web_view)) {
		gtk_widget_grab_focus (preview_pane->priv->web_view);
	}
}

static void
preview_pane_set_web_view (EPreviewPane *preview_pane,
			   GtkWidget *web_view)
{
	gulong handler_id;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (preview_pane->priv->web_view == NULL);

	preview_pane->priv->web_view = g_object_ref_sink (web_view);

	handler_id = g_signal_connect (
		web_view, "new-activity",
		G_CALLBACK (preview_pane_web_view_new_activity_cb),
		preview_pane);
	preview_pane->priv->web_view_new_activity_handler_id = handler_id;
}

static void
preview_pane_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEB_VIEW:
			preview_pane_set_web_view (
				E_PREVIEW_PANE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
preview_pane_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SEARCH_BAR:
			g_value_set_object (
				value, e_preview_pane_get_search_bar (
				E_PREVIEW_PANE (object)));
			return;

		case PROP_WEB_VIEW:
			g_value_set_object (
				value, e_preview_pane_get_web_view (
				E_PREVIEW_PANE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
preview_pane_dispose (GObject *object)
{
	EPreviewPane *self = E_PREVIEW_PANE (object);

	if (self->priv->web_view_new_activity_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->web_view,
			self->priv->web_view_new_activity_handler_id);
		self->priv->web_view_new_activity_handler_id = 0;
	}

	g_clear_object (&self->priv->alert_bar);
	g_clear_object (&self->priv->search_bar);
	g_clear_object (&self->priv->web_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_preview_pane_parent_class)->dispose (object);
}

static void
preview_pane_constructed (GObject *object)
{
	EPreviewPane *self = E_PREVIEW_PANE (object);
	GtkWidget *widget;

	widget = e_alert_bar_new ();
	gtk_box_pack_start (GTK_BOX (object), widget, FALSE, FALSE, 0);
	self->priv->alert_bar = g_object_ref (widget);
	/* EAlertBar controls its own visibility. */

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (object), widget, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (widget), self->priv->web_view);
	gtk_widget_show (widget);
	gtk_widget_show (self->priv->web_view);

	widget = e_search_bar_new (E_WEB_VIEW (self->priv->web_view));
	gtk_box_pack_start (GTK_BOX (object), widget, FALSE, FALSE, 0);
	self->priv->search_bar = g_object_ref (widget);
	gtk_widget_hide (widget);

	e_signal_connect_notify (self->priv->alert_bar, "notify::visible",
		G_CALLBACK (preview_pane_alert_bar_visible_notify_cb), object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_preview_pane_parent_class)->constructed (object);
}

static void
preview_pane_show_search_bar (EPreviewPane *preview_pane)
{
	GtkWidget *search_bar;

	search_bar = preview_pane->priv->search_bar;

	if (!gtk_widget_get_visible (search_bar))
		gtk_widget_show (search_bar);
	else
		e_search_bar_focus_entry (E_SEARCH_BAR (search_bar));
}

static void
preview_pane_submit_alert (EAlertSink *alert_sink,
                           EAlert *alert)
{
	EPreviewPane *preview_pane;

	preview_pane = E_PREVIEW_PANE (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (preview_pane->priv->alert_bar), alert);
}

static void
e_preview_pane_class_init (EPreviewPaneClass *class)
{
	GObjectClass *object_class;
	GtkBindingSet *binding_set;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = preview_pane_set_property;
	object_class->get_property = preview_pane_get_property;
	object_class->dispose = preview_pane_dispose;
	object_class->constructed = preview_pane_constructed;

	class->show_search_bar = preview_pane_show_search_bar;

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_BAR,
		g_param_spec_object (
			"search-bar",
			"Search Bar",
			NULL,
			E_TYPE_SEARCH_BAR,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_WEB_VIEW,
		g_param_spec_object (
			"web-view",
			"Web View",
			NULL,
			E_TYPE_WEB_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[SHOW_SEARCH_BAR] = g_signal_new (
		"show-search-bar",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EPreviewPaneClass, show_search_bar),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);

	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_f,
		GDK_SHIFT_MASK | GDK_CONTROL_MASK,
		"show-search-bar", 0);
}

static void
e_preview_pane_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = preview_pane_submit_alert;
}

static void
e_preview_pane_init (EPreviewPane *preview_pane)
{
	preview_pane->priv = e_preview_pane_get_instance_private (preview_pane);

	gtk_box_set_spacing (GTK_BOX (preview_pane), 1);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (preview_pane), GTK_ORIENTATION_VERTICAL);
}

GtkWidget *
e_preview_pane_new (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return g_object_new (
		E_TYPE_PREVIEW_PANE,
		"web-view", web_view, NULL);
}

EWebView *
e_preview_pane_get_web_view (EPreviewPane *preview_pane)
{
	g_return_val_if_fail (E_IS_PREVIEW_PANE (preview_pane), NULL);

	return E_WEB_VIEW (preview_pane->priv->web_view);
}

ESearchBar *
e_preview_pane_get_search_bar (EPreviewPane *preview_pane)
{
	g_return_val_if_fail (E_IS_PREVIEW_PANE (preview_pane), NULL);

	return E_SEARCH_BAR (preview_pane->priv->search_bar);
}

void
e_preview_pane_clear_alerts (EPreviewPane *preview_pane)
{
	g_return_if_fail (E_IS_PREVIEW_PANE (preview_pane));

	e_alert_bar_clear (E_ALERT_BAR (preview_pane->priv->alert_bar));
}

void
e_preview_pane_show_search_bar (EPreviewPane *preview_pane)
{
	g_return_if_fail (E_IS_PREVIEW_PANE (preview_pane));

	g_signal_emit (preview_pane, signals[SHOW_SEARCH_BAR], 0);
}
