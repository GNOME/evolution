/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* EDateTimeList - list of calendar dates/times with GtkTreeModel interface.
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

#include <string.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktreednd.h>
#include <glib.h>
#include <e-util/e-time-utils.h>
#include "e-date-time-list.h"
#include <libecal/e-cal-time-util.h>
#include "calendar-config.h"

#define G_LIST(x)                        ((GList *) x)
#define E_DATE_TIME_LIST_IS_SORTED(list) (E_DATE_TIME_LIST (list)->sort_column_id != -2)
#define IS_VALID_ITER(dt_list, iter)     (iter!= NULL && iter->user_data != NULL && \
                                          dt_list->stamp == iter->stamp)

static GType column_types [E_DATE_TIME_LIST_NUM_COLUMNS];

static void         e_date_time_list_init            (EDateTimeList      *file_list);
static void         e_date_time_list_class_init      (EDateTimeListClass *class);
static void         e_date_time_list_tree_model_init (GtkTreeModelIface  *iface);
static void         e_date_time_list_finalize        (GObject            *object);
static guint        e_date_time_list_get_flags       (GtkTreeModel       *tree_model);
static gint         e_date_time_list_get_n_columns   (GtkTreeModel       *tree_model);
static GType        e_date_time_list_get_column_type (GtkTreeModel       *tree_model,
						      gint                index);
static gboolean     e_date_time_list_get_iter        (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter,
						      GtkTreePath        *path);
static GtkTreePath *e_date_time_list_get_path        (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter);
static void         e_date_time_list_get_value       (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter,
						      gint                column,
						      GValue             *value);
static gboolean     e_date_time_list_iter_next       (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter);
static gboolean     e_date_time_list_iter_children   (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter,
						      GtkTreeIter        *parent);
static gboolean     e_date_time_list_iter_has_child  (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter);
static gint         e_date_time_list_iter_n_children (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter);
static gboolean     e_date_time_list_iter_nth_child  (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter,
						      GtkTreeIter        *parent,
						      gint                n);
static gboolean     e_date_time_list_iter_parent     (GtkTreeModel       *tree_model,
						      GtkTreeIter        *iter,
						      GtkTreeIter        *child);

static GObjectClass *parent_class = NULL;

GtkType
e_date_time_list_get_type (void)
{
	static GType date_time_list_type = 0;

	if (!date_time_list_type) {
		static const GTypeInfo date_time_list_info =
		{
			sizeof (EDateTimeListClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) e_date_time_list_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (EDateTimeList),
			0,
			(GInstanceInitFunc) e_date_time_list_init,
		};

		static const GInterfaceInfo tree_model_info =
		{
			(GInterfaceInitFunc) e_date_time_list_tree_model_init,
			NULL,
			NULL
		};

		column_types [E_DATE_TIME_LIST_COLUMN_DESCRIPTION] = G_TYPE_STRING;

		date_time_list_type = g_type_register_static (G_TYPE_OBJECT, "EDateTimeList",
							      &date_time_list_info, 0);
		g_type_add_interface_static (date_time_list_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return date_time_list_type;
}

static void
e_date_time_list_class_init (EDateTimeListClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	object_class = (GObjectClass*) class;

	object_class->finalize = e_date_time_list_finalize;
}

static void
e_date_time_list_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = e_date_time_list_get_flags;
	iface->get_n_columns = e_date_time_list_get_n_columns;
	iface->get_column_type = e_date_time_list_get_column_type;
	iface->get_iter = e_date_time_list_get_iter;
	iface->get_path = e_date_time_list_get_path;
	iface->get_value = e_date_time_list_get_value;
	iface->iter_next = e_date_time_list_iter_next;
	iface->iter_children = e_date_time_list_iter_children;
	iface->iter_has_child = e_date_time_list_iter_has_child;
	iface->iter_n_children = e_date_time_list_iter_n_children;
	iface->iter_nth_child = e_date_time_list_iter_nth_child;
	iface->iter_parent = e_date_time_list_iter_parent;
}

static void
e_date_time_list_init (EDateTimeList *date_time_list)
{
	date_time_list->stamp         = g_random_int ();
	date_time_list->columns_dirty = FALSE;
	date_time_list->list          = NULL;
}

EDateTimeList *
e_date_time_list_new (void)
{
	EDateTimeList *date_time_list;

	date_time_list = E_DATE_TIME_LIST (g_object_new (e_date_time_list_get_type (), NULL));

	return date_time_list;
}

static void
all_rows_deleted (EDateTimeList *date_time_list)
{
	GtkTreePath *path;
	gint         i;

	if (!date_time_list->list)
		return;

	path = gtk_tree_path_new ();
	i = g_list_length (date_time_list->list);
	gtk_tree_path_append_index (path, i);

	for ( ; i >= 0; i--) {
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (date_time_list), path);
		gtk_tree_path_prev (path);
	}

	gtk_tree_path_free (path);
}

static void
row_deleted (EDateTimeList *date_time_list, gint n)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (date_time_list), path);
	gtk_tree_path_free (path);
}

static void
row_added (EDateTimeList *date_time_list, gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (date_time_list), &iter, path))
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (date_time_list), path, &iter);

	gtk_tree_path_free (path);
}

static void
row_updated (EDateTimeList *date_time_list, gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (date_time_list), &iter, path))
		gtk_tree_model_row_changed (GTK_TREE_MODEL (date_time_list), path, &iter);

	gtk_tree_path_free (path);
}

static void
e_date_time_list_finalize (GObject *object)
{
	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Fulfill the GtkTreeModel requirements */
static guint
e_date_time_list_get_flags (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), 0);

	return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
e_date_time_list_get_n_columns (GtkTreeModel *tree_model)
{
	EDateTimeList *date_time_list = (EDateTimeList *) tree_model;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), 0);

	date_time_list->columns_dirty = TRUE;
	return E_DATE_TIME_LIST_NUM_COLUMNS;
}

static GType
e_date_time_list_get_column_type (GtkTreeModel *tree_model,
				  gint          index)
{
	EDateTimeList *date_time_list = (EDateTimeList *) tree_model;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail (index < E_DATE_TIME_LIST_NUM_COLUMNS &&
			      index >= 0, G_TYPE_INVALID);

	date_time_list->columns_dirty = TRUE;
	return column_types [index];
}

const ECalComponentDateTime *
e_date_time_list_get_date_time (EDateTimeList *date_time_list, GtkTreeIter *iter)
{
	g_return_val_if_fail (IS_VALID_ITER (date_time_list, iter), NULL);

	return G_LIST (iter->user_data)->data;
}

static void
free_datetime (ECalComponentDateTime *datetime)
{
	g_free (datetime->value);
	if (datetime->tzid)
		g_free ((gchar *) datetime->tzid);
	g_free (datetime);
}

static ECalComponentDateTime *
copy_datetime (const ECalComponentDateTime *datetime)
{
	ECalComponentDateTime *datetime_copy;

	datetime_copy = g_new0 (ECalComponentDateTime, 1);
	datetime_copy->value  = g_new (struct icaltimetype, 1);
	*datetime_copy->value = *datetime->value;

	if (datetime->tzid)
		datetime_copy->tzid = g_strdup (datetime->tzid);

	return datetime_copy;
}

static gint
compare_datetime (const ECalComponentDateTime *datetime1, const ECalComponentDateTime *datetime2)
{
	return icaltime_compare (*datetime1->value, *datetime2->value);
}

void
e_date_time_list_set_date_time (EDateTimeList *date_time_list, GtkTreeIter *iter,
				const ECalComponentDateTime *datetime)
{
	ECalComponentDateTime *datetime_old;

	g_return_if_fail (IS_VALID_ITER (date_time_list, iter));

	datetime_old = G_LIST (iter->user_data)->data;
	free_datetime (datetime_old);
	G_LIST (iter->user_data)->data = copy_datetime (datetime);
	row_updated (date_time_list, g_list_position (date_time_list->list, G_LIST (iter->user_data)));
}

void
e_date_time_list_append (EDateTimeList *date_time_list, GtkTreeIter *iter,
			 const ECalComponentDateTime *datetime)
{
	g_return_if_fail (datetime != NULL);

	if (g_list_find_custom (date_time_list->list, datetime, (GCompareFunc)compare_datetime) == NULL) {
		date_time_list->list = g_list_append (date_time_list->list, copy_datetime (datetime));
		row_added (date_time_list, g_list_length (date_time_list->list) - 1);
	}

	if (iter) {
		iter->user_data = g_list_last (date_time_list->list);
		iter->stamp     = date_time_list->stamp;
	}
}

void
e_date_time_list_remove (EDateTimeList *date_time_list, GtkTreeIter *iter)
{
	gint n;

	g_return_if_fail (IS_VALID_ITER (date_time_list, iter));

	n = g_list_position (date_time_list->list, G_LIST (iter->user_data));
	free_datetime ((ECalComponentDateTime *) G_LIST (iter->user_data)->data);
	date_time_list->list = g_list_delete_link (date_time_list->list, G_LIST (iter->user_data));
	row_deleted (date_time_list, n);
}

void
e_date_time_list_clear (EDateTimeList *date_time_list)
{
	GList *l;

	all_rows_deleted (date_time_list);

	for (l = date_time_list->list; l; l = g_list_next (l)) {
		free_datetime ((ECalComponentDateTime *) l->data);
	}

	g_list_free (date_time_list->list);
	date_time_list->list = NULL;
}

static gboolean
e_date_time_list_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
	EDateTimeList *date_time_list = (EDateTimeList *) tree_model;
	GList         *l;
	gint           i;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	if (!date_time_list->list)
		return FALSE;

	date_time_list->columns_dirty = TRUE;

	i = gtk_tree_path_get_indices (path)[0];
	l = g_list_nth (date_time_list->list, i);
	if (!l)
		return FALSE;

	iter->user_data = l;
	iter->stamp     = date_time_list->stamp;
	return TRUE;
}

static GtkTreePath *
e_date_time_list_get_path (GtkTreeModel *tree_model,
			   GtkTreeIter  *iter)
{
	EDateTimeList *date_time_list = (EDateTimeList *) tree_model;
	GtkTreePath   *retval;
	GList         *l;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), NULL);
	g_return_val_if_fail (iter->stamp == E_DATE_TIME_LIST (tree_model)->stamp, NULL);

	l = iter->user_data;
	retval = gtk_tree_path_new ();
	gtk_tree_path_append_index (retval, g_list_position (date_time_list->list, l));
	return retval;
}

/* Builds a static string out of an exception date */
static char *
get_exception_string (ECalComponentDateTime *dt)
{
	static char buf [256];
	struct tm tmp_tm;

	tmp_tm.tm_year  = dt->value->year - 1900;
	tmp_tm.tm_mon   = dt->value->month - 1;
	tmp_tm.tm_mday  = dt->value->day;
	tmp_tm.tm_hour  = dt->value->hour;
	tmp_tm.tm_min   = dt->value->minute;
	tmp_tm.tm_sec   = dt->value->second;
	tmp_tm.tm_isdst = -1;

	tmp_tm.tm_wday = time_day_of_week (dt->value->day,
					   dt->value->month - 1,
					   dt->value->year);

	e_time_format_date_and_time (&tmp_tm, calendar_config_get_24_hour_format (),
				     FALSE, FALSE, buf, sizeof (buf));

	return buf;
}

static void
e_date_time_list_get_value (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    gint          column,
			    GValue       *value)
{
	EDateTimeList        *date_time_list = E_DATE_TIME_LIST (tree_model);
	ECalComponentDateTime *datetime;
	GList                *l;
	const gchar          *str;

	g_return_if_fail (E_IS_DATE_TIME_LIST (tree_model));
	g_return_if_fail (column < E_DATE_TIME_LIST_NUM_COLUMNS);
	g_return_if_fail (E_DATE_TIME_LIST (tree_model)->stamp == iter->stamp);
	g_return_if_fail (IS_VALID_ITER (date_time_list, iter));

	g_value_init (value, column_types [column]);

	if (!date_time_list->list)
		return;

	l        = iter->user_data;
	datetime = l->data;

	if (!datetime)
		return;

	switch (column) {
		case E_DATE_TIME_LIST_COLUMN_DESCRIPTION:
			str = get_exception_string (datetime);
			g_value_set_string (value, str);
			break;
	}
}

static gboolean
e_date_time_list_iter_next (GtkTreeModel  *tree_model,
			    GtkTreeIter   *iter)
{
	GList *l;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), FALSE);
	g_return_val_if_fail (IS_VALID_ITER (E_DATE_TIME_LIST (tree_model), iter), FALSE);

	if (!E_DATE_TIME_LIST (tree_model)->list)
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
e_date_time_list_iter_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter,
				GtkTreeIter  *parent)
{
	EDateTimeList *date_time_list = E_DATE_TIME_LIST (tree_model);

	/* this is a list, nodes have no children */
	if (parent)
		return FALSE;

	/* but if parent == NULL we return the list itself as children of the
	 * "root" */

	if (!date_time_list->list)
		return FALSE;

	iter->stamp     = E_DATE_TIME_LIST (tree_model)->stamp;
	iter->user_data = date_time_list->list;
	return TRUE;
}

static gboolean
e_date_time_list_iter_has_child (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter)
{
	g_return_val_if_fail (IS_VALID_ITER (E_DATE_TIME_LIST (tree_model), iter), FALSE);
	return FALSE;
}

static gint
e_date_time_list_iter_n_children (GtkTreeModel *tree_model,
				  GtkTreeIter  *iter)
{
	EDateTimeList *date_time_list = E_DATE_TIME_LIST (tree_model);

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), -1);

	if (iter == NULL)
		return g_list_length (date_time_list->list);

	g_return_val_if_fail (E_DATE_TIME_LIST (tree_model)->stamp == iter->stamp, -1);
	return 0;
}

static gboolean
e_date_time_list_iter_nth_child (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 GtkTreeIter  *parent,
				 gint          n)
{
	EDateTimeList *date_time_list = E_DATE_TIME_LIST (tree_model);

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), FALSE);

	if (parent)
		return FALSE;

	if (date_time_list->list) {
		GList *l;

		l = g_list_nth (date_time_list->list, n);
		if (!l)
			return FALSE;

		iter->stamp     = date_time_list->stamp;
		iter->user_data = l;
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_date_time_list_iter_parent (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *child)
{
	return FALSE;
}
