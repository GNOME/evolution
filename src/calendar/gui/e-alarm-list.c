/*
 * EAlarmList - list of calendar alarms with GtkTreeModel interface.
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
 *
 * Authors:
 *		Hans Petter Jansson  <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include "calendar-config.h"
#include "e-alarm-list.h"

#define G_LIST(x)                    ((GList *) x)
#define E_ALARM_LIST_IS_SORTED(list) (E_ALARM_LIST (list)->sort_column_id != -2)
#define IS_VALID_ITER(dt_list, iter) (iter!= NULL && iter->user_data != NULL && \
                                      dt_list->stamp == iter->stamp)

static GType column_types[E_ALARM_LIST_NUM_COLUMNS];

static void         e_alarm_list_tree_model_init (GtkTreeModelIface  *iface);
static GtkTreeModelFlags e_alarm_list_get_flags       (GtkTreeModel       *tree_model);
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

G_DEFINE_TYPE_WITH_CODE (
	EAlarmList,
	e_alarm_list,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		GTK_TYPE_TREE_MODEL,
		e_alarm_list_tree_model_init))

static void
alarm_list_dispose (GObject *object)
{
	e_alarm_list_clear (E_ALARM_LIST (object));

	G_OBJECT_CLASS (e_alarm_list_parent_class)->dispose (object);
}

static void
e_alarm_list_class_init (EAlarmListClass *class)
{
	GObjectClass *object_class;

	column_types[E_ALARM_LIST_COLUMN_DESCRIPTION] = G_TYPE_STRING;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = alarm_list_dispose;
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
	alarm_list->stamp = g_random_int ();
	alarm_list->columns_dirty = FALSE;
	alarm_list->list = NULL;
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

	for (; i >= 0; i--) {
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (alarm_list), path);
		gtk_tree_path_prev (path);
	}

	gtk_tree_path_free (path);
}

static void
row_deleted (EAlarmList *alarm_list,
             gint n)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (alarm_list), path);
	gtk_tree_path_free (path);
}

static void
row_added (EAlarmList *alarm_list,
           gint n)
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
row_updated (EAlarmList *alarm_list,
             gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (alarm_list), &iter, path))
		gtk_tree_model_row_changed (GTK_TREE_MODEL (alarm_list), path, &iter);

	gtk_tree_path_free (path);
}

/* Fulfill the GtkTreeModel requirements */
static GtkTreeModelFlags
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
                              gint index)
{
	EAlarmList *alarm_list = (EAlarmList *) tree_model;

	g_return_val_if_fail (E_IS_ALARM_LIST (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail (index < E_ALARM_LIST_NUM_COLUMNS &&
			      index >= 0, G_TYPE_INVALID);

	alarm_list->columns_dirty = TRUE;
	return column_types[index];
}

const ECalComponentAlarm *
e_alarm_list_get_alarm (EAlarmList *alarm_list,
                        GtkTreeIter *iter)
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
	return e_cal_component_alarm_copy ((ECalComponentAlarm *) alarm);
}

void
e_alarm_list_set_alarm (EAlarmList *alarm_list,
                        GtkTreeIter *iter,
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
e_alarm_list_append (EAlarmList *alarm_list,
                     GtkTreeIter *iter,
                     const ECalComponentAlarm *alarm)
{
	g_return_if_fail (alarm != NULL);

	alarm_list->list = g_list_append (alarm_list->list, copy_alarm (alarm));
	row_added (alarm_list, g_list_length (alarm_list->list) - 1);

	if (iter) {
		iter->user_data = g_list_last (alarm_list->list);
		iter->stamp = alarm_list->stamp;
	}
}

void
e_alarm_list_remove (EAlarmList *alarm_list,
                     GtkTreeIter *iter)
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
e_alarm_list_get_iter (GtkTreeModel *tree_model,
                       GtkTreeIter *iter,
                       GtkTreePath *path)
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
	iter->stamp = alarm_list->stamp;
	return TRUE;
}

static GtkTreePath *
e_alarm_list_get_path (GtkTreeModel *tree_model,
                       GtkTreeIter *iter)
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
static gchar *
get_alarm_duration_string (ICalDuration *duration)
{
	gint seconds = 0;

	seconds = i_cal_duration_as_int (duration);

	if (!seconds)
		return NULL;

	if (seconds < 0)
		seconds *= -1;

	return e_cal_util_seconds_to_string (seconds);
}

static gchar *
get_alarm_string (ECalComponentAlarm *alarm)
{
	ECalComponentAlarmAction action;
	ECalComponentAlarmTrigger *trigger;
	ICalDuration *duration;
	const gchar *base;
	gchar *str = NULL, *dur;

	action = e_cal_component_alarm_get_action (alarm);
	trigger = e_cal_component_alarm_get_trigger (alarm);

	switch (action) {
	case E_CAL_COMPONENT_ALARM_AUDIO:
		base = C_("cal-reminders", "Play a sound");
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		base = C_("cal-reminders", "Pop up an alert");
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		base = C_("cal-reminders", "Send an email");
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		base = C_("cal-reminders", "Run a program");
		break;

	case E_CAL_COMPONENT_ALARM_NONE:
	case E_CAL_COMPONENT_ALARM_UNKNOWN:
	default:
		base = C_("cal-reminders", "Unknown action to be performed");
		break;
	}

	/* FIXME: This does not look like it will localize correctly. */

	switch (trigger ? e_cal_component_alarm_trigger_get_kind (trigger) : E_CAL_COMPONENT_ALARM_TRIGGER_NONE) {
	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START:
		duration = e_cal_component_alarm_trigger_get_duration (trigger);
		dur = get_alarm_duration_string (duration);

		if (dur) {
			if (i_cal_duration_is_neg (duration))
				str = g_strdup_printf (
					/*Translator: The first %s refers to the base, which would be actions like
					 * "Play a Sound". Second %s refers to the duration string e.g:"15 minutes"*/
					C_("cal-reminders", "%s %s before the start"),
					base, dur);
			else
				str = g_strdup_printf (
					/*Translator: The first %s refers to the base, which would be actions like
					 * "Play a Sound". Second %s refers to the duration string e.g:"15 minutes"*/
					C_("cal-reminders", "%s %s after the start"),
					base, dur);

			g_free (dur);
		} else
			/*Translator: The %s refers to the base, which would be actions like
			 * "Play a sound" */
			str = g_strdup_printf (C_("cal-reminders", "%s at the start"), base);

		break;

	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END:
		duration = e_cal_component_alarm_trigger_get_duration (trigger);
		dur = get_alarm_duration_string (duration);

		if (dur) {
			if (i_cal_duration_is_neg (duration))
				str = g_strdup_printf (
					/* Translator: The first %s refers to the base, which would be actions like
					 * "Play a Sound". Second %s refers to the duration string e.g:"15 minutes" */
					C_("cal-reminders", "%s %s before the end"),
					base, dur);
			else
				str = g_strdup_printf (
					/* Translator: The first %s refers to the base, which would be actions like
					 * "Play a Sound". Second %s refers to the duration string e.g:"15 minutes" */
					C_("cal-reminders", "%s %s after the end"),
					base, dur);

			g_free (dur);
		} else
			/* Translator: The %s refers to the base, which would be actions like
			 * "Play a sound" */
			str = g_strdup_printf (C_("cal-reminders", "%s at the end"), base);

		break;

	case E_CAL_COMPONENT_ALARM_TRIGGER_ABSOLUTE: {
		ICalTime *itt;
		ICalTimezone *utc_zone, *current_zone;
		struct tm tm;
		gchar buf[256];

		/* Absolute triggers come in UTC, so convert them to the local timezone */

		itt = e_cal_component_alarm_trigger_get_absolute_time (trigger);

		utc_zone = i_cal_timezone_get_utc_timezone ();
		current_zone = calendar_config_get_icaltimezone ();

		tm = e_cal_util_icaltime_to_tm_with_zone (itt, utc_zone, current_zone);

		e_time_format_date_and_time (&tm, calendar_config_get_24_hour_format (),
					     FALSE, FALSE, buf, sizeof (buf));

		/* Translator: The first %s refers to the base, which would be actions like
		 * "Play a Sound". Second %s is an absolute time, e.g. "10:00AM" */
		str = g_strdup_printf (C_("cal-reminders", "%s at %s"), base, buf);

		break; }

	case E_CAL_COMPONENT_ALARM_TRIGGER_NONE:
	default:
		/* Translator: The %s refers to the base, which would be actions like
		 * "Play a sound". "Trigger types" are absolute or relative dates */
		str = g_strdup_printf (C_("cal-reminders", "%s for an unknown trigger type"), base);
		break;
	}

	return str;
}

static void
e_alarm_list_get_value (GtkTreeModel *tree_model,
                        GtkTreeIter *iter,
                        gint column,
                        GValue *value)
{
	EAlarmList        *alarm_list = E_ALARM_LIST (tree_model);
	ECalComponentAlarm *alarm;
	GList             *l;
	gchar	  *str;

	g_return_if_fail (E_IS_ALARM_LIST (tree_model));
	g_return_if_fail (column < E_ALARM_LIST_NUM_COLUMNS);
	g_return_if_fail (E_ALARM_LIST (tree_model)->stamp == iter->stamp);
	g_return_if_fail (IS_VALID_ITER (alarm_list, iter));

	g_value_init (value, column_types[column]);

	if (!alarm_list->list)
		return;

	l = iter->user_data;
	alarm = l->data;

	if (!alarm)
		return;

	switch (column) {
		case E_ALARM_LIST_COLUMN_DESCRIPTION:
			str = get_alarm_string (alarm);
			g_value_set_string (value, str);
			g_free (str);
			break;
	}
}

static gboolean
e_alarm_list_iter_next (GtkTreeModel *tree_model,
                        GtkTreeIter *iter)
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
                            GtkTreeIter *iter,
                            GtkTreeIter *parent)
{
	EAlarmList *alarm_list = E_ALARM_LIST (tree_model);

	/* this is a list, nodes have no children */
	if (parent)
		return FALSE;

	/* but if parent == NULL we return the list itself as children of the
	 * "root" */

	if (!alarm_list->list)
		return FALSE;

	iter->stamp = E_ALARM_LIST (tree_model)->stamp;
	iter->user_data = alarm_list->list;
	return TRUE;
}

static gboolean
e_alarm_list_iter_has_child (GtkTreeModel *tree_model,
                             GtkTreeIter *iter)
{
	g_return_val_if_fail (IS_VALID_ITER (E_ALARM_LIST (tree_model), iter), FALSE);
	return FALSE;
}

static gint
e_alarm_list_iter_n_children (GtkTreeModel *tree_model,
                              GtkTreeIter *iter)
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
                             GtkTreeIter *iter,
                             GtkTreeIter *parent,
                             gint n)
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

		iter->stamp = alarm_list->stamp;
		iter->user_data = l;
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_alarm_list_iter_parent (GtkTreeModel *tree_model,
                          GtkTreeIter *iter,
                          GtkTreeIter *child)
{
	return FALSE;
}
