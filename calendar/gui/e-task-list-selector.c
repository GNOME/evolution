/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-list-selector.c
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

#include "e-task-list-selector.h"

#include <string.h>
#include <libecal/libecal.h>

#include "calendar/gui/comp-util.h"

#define E_TASK_LIST_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TASK_LIST_SELECTOR, ETaskListSelectorPrivate))

struct _ETaskListSelectorPrivate {
	gint dummy_value;
};

G_DEFINE_TYPE (
	ETaskListSelector,
	e_task_list_selector,
	E_TYPE_SOURCE_SELECTOR)

static gboolean
task_list_selector_update_single_object (ECalClient *client,
                                         icalcomponent *icalcomp)
{
	gchar *uid = NULL;
	icalcomponent *tmp_icalcomp;

	uid = (gchar *) icalcomponent_get_uid (icalcomp);

	if (e_cal_client_get_object_sync (client, uid, NULL, &tmp_icalcomp, NULL, NULL)) {
		icalcomponent_free (tmp_icalcomp);

		return e_cal_client_modify_object_sync (
			client, icalcomp, CALOBJ_MOD_ALL, NULL, NULL);
	}

	if (!e_cal_client_create_object_sync (client, icalcomp, &uid, NULL, NULL))
		return FALSE;

	if (uid)
		icalcomponent_set_uid (icalcomp, uid);

	g_free (uid);

	return TRUE;
}

static gboolean
task_list_selector_update_objects (ECalClient *client,
                                   icalcomponent *icalcomp)
{
	icalcomponent *subcomp;
	icalcomponent_kind kind;

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VTODO_COMPONENT || kind == ICAL_VEVENT_COMPONENT)
		return task_list_selector_update_single_object (
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
			success = task_list_selector_update_single_object (
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
	gchar *uid = user_data;
	GError *error = NULL;

	g_return_if_fail (uid != NULL);

	client = e_cal_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	if (!e_client_is_readonly (client))
		e_cal_client_remove_object_sync (
			E_CAL_CLIENT (client), uid, NULL,
			CALOBJ_MOD_THIS, NULL, NULL);

	g_object_unref (client);

exit:
	g_free (uid);
}

static gboolean
task_list_selector_process_data (ESourceSelector *selector,
                                 ECalClient *client,
                                 const gchar *source_uid,
                                 icalcomponent *icalcomp,
                                 GdkDragAction action)
{
	ESource *source;
	ESourceRegistry *registry;
	icalcomponent *tmp_icalcomp = NULL;
	const gchar *uid;
	gchar *old_uid = NULL;
	gboolean success = FALSE;
	GError *error = NULL;

	/* FIXME Deal with GDK_ACTION_ASK. */
	if (action == GDK_ACTION_COPY) {
		old_uid = g_strdup (icalcomponent_get_uid (icalcomp));
		uid = e_cal_component_gen_uid ();
		icalcomponent_set_uid (icalcomp, uid);
	}

	uid = icalcomponent_get_uid (icalcomp);
	if (old_uid == NULL)
		old_uid = g_strdup (uid);

	if (e_cal_client_get_object_sync (client, uid, NULL, &tmp_icalcomp, NULL, &error)) {
		icalcomponent_free (tmp_icalcomp);
		success = TRUE;
		goto exit;
	}

	if (error != NULL && !g_error_matches (error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND)) {
		g_message (
			"Failed to search the object in destination "
			"task list: %s", error->message);
		g_error_free (error);
		goto exit;
	}

	if (error)
		g_error_free (error);
	error = NULL;

	success = task_list_selector_update_objects (client, icalcomp);

	if (!success || action != GDK_ACTION_MOVE)
		goto exit;

	registry = e_source_selector_get_registry (selector);
	source = e_source_registry_ref_source (registry, source_uid);

	if (source != NULL) {
		e_cal_client_connect (
			source, E_CAL_CLIENT_SOURCE_TYPE_MEMOS, NULL,
			client_connect_cb, g_strdup (old_uid));
		g_object_unref (source);
	}

exit:
	g_free (old_uid);

	return success;
}

struct DropData
{
	ESourceSelector *selector;
	GdkDragAction action;
	GSList *list;
};

static void
client_connect_for_drop_cb (GObject *source_object,
                            GAsyncResult *result,
                            gpointer user_data)
{
	struct DropData *dd = user_data;
	EClient *client;
	ECalClient *cal_client;
	GSList *iter;
	GError *error = NULL;

	g_return_if_fail (dd != NULL);

	client = e_cal_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	cal_client = E_CAL_CLIENT (client);

	for (iter = dd->list; iter != NULL; iter = iter->next) {
		gchar *source_uid = iter->data;
		icalcomponent *icalcomp;
		gchar *component_string;

		/* Each string is "source_uid\ncomponent_string". */
		component_string = strchr (source_uid, '\n');
		if (component_string == NULL)
			continue;

		*component_string++ = '\0';
		icalcomp = icalparser_parse_string (component_string);
		if (icalcomp == NULL)
			continue;

		task_list_selector_process_data (
			dd->selector, cal_client, source_uid,
			icalcomp, dd->action);

		icalcomponent_free (icalcomp);
	}

	g_object_unref (client);

exit:
	g_slist_foreach (dd->list, (GFunc) g_free, NULL);
	g_slist_free (dd->list);
	g_object_unref (dd->selector);
	g_free (dd);
}

static void
task_list_selector_constructed (GObject *object)
{
	ESourceSelector *selector;
	ESourceRegistry *registry;
	ESource *source;

	selector = E_SOURCE_SELECTOR (object);
	registry = e_source_selector_get_registry (selector);
	source = e_source_registry_ref_default_task_list (registry);
	e_source_selector_set_primary_selection (selector, source);
	g_object_unref (source);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_task_list_selector_parent_class)->
		constructed (object);
}

static gboolean
task_list_selector_data_dropped (ESourceSelector *selector,
                                 GtkSelectionData *selection_data,
                                 ESource *destination,
                                 GdkDragAction action,
                                 guint info)
{
	struct DropData *dd;

	dd = g_new0 (struct DropData, 1);
	dd->selector = g_object_ref (selector);
	dd->action = action;
	dd->list = cal_comp_selection_get_string_list (selection_data);

	e_cal_client_connect (
		destination, E_CAL_CLIENT_SOURCE_TYPE_TASKS, NULL,
		client_connect_for_drop_cb, dd);

	return TRUE;
}

static void
e_task_list_selector_class_init (ETaskListSelectorClass *class)
{
	GObjectClass *object_class;
	ESourceSelectorClass *source_selector_class;

	g_type_class_add_private (class, sizeof (ETaskListSelectorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = task_list_selector_constructed;

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->data_dropped = task_list_selector_data_dropped;
}

static void
e_task_list_selector_init (ETaskListSelector *selector)
{
	selector->priv = E_TASK_LIST_SELECTOR_GET_PRIVATE (selector);

	gtk_drag_dest_set (
		GTK_WIDGET (selector), GTK_DEST_DEFAULT_ALL,
		NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_drag_dest_add_calendar_targets (GTK_WIDGET (selector));
}

GtkWidget *
e_task_list_selector_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_TASK_LIST_SELECTOR,
		"extension-name", E_SOURCE_EXTENSION_TASK_LIST,
		"registry", registry, NULL);
}
