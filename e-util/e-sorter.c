/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-sorter.h"

G_DEFINE_TYPE (ESorter, e_sorter, G_TYPE_OBJECT)

static gint
sorter_model_to_sorted (ESorter *sorter,
                        gint row)
{
	return row;
}

static gint
sorter_sorted_to_model (ESorter *sorter,
                        gint row)
{
	return row;
}

static void
sorter_get_model_to_sorted_array (ESorter *sorter,
                                  gint **array,
                                  gint *count)
{
}

static void
sorter_get_sorted_to_model_array (ESorter *sorter,
                                  gint **array,
                                  gint *count)
{
}

static gboolean
sorter_needs_sorting (ESorter *sorter)
{
	return FALSE;
}

static void
e_sorter_class_init (ESorterClass *class)
{
	class->model_to_sorted = sorter_model_to_sorted;
	class->sorted_to_model = sorter_sorted_to_model;
	class->get_model_to_sorted_array = sorter_get_model_to_sorted_array;
	class->get_sorted_to_model_array = sorter_get_sorted_to_model_array;
	class->needs_sorting = sorter_needs_sorting;
}

static void
e_sorter_init (ESorter *sorter)
{
}

ESorter *
e_sorter_new (void)
{
	return g_object_new (E_TYPE_SORTER, NULL);
}

gint
e_sorter_model_to_sorted (ESorter *sorter,
                          gint row)
{
	ESorterClass *class;

	g_return_val_if_fail (E_IS_SORTER (sorter), -1);
	g_return_val_if_fail (row >= 0, -1);

	class = E_SORTER_GET_CLASS (sorter);
	g_return_val_if_fail (class->model_to_sorted != NULL, -1);

	return class->model_to_sorted (sorter, row);
}

gint
e_sorter_sorted_to_model (ESorter *sorter,
                          gint row)
{
	ESorterClass *class;

	g_return_val_if_fail (E_IS_SORTER (sorter), -1);
	g_return_val_if_fail (row >= 0, -1);

	class = E_SORTER_GET_CLASS (sorter);
	g_return_val_if_fail (class->sorted_to_model != NULL, -1);

	return class->sorted_to_model (sorter, row);
}

void
e_sorter_get_model_to_sorted_array (ESorter *sorter,
                                    gint **array,
                                    gint *count)
{
	ESorterClass *class;

	g_return_if_fail (E_IS_SORTER (sorter));

	class = E_SORTER_GET_CLASS (sorter);
	g_return_if_fail (class->get_model_to_sorted_array != NULL);

	class->get_model_to_sorted_array (sorter, array, count);
}

void
e_sorter_get_sorted_to_model_array (ESorter *sorter,
                                    gint **array,
                                    gint *count)
{
	ESorterClass *class;

	g_return_if_fail (E_IS_SORTER (sorter));

	class = E_SORTER_GET_CLASS (sorter);
	g_return_if_fail (class->get_sorted_to_model_array != NULL);

	class->get_sorted_to_model_array (sorter, array, count);
}

gboolean
e_sorter_needs_sorting (ESorter *sorter)
{
	ESorterClass *class;

	g_return_val_if_fail (E_IS_SORTER (sorter), FALSE);

	class = E_SORTER_GET_CLASS (sorter);
	g_return_val_if_fail (class->needs_sorting != NULL, FALSE);

	return class->needs_sorting (sorter);
}

