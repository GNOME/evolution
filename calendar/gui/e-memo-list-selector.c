/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-list-selector.c
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

#include "e-memo-list-selector.h"

#include <string.h>
#include <libecal/e-cal.h>
#include "e-util/e-selection.h"
#include "calendar/common/authentication.h"
#include "calendar/gui/comp-util.h"

#define E_MEMO_LIST_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_LIST_SELECTOR, EMemoListSelectorPrivate))

struct _EMemoListSelectorPrivate {
	gint dummy_value;
};

static gpointer parent_class;

static gboolean
memo_list_selector_update_single_object (ECal *client,
                                         icalcomponent *icalcomp)
{
	gchar *uid;
	icalcomponent *tmp_icalcomp;

	uid = (gchar *) icalcomponent_get_uid (icalcomp);

	if (e_cal_get_object (client, uid, NULL, &tmp_icalcomp, NULL))
		return e_cal_modify_object (
			client, icalcomp, CALOBJ_MOD_ALL, NULL);

	return e_cal_create_object (client, icalcomp, &uid, NULL);
}

static gboolean
memo_list_selector_update_objects (ECal *client,
                                   icalcomponent *icalcomp)
{
	icalcomponent *subcomp;
	icalcomponent_kind kind;

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VJOURNAL_COMPONENT)
		return memo_list_selector_update_single_object (
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

			zone = icaltimezone_new ();
			icaltimezone_set_component (zone, subcomp);

			success = e_cal_add_timezone (client, zone, NULL);
			icaltimezone_free (zone, 1);
			if (!success)
				return FALSE;
		} else if (kind == ICAL_VJOURNAL_COMPONENT) {
			success = memo_list_selector_update_single_object (
				client, subcomp);
			if (!success)
				return FALSE;
		}

		subcomp = icalcomponent_get_next_component (
			icalcomp, ICAL_ANY_COMPONENT);
	}

	return TRUE;
}

static gboolean
memo_list_selector_process_data (ESourceSelector *selector,
                                 ECal *client,
                                 const gchar *source_uid,
                                 icalcomponent *icalcomp,
                                 GdkDragAction action)
{
	ESourceList *source_list;
	ESource *source;
	icalcomponent *tmp_icalcomp = NULL;
	const gchar *uid;
	gchar *old_uid = NULL;
	gboolean success = FALSE;
	gboolean read_only = TRUE;
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

	if (e_cal_get_object (client, uid, NULL, &tmp_icalcomp, &error)) {
		icalcomponent_free (tmp_icalcomp);
		success = TRUE;
		goto exit;
	}

	if (error != NULL && error->code != E_CALENDAR_STATUS_OBJECT_NOT_FOUND) {
		g_message (
			"Failed to search the object in destination "
			"task list: %s", error->message);
		g_error_free (error);
		goto exit;
	}

	success = memo_list_selector_update_objects (client, icalcomp);

	if (!success || action != GDK_ACTION_MOVE)
		goto exit;

	source_list = e_source_selector_get_source_list (selector);
	source = e_source_list_peek_source_by_uid (source_list, source_uid);

	if (!E_IS_SOURCE (source) || e_source_get_readonly (source))
		goto exit;

	client = e_auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
	if (client == NULL) {
		g_message ("Cannot create source client to remove old memo");
		goto exit;
	}

	e_cal_is_read_only (client, &read_only, NULL);
	if (!read_only && e_cal_open (client, TRUE, NULL))
		e_cal_remove_object (client, old_uid, NULL);
	else if (!read_only)
		g_message ("Cannot open source client to remove old memo");

	g_object_unref (client);

exit:
	g_free (old_uid);

	return success;
}

static gboolean
memo_list_selector_data_dropped (ESourceSelector *selector,
                                 GtkSelectionData *selection_data,
                                 ESource *destination,
                                 GdkDragAction action,
                                 guint info)
{
	ECal *client;
	GSList *list, *iter;
	gboolean success = FALSE;

	client = e_auth_new_cal_from_source (
		destination, E_CAL_SOURCE_TYPE_JOURNAL);

	if (client == NULL || !e_cal_open (client, TRUE, NULL))
		goto exit;

	list = cal_comp_selection_get_string_list (selection_data);

	for (iter = list; iter != NULL; iter = iter->next) {
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

		success = memo_list_selector_process_data (
			selector, client, source_uid, icalcomp, action);

		icalcomponent_free (icalcomp);
	}

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

exit:
	if (client != NULL)
		g_object_unref (client);

	return success;
}

static void
memo_list_selector_class_init (EMemoListSelectorClass *class)
{
	ESourceSelectorClass *source_selector_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoListSelectorPrivate));

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->data_dropped = memo_list_selector_data_dropped;
}

static void
memo_list_selector_init (EMemoListSelector *selector)
{
	selector->priv = E_MEMO_LIST_SELECTOR_GET_PRIVATE (selector);

	gtk_drag_dest_set (
		GTK_WIDGET (selector), GTK_DEST_DEFAULT_ALL,
		NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_drag_dest_add_calendar_targets (GTK_WIDGET (selector));
}

GType
e_memo_list_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EMemoListSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) memo_list_selector_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMemoListSelector),
			0,     /* n_preallocs */
			(GInstanceInitFunc) memo_list_selector_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SOURCE_SELECTOR, "EMemoListSelector",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_memo_list_selector_new (ESourceList *source_list)
{
	g_return_val_if_fail (E_IS_SOURCE_LIST (source_list), NULL);

	return g_object_new (
		E_TYPE_MEMO_LIST_SELECTOR,
		"source-list", source_list, NULL);
}
