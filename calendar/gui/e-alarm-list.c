/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* EAlarmList - list of calendar alarms with GtkTreeModel interface.
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:  Hans Petter Jansson  <hpj@ximian.com>
 */

#include <config.h>
#include <string.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktreednd.h>
#include <libgnome/gnome-i18n.h>
#include <glib.h>
#include <libecal/e-cal-time-util.h>
#include <e-util/e-time-utils.h>
#include "calendar-config.h"
#include "e-alarm-list.h"

#define G_LIST(x)                    ((GList *) x)
#define E_ALARM_LIST_IS_SORTED(list) (E_ALARM_LIST (list)->sort_column_id != -2)
#define IS_VALID_ITER(dt_list, iter) (iter!= NULL && iter->user_data != NULL && \
                                      dt_list->stamp == iter->stamp)

static GType column_types [E_ALARM_LIST_NUM_COLUMNS];

static void         e_alarm_list_init            (EAlarmList         *file_list);
static void         e_alarm_list_class_init      (EAlarmListClass    *class);
static void         e_alarm_list_tree_model_init (GtkTreeModelIface  *iface);
static void         e_alarm_list_finalize        (GObject            *object);
static guint        e_alarm_list_get_flags       (GtkTreeModel       *tree_model);
static gint         e_alarm_list_get_n_columns   (GtkTreeModel       *tree_model);
static GType        e_alarm_list_get_column_type (GtkTreeModel       *tree_model,
						  gint                index);
static gboolean     e_alarm_list_get_iter        (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter,
						  GtkTreePath        *path);
static GtkTreePath *e_alarm_list_get_path        (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter);
static void         e_alarm_list_get_value       (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter,
						  gint                column,
						  GValue             *value);
static gboolean     e_alarm_list_iter_next       (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter);
static gboolean     e_alarm_list_iter_children   (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter,
						  GtkTreeIter        *parent);
static gboolean     e_alarm_list_iter_has_child  (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter);
static gint         e_alarm_list_iter_n_children (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter);
static gboolean     e_alarm_list_iter_nth_child  (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter,
						  GtkTreeIter        *parent,
						  gint                n);
static gboolean     e_alarm_list_iter_parent     (GtkTreeModel       *tree_model,
						  GtkTreeIter        *iter,
						  GtkTreeIter        *child);

static GObjectClass *parent_class = NULL;

GtkType
e_alarm_list_get_type (void)
{
	static GType alarm_list_type = 0;

	if (!alarm_list_type) {
		static const GTypeInfo alarm_list_info =
		{
			sizeof (EAlarmListClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) e_alarm_list_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (EAlarmList),
			0,
			(GInstanceInitFunc) e_alarm_list_init,
		};

		static const GInterfaceInfo tree_model_info =
		{
			(GInterfaceInitFunc) e_alarm_list_tree_model_init,
			NULL,
			NULL
		};

		column_types [E_ALARM_LIST_COLUMN_DESCRIPTION] = G_TYPE_STRING;

		alarm_list_type = g_type_register_static (G_TYPE_OBJECT, "EAlarmList",
							  &alarm_list_info, 0);
		g_type_add_interface_static (alarm_list_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return alarm_list_type;
}

static void
e_alarm_list_class_init (EAlarmListClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	object_class = (GObjectClass *) class;

	object_class->finalize = e_alarm_list_finalize;
}

static void
e_alarm_list_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = e_alarm_list_get_flags;
	iface->get_n_columns = e_alarm_list_get_n_columns;
	iface->get_column_type = e_alarm_list_get_column_type;
	iface->get_iter = e_alarm_list_get_iter;
	iface->get_path = e_alarm_list_get_path;
	iface->get_value = e_alarm_list_get_value;
	iface->iter_next = e_alarm_list_iter_next;
	iface->iter_children = e_alarm_list_iter_children;
	iface->iter_has_child = e_alarm_list_iter_has_child;
	iface->iter_n_children = e_alarm_list_iter_n_children;
	iface->iter_nth_child = e_alarm_list_iter_nth_child;
	iface->iter_parent = e_alarm_list_iter_parent;
}

static void
e_alarm_list_init (EAlarmList *alarm_list)
{
	alarm_list->stamp         = g_random_int ();
	alarm_list->columns_dirty = FALSE;
	alarm_list->list          = NULL;
}

EAlarmList *
e_alarm_list_new (void)
{
	EAlarmList *alarm_list;

	alarm_list = E_ALARM_LIST (g_object_new (e_alarm_list_get_type (), NULL));

	return alarm_list;
}

static void
all_rows_deleted (EAlarmList *alarm_list)
{
	GtkTreePath *path;
	gint         i;

	if (!alarm_list->list)
		return;

	path = gtk_tree_path_new ();
	i = g_list_length (alarm_list->list);
	gtk_tree_path_append_index (path, i);

	for ( ; i >= 0; i--) {
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (alarm_list), path);
		gtk_tree_path_prev (path);
	}

	gtk_tree_path_free (path);
}

static void
row_deleted (EAlarmList *alarm_list, gint n)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (alarm_list), path);
	gtk_tree_path_free (path);
}

static void
row_added (EAlarmList *alarm_list, gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (alarm_list), &iter, path))
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (alarm_list), path, &iter);

	gtk_tree_path_free (path);
}

static void
row_updated (EAlarmList *alarm_list, gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (alarm_list), &iter, path))
		gtk_tree_model_row_changed (GTK_TREE_MODEL (alarm_list), path, &iter);

	gtk_tree_path_free (path);
}

static void
e_alarm_list_finalize (GObject *object)
{
	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Fulfill the GtkTreeModel requirements */
static guint
e_alarm_list_get_flags (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), 0);

	return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
e_alarm_list_get_n_columns (GtkTreeModel *tree_model)
{
	EAlarmList *alarm_list = (EAlarmList *) tree_model;

	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), 0);

	alarm_list->columns_dirty = TRUE;
	return E_ALARM_LIST_NUM_COLUMNS;
}

static GType
e_alarm_list_get_column_type (GtkTreeModel *tree_model,
			      gint          index)
{
	EAlarmList *alarm_list = (EAlarmList *) tree_model;

	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail (index < E_ALARM_LIST_NUM_COLUMNS &&
			      index >= 0, G_TYPE_INVALID);

	alarm_list->columns_dirty = TRUE;
	return column_types [index];
}

const ECalComponentAlarm *
e_alarm_list_get_alarm (EAlarmList *alarm_list, GtkTreeIter *iter)
{
	g_return_val_if_fail (IS_VALID_ITER (alarm_list, iter), NULL);

	return G_LIST (iter->user_data)->data;
}

static void
free_alarm (ECalComponentAlarm *alarm)
{
	e_cal_component_alarm_free (alarm);
}

static ECalComponentAlarm *
copy_alarm (const ECalComponentAlarm *alarm)
{
	return e_cal_component_alarm_clone ((ECalComponentAlarm *) alarm);
}

void
e_alarm_list_set_alarm (EAlarmList *alarm_list, GtkTreeIter *iter,
			const ECalComponentAlarm *alarm)
{
	ECalComponentAlarm *alarm_old;

	g_return_if_fail (IS_VALID_ITER (alarm_list, iter));

	alarm_old = G_LIST (iter->user_data)->data;
	free_alarm (alarm_old);
	G_LIST (iter->user_data)->data = copy_alarm (alarm);
	row_updated (alarm_list, g_list_position (alarm_list->list, G_LIST (iter->user_data)));
}

void
e_alarm_list_append (EAlarmList *alarm_list, GtkTreeIter *iter,
		     const ECalComponentAlarm *alarm)
{
	g_return_if_fail (alarm != NULL);

	alarm_list->list = g_list_append (alarm_list->list, copy_alarm (alarm));
	row_added (alarm_list, g_list_length (alarm_list->list) - 1);

	if (iter) {
		iter->user_data = g_list_last (alarm_list->list);
		iter->stamp     = alarm_list->stamp;
	}
}

void
e_alarm_list_remove (EAlarmList *alarm_list, GtkTreeIter *iter)
{
	gint n;

	g_return_if_fail (IS_VALID_ITER (alarm_list, iter));

	n = g_list_position (alarm_list->list, G_LIST (iter->user_data));
	free_alarm ((ECalComponentAlarm *) G_LIST (iter->user_data)->data);
	alarm_list->list = g_list_delete_link (alarm_list->list, G_LIST (iter->user_data));
	row_deleted (alarm_list, n);
}

void
e_alarm_list_clear (EAlarmList *alarm_list)
{
	GList *l;

	all_rows_deleted (alarm_list);

	for (l = alarm_list->list; l; l = g_list_next (l)) {
		free_alarm ((ECalComponentAlarm *) l->data);
	}

	g_list_free (alarm_list->list);
	alarm_list->list = NULL;
}

static gboolean
e_alarm_list_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
	EAlarmList *alarm_list = (EAlarmList *) tree_model;
	GList      *l;
	gint        i;

	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	if (!alarm_list->list)
		return FALSE;

	alarm_list->columns_dirty = TRUE;

	i = gtk_tree_path_get_indices (path)[0];
	l = g_list_nth (alarm_list->list, i);
	if (!l)
		return FALSE;

	iter->user_data = l;
	iter->stamp     = alarm_list->stamp;
	return TRUE;
}

static GtkTreePath *
e_alarm_list_get_path (GtkTreeModel *tree_model,
		       GtkTreeIter  *iter)
{
	EAlarmList  *alarm_list = (EAlarmList *) tree_model;
	GtkTreePath *retval;
	GList       *l;

	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), NULL);
	g_return_val_if_fail (iter->stamp == E_ALARM_LIST (tree_model)->stamp, NULL);

	l = iter->user_data;
	retval = gtk_tree_path_new ();
	gtk_tree_path_append_index (retval, g_list_position (alarm_list->list, l));
	return retval;
}

/* Builds a string for the duration of the alarm.  If the duration is zero, returns NULL. */
static char *
get_alarm_duration_string (struct icaldurationtype *duration)
{
	GString *string = g_string_new (NULL);
	char *ret;
	gboolean have_something;

	have_something = FALSE;

	if (duration->days >= 1) {
		g_string_sprintf (string, ngettext("%d day", "%d days", duration->days), duration->days);
		have_something = TRUE;
	}

	if (duration->weeks >= 1) {
		g_string_sprintf (string, ngettext("%d week","%d weeks", duration->weeks), duration->weeks);
		have_something = TRUE;
	}

	if (duration->hours >= 1) {
		g_string_sprintf (string, ngettext("%d hour", "%d hours", duration->hours), duration->hours);
		have_something = TRUE;
	}

	if (duration->minutes >= 1) {
		g_string_sprintf (string, ngettext("%d minute", "%d minutes", duration->minutes), duration->minutes);
		have_something = TRUE;
	}

	if (duration->seconds >= 1) {
		g_string_sprintf (string, ngettext("%d second", "%d seconds", duration->seconds), duration->seconds);
		have_something = TRUE;
	}

	if (have_something) {
		ret = string->str;
		g_string_free (string, FALSE);
		return ret;
	} else {
		g_string_free (string, TRUE);
		return NULL;
	}
}

static char *
get_alarm_string (ECalComponentAlarm *alarm)
{
	ECalComponentAlarmAction action;
	ECalComponentAlarmTrigger trigger;
	char string[256];
	char *base, *str = NULL, *dur;

	string [0] = '\0';

	e_cal_component_alarm_get_action (alarm, &action);
	e_cal_component_alarm_get_trigger (alarm, &trigger);

	switch (action) {
	case E_CAL_COMPONENT_ALARM_AUDIO:
		base = _("Play a sound");
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		base = _("Pop up an alert");
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		base = _("Send an email");
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		base = _("Run a program");
		break;

	case E_CAL_COMPONENT_ALARM_NONE:
	case E_CAL_COMPONENT_ALARM_UNKNOWN:
	default:
		base = _("Unknown action to be performed");
		break;
	}

	/* FIXME: This does not look like it will localize correctly. */

	switch (trigger.type) {
	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START:
		dur = get_alarm_duration_string (&trigger.u.rel_duration);

		if (dur) {
			if (trigger.u.rel_duration.is_neg)
				str = g_strdup_printf (_("%s %s before the start of the appointment"),
						       base, dur);
			else
				str = g_strdup_printf (_("%s %s after the start of the appointment"),
						       base, dur);

			g_free (dur);
		} else
			str = g_strdup_printf (_("%s at the start of the appointment"), base);

		break;

	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END:
		dur = get_alarm_duration_string (&trigger.u.rel_duration);

		if (dur) {
			if (trigger.u.rel_duration.is_neg)
				str = g_strdup_printf (_("%s %s before the end of the appointment"),
						       base, dur);
			else
				str = g_strdup_printf (_("%s %s after the end of the appointment"),
						       base, dur);

			g_free (dur);
		} else
			str = g_strdup_printf (_("%s at the end of the appointment"), base);

		break;

	case E_CAL_COMPONENT_ALARM_TRIGGER_ABSOLUTE: {
		struct icaltimetype itt;
		icaltimezone *utc_zone, *current_zone;
		struct tm tm;
		char buf[256];

		/* Absolute triggers come in UTC, so convert them to the local timezone */

		itt = trigger.u.abs_time;

		utc_zone = icaltimezone_get_utc_timezone ();
		current_zone = calendar_config_get_icaltimezone ();

		tm = icaltimetype_to_tm_with_zone (&itt, utc_zone, current_zone);

		e_time_format_date_and_time (&tm, calendar_config_get_24_hour_format (),
					     FALSE, FALSE, buf, sizeof (buf));

		str = g_strdup_printf (_("%s at %s"), base, buf);

		break; }

	case E_CAL_COMPONENT_ALARM_TRIGGER_NONE:
	default:
		str = g_strdup_printf (_("%s for an unknown trigger type"), base);
		break;
	}

	return str;
}

static void
e_alarm_list_get_value (GtkTreeModel *tree_model,
			GtkTreeIter  *iter,
			gint          column,
			GValue       *value)
{
	EAlarmList        *alarm_list = E_ALARM_LIST (tree_model);
	ECalComponentAlarm *alarm;
	GList             *l;
	const gchar       *str;

	g_return_if_fail (E_IS_ALARM_LIST (tree_model));
	g_return_if_fail (column < E_ALARM_LIST_NUM_COLUMNS);
	g_return_if_fail (E_ALARM_LIST (tree_model)->stamp == iter->stamp);
	g_return_if_fail (IS_VALID_ITER (alarm_list, iter));

	g_value_init (value, column_types [column]);

	if (!alarm_list->list)
		return;

	l        = iter->user_data;
	alarm = l->data;

	if (!alarm)
		return;

	switch (column) {
		case E_ALARM_LIST_COLUMN_DESCRIPTION:
			str = get_alarm_string (alarm);
			g_value_set_string (value, str);
			break;
	}
}

static gboolean
e_alarm_list_iter_next (GtkTreeModel  *tree_model,
			GtkTreeIter   *iter)
{
	GList *l;

	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), FALSE);
	g_return_val_if_fail (IS_VALID_ITER (E_ALARM_LIST (tree_model), iter), FALSE);

	if (!E_ALARM_LIST (tree_model)->list)
		return FALSE;

	l = iter->user_data;
	l = g_list_next (l);
	if (l) {
		iter->user_data = l;
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_alarm_list_iter_children (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *parent)
{
	EAlarmList *alarm_list = E_ALARM_LIST (tree_model);

	/* this is a list, nodes have no children */
	if (parent)
		return FALSE;

	/* but if parent == NULL we return the list itself as children of the
	 * "root" */

	if (!alarm_list->list)
		return FALSE;

	iter->stamp     = E_ALARM_LIST (tree_model)->stamp;
	iter->user_data = alarm_list->list;
	return TRUE;
}

static gboolean
e_alarm_list_iter_has_child (GtkTreeModel *tree_model,
			     GtkTreeIter  *iter)
{
	g_return_val_if_fail (IS_VALID_ITER (E_ALARM_LIST (tree_model), iter), FALSE);
	return FALSE;
}

static gint
e_alarm_list_iter_n_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter)
{
	EAlarmList *alarm_list = E_ALARM_LIST (tree_model);

	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), -1);

	if (iter == NULL)
		return g_list_length (alarm_list->list);

	g_return_val_if_fail (E_ALARM_LIST (tree_model)->stamp == iter->stamp, -1);
	return 0;
}

static gboolean
e_alarm_list_iter_nth_child (GtkTreeModel *tree_model,
			     GtkTreeIter  *iter,
			     GtkTreeIter  *parent,
			     gint          n)
{
	EAlarmList *alarm_list = E_ALARM_LIST (tree_model);

	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), FALSE);

	if (parent)
		return FALSE;

	if (alarm_list->list) {
		GList *l;

		l = g_list_nth (alarm_list->list, n);
		if (!l)
			return FALSE;

		iter->stamp     = alarm_list->stamp;
		iter->user_data = l;
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_alarm_list_iter_parent (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  GtkTreeIter  *child)
{
	return FALSE;
}
