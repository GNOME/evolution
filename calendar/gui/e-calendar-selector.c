/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-calendar-selector.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "e-calendar-selector.h"

#include <libecal/libecal.h>

#define E_CALENDAR_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CALENDAR_SELECTOR, ECalendarSelectorPrivate))

struct _ECalendarSelectorPrivate {
	EShellView *shell_view;
};

G_DEFINE_TYPE (
	ECalendarSelector,
	e_calendar_selector,
	E_TYPE_CLIENT_SELECTOR)

enum {
	PROP_0,
	PROP_SHELL_VIEW,
};

static gboolean
calendar_selector_update_single_object (ECalClient *client,
                                        icalcomponent *icalcomp)
{
	gchar *uid;
	icalcomponent *tmp_icalcomp;

	uid = (gchar *) icalcomponent_get_uid (icalcomp);

	if (e_cal_client_get_object_sync (client, uid, NULL, &tmp_icalcomp, NULL, NULL)) {
		icalcomponent_free (tmp_icalcomp);

		return e_cal_client_modify_object_sync (
			client, icalcomp, CALOBJ_MOD_ALL, NULL, NULL);
	}

	uid = NULL;
	if (!e_cal_client_create_object_sync (client, icalcomp, &uid, NULL, NULL))
		return FALSE;

	if (uid)
		icalcomponent_set_uid (icalcomp, uid);
	g_free (uid);

	return TRUE;
}

static gboolean
calendar_selector_update_objects (ECalClient *client,
                                  icalcomponent *icalcomp)
{
	icalcomponent *subcomp;
	icalcomponent_kind kind;

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VTODO_COMPONENT || kind == ICAL_VEVENT_COMPONENT)
		return calendar_selector_update_single_object (
			client, icalcomp);
	else if (kind != ICAL_VCALENDAR_COMPONENT)
		return FALSE;

	subcomp = icalcomponent_get_first_component (
		icalcomp, ICAL_ANY_COMPONENT);
	while (subcomp != NULL) {
		gboolean success;

		kind = icalcomponent_isa (subcomp);
		if (kind == ICAL_VTIMEZONE_COMPONENT) {
			icaltimezone *zone;
			GError *error = NULL;

			zone = icaltimezone_new ();
			icaltimezone_set_component (zone, subcomp);

			e_cal_client_add_timezone_sync (client, zone, NULL, &error);
			icaltimezone_free (zone, 1);

			if (error != NULL) {
				g_warning (
					"%s: Failed to add timezone: %s",
					G_STRFUNC, error->message);
				g_error_free (error);
				return FALSE;
			}
		} else if (kind == ICAL_VTODO_COMPONENT ||
			kind == ICAL_VEVENT_COMPONENT) {
			success = calendar_selector_update_single_object (
				client, subcomp);
			if (!success)
				return FALSE;
		}

		subcomp = icalcomponent_get_next_component (
			icalcomp, ICAL_ANY_COMPONENT);
	}

	return TRUE;
}

static void
client_connect_cb (GObject *source_object,
                   GAsyncResult *result,
                   gpointer user_data)
{
	EClient *client;
	icalcomponent *icalcomp = user_data;
	GError *error = NULL;

	g_return_if_fail (icalcomp != NULL);

	client = e_client_selector_get_client_finish (
		E_CLIENT_SELECTOR (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	calendar_selector_update_objects (E_CAL_CLIENT (client), icalcomp);
	g_object_unref (client);

	icalcomponent_free (icalcomp);
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
	G_OBJECT_CLASS (e_calendar_selector_parent_class)->
		constructed (object);
}

static gboolean
calendar_selector_data_dropped (ESourceSelector *selector,
                                GtkSelectionData *selection_data,
                                ESource *destination,
                                GdkDragAction action,
                                guint info)
{
	GtkTreePath *path = NULL;
	icalcomponent *icalcomp;
	const guchar *data;
	gboolean success = FALSE;
	gpointer object = NULL;

	data = gtk_selection_data_get_data (selection_data);
	icalcomp = icalparser_parse_string ((const gchar *) data);

	if (icalcomp == NULL)
		goto exit;

	/* FIXME Deal with GDK_ACTION_ASK. */
	if (action == GDK_ACTION_COPY) {
		gchar *uid;

		uid = e_cal_component_gen_uid ();
		icalcomponent_set_uid (icalcomp, uid);
	}

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector), destination, NULL,
		client_connect_cb, icalcomp);

	success = TRUE;

exit:
	if (path != NULL)
		gtk_tree_path_free (path);

	if (object != NULL)
		g_object_unref (object);

	return success;
}

EShellView *
e_calendar_selector_get_shell_view (ECalendarSelector *calendar_selector)
{
	g_return_val_if_fail (E_IS_CALENDAR_SELECTOR (calendar_selector), NULL);

	return calendar_selector->priv->shell_view;
}

static void
e_calendar_selector_set_shell_view (ECalendarSelector *calendar_selector,
				    EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (calendar_selector->priv->shell_view == NULL);

	calendar_selector->priv->shell_view = g_object_ref (shell_view);
}

static void
calendar_selector_set_property (GObject *object,
				guint property_id,
				const GValue *value,
				GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			e_calendar_selector_set_shell_view (
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
				e_calendar_selector_get_shell_view (E_CALENDAR_SELECTOR (object)));
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
