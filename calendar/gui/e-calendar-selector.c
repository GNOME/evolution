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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-calendar-selector.h"

#include <libecal/e-cal-client.h>
#include <libedataserverui/e-client-utils.h>
#include "e-util/e-selection.h"

struct _ECalendarSelectorPrivate {
	gint dummy_value;
};

static gpointer parent_class;

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
client_opened_cb (GObject *source_object,
                  GAsyncResult *result,
                  gpointer user_data)
{
	ESource *source = E_SOURCE (source_object);
	EClient *client = NULL;
	icalcomponent *icalcomp = user_data;
	GError *error = NULL;

	g_return_if_fail (icalcomp != NULL);

	e_client_utils_open_new_finish (source, result, &client, &error);

	if (error != NULL) {
		g_warn_if_fail (client == NULL);
		g_warning (
			"%s: Failed to open client: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_return_if_fail (E_IS_CLIENT (client));

	calendar_selector_update_objects (E_CAL_CLIENT (client), icalcomp);
	g_object_unref (client);

	icalcomponent_free (icalcomp);
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

	e_client_utils_open_new (destination, E_CLIENT_SOURCE_TYPE_EVENTS, FALSE, NULL,
		e_client_utils_authenticate_handler, NULL,
		client_opened_cb, icalcomp);

	success = TRUE;

exit:
	if (path != NULL)
		gtk_tree_path_free (path);

	if (object != NULL)
		g_object_unref (object);

	return success;
}

static void
calendar_selector_class_init (ECalendarSelectorClass *class)
{
	ESourceSelectorClass *source_selector_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalendarSelectorPrivate));

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->data_dropped = calendar_selector_data_dropped;
}

static void
calendar_selector_init (ECalendarSelector *selector)
{
	selector->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		selector, E_TYPE_CALENDAR_SELECTOR, ECalendarSelectorPrivate);

	gtk_drag_dest_set (
		GTK_WIDGET (selector), GTK_DEST_DEFAULT_ALL,
		NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_drag_dest_add_calendar_targets (GTK_WIDGET (selector));
}

GType
e_calendar_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (ECalendarSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) calendar_selector_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ECalendarSelector),
			0,     /* n_preallocs */
			(GInstanceInitFunc) calendar_selector_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SOURCE_SELECTOR, "ECalendarSelector",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_calendar_selector_new (ESourceList *source_list)
{
	g_return_val_if_fail (E_IS_SOURCE_LIST (source_list), NULL);

	return g_object_new (
		E_TYPE_CALENDAR_SELECTOR,
		"source-list", source_list, NULL);
}
