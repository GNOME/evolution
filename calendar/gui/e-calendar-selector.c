/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-calendar-selector.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#include <config.h>

#include <glib/gi18n.h>

#include "e-calendar-selector.h"
#include "comp-util.h"

#include <libecal/libecal.h>

#define E_CALENDAR_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CALENDAR_SELECTOR, ECalendarSelectorPrivate))

struct _ECalendarSelectorPrivate {
	EShellView *shell_view;

	gpointer transfer_alert; /* weak pointer to EAlert */
};

G_DEFINE_TYPE (
	ECalendarSelector,
	e_calendar_selector,
	E_TYPE_CLIENT_SELECTOR)

enum {
	PROP_0,
	PROP_SHELL_VIEW,
};

static void
cal_transferring_update_alert (ECalendarSelector *calendar_selector,
                               EShellView *shell_view,
                               const gchar *domain,
                               const gchar *calendar,
                               const gchar *message)
{
	ECalendarSelectorPrivate *priv;
	EShellContent *shell_content;
	EAlert *alert;

	g_return_if_fail (calendar_selector != NULL);
	g_return_if_fail (calendar_selector->priv != NULL);

	priv = calendar_selector->priv;

	if (priv->transfer_alert) {
		e_alert_response (
			priv->transfer_alert,
			e_alert_get_default_response (priv->transfer_alert));
		priv->transfer_alert = NULL;
	}

	if (!message)
		return;

	alert = e_alert_new (domain, calendar, message, NULL);
	g_return_if_fail (alert != NULL);

	priv->transfer_alert = alert;
	g_object_add_weak_pointer (G_OBJECT (alert), &priv->transfer_alert);
	e_alert_start_timer (priv->transfer_alert, 300);

	shell_content = e_shell_view_get_shell_content (shell_view);
	e_alert_sink_submit_alert (E_ALERT_SINK (shell_content), priv->transfer_alert);
	g_object_unref (priv->transfer_alert);
}

typedef struct _TransferItemToData {
	ESource *destination;
	ESourceSelector *selector;
	EClient *src_client;
	EShellView *shell_view;
	EActivity *activity;
	icalcomponent *icalcomp;
	const gchar *display_name;
	gboolean do_copy;
} TransferItemToData;

static void
transfer_item_to_cb (GObject *source_object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	TransferItemToData *titd = user_data;
	GError *error = NULL;
	GCancellable *cancellable;
	gboolean success;

	success = cal_comp_transfer_item_to_finish (E_CAL_CLIENT (source_object), result, &error);

	if (!success) {
		cal_transferring_update_alert (
			E_CALENDAR_SELECTOR (titd->selector),
			titd->shell_view,
			titd->do_copy ? "calendar:failed-copy-event" : "calendar:failed-move-event",
			titd->display_name,
			error->message);
		g_clear_error (&error);
	}

	cancellable = e_activity_get_cancellable (titd->activity);
	e_activity_set_state (
		titd->activity,
		g_cancellable_is_cancelled (cancellable) ? E_ACTIVITY_CANCELLED : E_ACTIVITY_COMPLETED);

	g_object_unref (titd->activity);
	icalcomponent_free (titd->icalcomp);
	g_free (titd);
}

static void
destination_client_connect_cb (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	EClient *client;
	TransferItemToData *titd = user_data;
	GCancellable *cancellable;
	GError *error = NULL;

	client = e_client_selector_get_client_finish (E_CLIENT_SELECTOR (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	cancellable = e_activity_get_cancellable (titd->activity);

	if (error != NULL) {
		cal_transferring_update_alert (
			E_CALENDAR_SELECTOR (titd->selector),
			titd->shell_view,
			titd->do_copy ? "calendar:failed-copy-event" : "calendar:failed-move-event",
			titd->display_name,
			error->message);
		g_clear_error (&error);

		goto exit;
	}

	if (g_cancellable_is_cancelled (cancellable))
		goto exit;

	cal_comp_transfer_item_to (
		E_CAL_CLIENT (titd->src_client), E_CAL_CLIENT (client),
		titd->icalcomp, titd->do_copy, cancellable, transfer_item_to_cb, titd);

	return;

exit:
	e_activity_set_state (
		titd->activity,
		g_cancellable_is_cancelled (cancellable) ? E_ACTIVITY_CANCELLED : E_ACTIVITY_COMPLETED);

	g_object_unref (titd->activity);
	icalcomponent_free (titd->icalcomp);
	g_free (titd);

}

static void
source_client_connect_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	EClient *client;
	TransferItemToData *titd = user_data;
	GCancellable *cancellable;
	GError *error = NULL;

	client = e_client_selector_get_client_finish (E_CLIENT_SELECTOR (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	cancellable = e_activity_get_cancellable (titd->activity);

	if (error != NULL) {
		cal_transferring_update_alert (
			E_CALENDAR_SELECTOR (titd->selector),
			titd->shell_view,
			titd->do_copy ? "calendar:failed-copy-event" : "calendar:failed-move-event",
			titd->display_name,
			error->message);
		g_clear_error (&error);

		goto exit;
	}

	if (g_cancellable_is_cancelled (cancellable))
		goto exit;

	titd->src_client = client;

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (titd->selector), titd->destination, cancellable,
		destination_client_connect_cb, titd);

	return;

exit:
	e_activity_set_state (
		titd->activity,
		g_cancellable_is_cancelled (cancellable) ? E_ACTIVITY_CANCELLED : E_ACTIVITY_COMPLETED);

	g_object_unref (titd->activity);
	icalcomponent_free (titd->icalcomp);
	g_free (titd);
}

static void
calendar_selector_set_shell_view (ECalendarSelector *selector,
                                  EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (selector->priv->shell_view == NULL);

	selector->priv->shell_view = g_object_ref (shell_view);
}

static void
calendar_selector_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			calendar_selector_set_shell_view (
				E_CALENDAR_SELECTOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
calendar_selector_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			g_value_set_object (
				value,
				e_calendar_selector_get_shell_view (
				E_CALENDAR_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
calendar_selector_dispose (GObject *object)
{
	ECalendarSelectorPrivate *priv;

	priv = E_CALENDAR_SELECTOR_GET_PRIVATE (object);

	g_clear_object (&priv->shell_view);

	/* Chain up to the parent' s dispose() method. */
	G_OBJECT_CLASS (e_calendar_selector_parent_class)->dispose (object);
}

static void
calendar_selector_constructed (GObject *object)
{
	ESourceSelector *selector;
	ESourceRegistry *registry;
	ESource *source;

	selector = E_SOURCE_SELECTOR (object);
	registry = e_source_selector_get_registry (selector);
	source = e_source_registry_ref_default_calendar (registry);
	e_source_selector_set_primary_selection (selector, source);
	g_object_unref (source);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_calendar_selector_parent_class)->constructed (object);
}

static gboolean
calendar_selector_data_dropped (ESourceSelector *selector,
                                GtkSelectionData *selection_data,
                                ESource *destination,
                                GdkDragAction action,
                                guint info)
{
	icalcomponent *icalcomp = NULL;
	EActivity *activity;
	EShellBackend *shell_backend;
	EShellView *shell_view;
	ESource *source = NULL;
	ESourceRegistry *registry;
	GCancellable *cancellable;
	gchar **segments;
	gchar *source_uid = NULL;
	gchar *message;
	const gchar *display_name;
	const guchar *data;
	gboolean do_copy;
	TransferItemToData *titd;

	data = gtk_selection_data_get_data (selection_data);
	g_return_val_if_fail (data != NULL, FALSE);

	segments = g_strsplit ((const gchar *) data, "\n", 2);
	if (g_strv_length (segments) != 2)
		goto exit;

	source_uid = g_strdup (segments[0]);
	icalcomp = icalparser_parse_string (segments[1]);

	if (!icalcomp)
		goto exit;

	registry = e_source_selector_get_registry (selector);
	source = e_source_registry_ref_source (registry, source_uid);
	if (!source)
		goto exit;

	shell_view = e_calendar_selector_get_shell_view (E_CALENDAR_SELECTOR (selector));
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	display_name = e_source_get_display_name (destination);

	do_copy = action == GDK_ACTION_COPY ? TRUE : FALSE;

	message = do_copy ?
		g_strdup_printf (_("Copying an event into the calendar %s"), display_name) :
		g_strdup_printf (_("Moving an event into the calendar %s"), display_name);

	cancellable = g_cancellable_new ();
	activity = e_activity_new ();
	e_activity_set_cancellable (activity, cancellable);
	e_activity_set_state (activity, E_ACTIVITY_RUNNING);
	e_activity_set_text (activity, message);
	g_free (message);

	e_shell_backend_add_activity (shell_backend, activity);

	titd = g_new0 (TransferItemToData, 1);

	titd->destination = destination;
	titd->icalcomp = icalcomponent_new_clone (icalcomp);
	titd->selector = selector;
	titd->shell_view = shell_view;
	titd->activity = activity;
	titd->display_name = display_name;
	titd->do_copy = do_copy;

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector), source, cancellable,
		source_client_connect_cb, titd);

exit:
	if (source)
		g_object_unref (source);

	if (icalcomp)
		icalcomponent_free (icalcomp);

	g_free (source_uid);
	g_strfreev (segments);
	return TRUE;
}

static void
e_calendar_selector_class_init (ECalendarSelectorClass *class)
{
	GObjectClass *object_class;
	ESourceSelectorClass *source_selector_class;

	g_type_class_add_private (class, sizeof (ECalendarSelectorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = calendar_selector_constructed;
	object_class->set_property = calendar_selector_set_property;
	object_class->get_property = calendar_selector_get_property;
	object_class->dispose = calendar_selector_dispose;

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->data_dropped = calendar_selector_data_dropped;

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			NULL,
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_calendar_selector_init (ECalendarSelector *selector)
{
	selector->priv = E_CALENDAR_SELECTOR_GET_PRIVATE (selector);

	gtk_drag_dest_set (
		GTK_WIDGET (selector), GTK_DEST_DEFAULT_ALL,
		NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_drag_dest_add_calendar_targets (GTK_WIDGET (selector));
}

GtkWidget *
e_calendar_selector_new (EClientCache *client_cache,
                         EShellView *shell_view)
{
	ESourceRegistry *registry;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	registry = e_client_cache_ref_registry (client_cache);

	widget = g_object_new (
		E_TYPE_CALENDAR_SELECTOR,
		"client-cache", client_cache,
		"extension-name", E_SOURCE_EXTENSION_CALENDAR,
		"registry", registry,
		"shell-view", shell_view,
		NULL);

	g_object_unref (registry);

	return widget;
}

EShellView *
e_calendar_selector_get_shell_view (ECalendarSelector *selector)
{
	g_return_val_if_fail (E_IS_CALENDAR_SELECTOR (selector), NULL);

	return selector->priv->shell_view;
}

