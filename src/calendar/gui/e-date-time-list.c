/*
 * EDateTimeList - list of calendar dates/times with GtkTreeModel interface.
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

#include "comp-util.h"

#include "e-date-time-list.h"

#include <string.h>
#include <libecal/libecal.h>

/* XXX Was it really necessary to implement a custom GtkTreeModel for a
 *     one-column list store?  There's no mention of why this was done. */

#define G_LIST(x)                        ((GList *) x)
#define E_DATE_TIME_LIST_IS_SORTED(list) \
	(E_DATE_TIME_LIST (list)->sort_column_id != -2)
#define IS_VALID_ITER(dt_list, iter) \
	(iter != NULL && iter->user_data != NULL && \
	dt_list->priv->stamp == iter->stamp)

struct _EDateTimeListPrivate {
	gint     stamp;
	GList   *list;

	guint    columns_dirty : 1;

	gboolean use_24_hour_format;
	ICalTimezone *zone;
};

enum {
	PROP_0,
	PROP_USE_24_HOUR_FORMAT,
	PROP_TIMEZONE
};

static GType column_types[E_DATE_TIME_LIST_NUM_COLUMNS];

static void e_date_time_list_tree_model_init (GtkTreeModelIface *iface);

G_DEFINE_TYPE_WITH_CODE (EDateTimeList, e_date_time_list, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EDateTimeList)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, e_date_time_list_tree_model_init))

static gint
compare_datetime (const ICalTime *itt1,
                  const ICalTime *itt2)
{
	return i_cal_time_compare ((ICalTime *) itt1, (ICalTime *) itt2);
}

static void
all_rows_deleted (EDateTimeList *date_time_list)
{
	GtkTreePath *path;
	gint         i;

	if (!date_time_list->priv->list)
		return;

	path = gtk_tree_path_new ();
	i = g_list_length (date_time_list->priv->list);
	gtk_tree_path_append_index (path, i);

	for (; i >= 0; i--) {
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (date_time_list), path);
		gtk_tree_path_prev (path);
	}

	gtk_tree_path_free (path);
}

static void
row_deleted (EDateTimeList *date_time_list,
             gint n)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (date_time_list), path);
	gtk_tree_path_free (path);
}

static void
row_added (EDateTimeList *date_time_list,
           gint n)
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
row_updated (EDateTimeList *date_time_list,
             gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (date_time_list), &iter, path))
		gtk_tree_model_row_changed (GTK_TREE_MODEL (date_time_list), path, &iter);

	gtk_tree_path_free (path);
}

/* Builds a static string out of an exception date */
static gchar *
get_exception_string (EDateTimeList *date_time_list,
		      ICalTime *itt)
{
	static gchar buf[256];
	ICalTime *tt;
	ICalTimezone *zone;

	zone = e_date_time_list_get_timezone (date_time_list);

	if (zone)
		tt = i_cal_time_convert_to_zone (itt, zone);
	else
		tt = g_object_ref (itt);

	cal_comp_util_format_itt (tt, buf, sizeof (buf));

	g_clear_object (&tt);

	return buf;
}

static void
date_time_list_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_USE_24_HOUR_FORMAT:
			e_date_time_list_set_use_24_hour_format (
				E_DATE_TIME_LIST (object),
				g_value_get_boolean (value));
			return;

		case PROP_TIMEZONE:
			e_date_time_list_set_timezone (
				E_DATE_TIME_LIST (object),
				g_value_get_pointer (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
date_time_list_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_USE_24_HOUR_FORMAT:
			g_value_set_boolean (
				value,
				e_date_time_list_get_use_24_hour_format (
				E_DATE_TIME_LIST (object)));
			return;

		case PROP_TIMEZONE:
			g_value_set_pointer (
				value, e_date_time_list_get_timezone (
				E_DATE_TIME_LIST (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
date_time_list_finalize (GObject *object)
{
	EDateTimeList *date_time_list = E_DATE_TIME_LIST (object);

	g_clear_object (&date_time_list->priv->zone);

	g_list_free_full (date_time_list->priv->list, g_object_unref);
	date_time_list->priv->list = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_date_time_list_parent_class)->finalize (object);
}

static GtkTreeModelFlags
date_time_list_get_flags (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), 0);

	return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
date_time_list_get_n_columns (GtkTreeModel *tree_model)
{
	EDateTimeList *date_time_list = (EDateTimeList *) tree_model;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), 0);

	date_time_list->priv->columns_dirty = TRUE;
	return E_DATE_TIME_LIST_NUM_COLUMNS;
}

static GType
date_time_list_get_column_type (GtkTreeModel *tree_model,
                                gint index)
{
	EDateTimeList *date_time_list = (EDateTimeList *) tree_model;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail (index < E_DATE_TIME_LIST_NUM_COLUMNS &&
			      index >= 0, G_TYPE_INVALID);

	date_time_list->priv->columns_dirty = TRUE;
	return column_types[index];
}

static gboolean
date_time_list_get_iter (GtkTreeModel *tree_model,
                         GtkTreeIter *iter,
                         GtkTreePath *path)
{
	EDateTimeList *date_time_list = (EDateTimeList *) tree_model;
	GList         *l;
	gint           i;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	if (!date_time_list->priv->list)
		return FALSE;

	date_time_list->priv->columns_dirty = TRUE;

	i = gtk_tree_path_get_indices (path)[0];
	l = g_list_nth (date_time_list->priv->list, i);
	if (!l)
		return FALSE;

	iter->user_data = l;
	iter->stamp = date_time_list->priv->stamp;
	return TRUE;
}

static GtkTreePath *
date_time_list_get_path (GtkTreeModel *tree_model,
                         GtkTreeIter *iter)
{
	EDateTimeList *date_time_list = (EDateTimeList *) tree_model;
	GtkTreePath   *retval;
	GList         *l;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), NULL);
	g_return_val_if_fail (iter->stamp == E_DATE_TIME_LIST (tree_model)->priv->stamp, NULL);

	l = iter->user_data;
	retval = gtk_tree_path_new ();
	gtk_tree_path_append_index (retval, g_list_position (date_time_list->priv->list, l));
	return retval;
}

static void
date_time_list_get_value (GtkTreeModel *tree_model,
                          GtkTreeIter *iter,
                          gint column,
                          GValue *value)
{
	EDateTimeList *date_time_list = E_DATE_TIME_LIST (tree_model);
	ICalTime *itt;
	GList *link;
	const gchar *str;

	g_return_if_fail (E_IS_DATE_TIME_LIST (tree_model));
	g_return_if_fail (column < E_DATE_TIME_LIST_NUM_COLUMNS);
	g_return_if_fail (E_DATE_TIME_LIST (tree_model)->priv->stamp == iter->stamp);
	g_return_if_fail (IS_VALID_ITER (date_time_list, iter));

	g_value_init (value, column_types[column]);

	if (!date_time_list->priv->list)
		return;

	link = iter->user_data;
	itt = link->data;

	if (!itt)
		return;

	switch (column) {
		case E_DATE_TIME_LIST_COLUMN_DESCRIPTION:
			str = get_exception_string (date_time_list, itt);
			g_value_set_string (value, str);
			break;
	}
}

static gboolean
date_time_list_iter_next (GtkTreeModel *tree_model,
                          GtkTreeIter *iter)
{
	GList *l;

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), FALSE);
	g_return_val_if_fail (IS_VALID_ITER (E_DATE_TIME_LIST (tree_model), iter), FALSE);

	if (!E_DATE_TIME_LIST (tree_model)->priv->list)
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
date_time_list_iter_children (GtkTreeModel *tree_model,
                              GtkTreeIter *iter,
                              GtkTreeIter *parent)
{
	EDateTimeList *date_time_list = E_DATE_TIME_LIST (tree_model);

	/* this is a list, nodes have no children */
	if (parent)
		return FALSE;

	/* but if parent == NULL we return the list itself as children of the
	 * "root" */

	if (!date_time_list->priv->list)
		return FALSE;

	iter->stamp = E_DATE_TIME_LIST (tree_model)->priv->stamp;
	iter->user_data = date_time_list->priv->list;
	return TRUE;
}

static gboolean
date_time_list_iter_has_child (GtkTreeModel *tree_model,
                               GtkTreeIter *iter)
{
	g_return_val_if_fail (IS_VALID_ITER (E_DATE_TIME_LIST (tree_model), iter), FALSE);
	return FALSE;
}

static gint
date_time_list_iter_n_children (GtkTreeModel *tree_model,
                                GtkTreeIter *iter)
{
	EDateTimeList *date_time_list = E_DATE_TIME_LIST (tree_model);

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), -1);

	if (iter == NULL)
		return g_list_length (date_time_list->priv->list);

	g_return_val_if_fail (E_DATE_TIME_LIST (tree_model)->priv->stamp == iter->stamp, -1);
	return 0;
}

static gboolean
date_time_list_iter_nth_child (GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               GtkTreeIter *parent,
                               gint n)
{
	EDateTimeList *date_time_list = E_DATE_TIME_LIST (tree_model);

	g_return_val_if_fail (E_IS_DATE_TIME_LIST (tree_model), FALSE);

	if (parent)
		return FALSE;

	if (date_time_list->priv->list) {
		GList *l;

		l = g_list_nth (date_time_list->priv->list, n);
		if (!l)
			return FALSE;

		iter->stamp = date_time_list->priv->stamp;
		iter->user_data = l;
		return TRUE;
	}

	return FALSE;
}

static gboolean
date_time_list_iter_parent (GtkTreeModel *tree_model,
                            GtkTreeIter *iter,
                            GtkTreeIter *child)
{
	return FALSE;
}

static void
e_date_time_list_class_init (EDateTimeListClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = date_time_list_set_property;
	object_class->get_property = date_time_list_get_property;
	object_class->finalize = date_time_list_finalize;

	g_object_class_install_property (
		object_class,
		PROP_USE_24_HOUR_FORMAT,
		g_param_spec_boolean (
			"use-24-hour-format",
			"Use 24-hour Format",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_pointer (
			"timezone",
			"Time Zone",
			NULL,
			G_PARAM_READWRITE));

	column_types[E_DATE_TIME_LIST_COLUMN_DESCRIPTION] = G_TYPE_STRING;
}

static void
e_date_time_list_init (EDateTimeList *date_time_list)
{
	date_time_list->priv = e_date_time_list_get_instance_private (date_time_list);

	date_time_list->priv->stamp = g_random_int ();
	date_time_list->priv->columns_dirty = FALSE;
	date_time_list->priv->list = NULL;
}

static void
e_date_time_list_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = date_time_list_get_flags;
	iface->get_n_columns = date_time_list_get_n_columns;
	iface->get_column_type = date_time_list_get_column_type;
	iface->get_iter = date_time_list_get_iter;
	iface->get_path = date_time_list_get_path;
	iface->get_value = date_time_list_get_value;
	iface->iter_next = date_time_list_iter_next;
	iface->iter_children = date_time_list_iter_children;
	iface->iter_has_child = date_time_list_iter_has_child;
	iface->iter_n_children = date_time_list_iter_n_children;
	iface->iter_nth_child = date_time_list_iter_nth_child;
	iface->iter_parent = date_time_list_iter_parent;
}

EDateTimeList *
e_date_time_list_new (void)
{
	return g_object_new (E_TYPE_DATE_TIME_LIST, NULL);
}

ICalTime *
e_date_time_list_get_date_time (EDateTimeList *date_time_list,
                                GtkTreeIter *iter)
{
	g_return_val_if_fail (IS_VALID_ITER (date_time_list, iter), NULL);

	return G_LIST (iter->user_data)->data;
}

void
e_date_time_list_set_date_time (EDateTimeList *date_time_list,
                                GtkTreeIter *iter,
                                const ICalTime *itt)
{
	ICalTime *itt_old;

	g_return_if_fail (IS_VALID_ITER (date_time_list, iter));

	itt_old = G_LIST (iter->user_data)->data;
	g_clear_object (&itt_old);
	G_LIST (iter->user_data)->data = i_cal_time_clone (itt);
	row_updated (date_time_list,
		g_list_position (date_time_list->priv->list, G_LIST (iter->user_data)));
}

gboolean
e_date_time_list_get_use_24_hour_format (EDateTimeList *date_time_list)
{
	g_return_val_if_fail (E_IS_DATE_TIME_LIST (date_time_list), FALSE);

	return date_time_list->priv->use_24_hour_format;
}

void
e_date_time_list_set_use_24_hour_format (EDateTimeList *date_time_list,
                                         gboolean use_24_hour_format)
{
	g_return_if_fail (E_IS_DATE_TIME_LIST (date_time_list));

	if (date_time_list->priv->use_24_hour_format == use_24_hour_format)
		return;

	date_time_list->priv->use_24_hour_format = use_24_hour_format;

	g_object_notify (G_OBJECT (date_time_list), "use-24-hour-format");
}

ICalTimezone *
e_date_time_list_get_timezone (EDateTimeList *date_time_list)
{
	g_return_val_if_fail (E_IS_DATE_TIME_LIST (date_time_list), NULL);

	return date_time_list->priv->zone;
}

void
e_date_time_list_set_timezone (EDateTimeList *date_time_list,
			       const ICalTimezone *zone)
{
	g_return_if_fail (E_IS_DATE_TIME_LIST (date_time_list));

	if (date_time_list->priv->zone == zone)
		return;

	g_clear_object (&date_time_list->priv->zone);
	if (zone)
		date_time_list->priv->zone = g_object_ref ((ICalTimezone *) zone);

	g_object_notify (G_OBJECT (date_time_list), "timezone");
}

void
e_date_time_list_append (EDateTimeList *date_time_list,
                         GtkTreeIter *iter,
                         const ICalTime *itt)
{
	g_return_if_fail (itt != NULL);
	g_return_if_fail (i_cal_time_is_valid_time ((ICalTime *) itt));

	if (g_list_find_custom (date_time_list->priv->list, itt, (GCompareFunc) compare_datetime) == NULL) {
		date_time_list->priv->list = g_list_append (date_time_list->priv->list, i_cal_time_clone (itt));
		row_added (date_time_list, g_list_length (date_time_list->priv->list) - 1);
	}

	if (iter) {
		iter->user_data = g_list_last (date_time_list->priv->list);
		iter->stamp = date_time_list->priv->stamp;
	}
}

void
e_date_time_list_remove (EDateTimeList *date_time_list,
                         GtkTreeIter *iter)
{
	ICalTime *itt;
	gint n;

	g_return_if_fail (IS_VALID_ITER (date_time_list, iter));

	n = g_list_position (date_time_list->priv->list, G_LIST (iter->user_data));
	itt = G_LIST (iter->user_data)->data;
	g_clear_object (&itt);
	date_time_list->priv->list = g_list_delete_link (
		date_time_list->priv->list, G_LIST (iter->user_data));
	row_deleted (date_time_list, n);
}

void
e_date_time_list_clear (EDateTimeList *date_time_list)
{
	all_rows_deleted (date_time_list);

	g_list_free_full (date_time_list->priv->list, g_object_unref);
	date_time_list->priv->list = NULL;
}
