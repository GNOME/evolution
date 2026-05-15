/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "e-sorter.h"

G_DEFINE_INTERFACE (ESorter, e_sorter, G_TYPE_OBJECT)

static void
e_sorter_default_init (ESorterInterface *iface)
{
}

gint
e_sorter_model_to_sorted (ESorter *sorter,
                          gint row)
{
	ESorterInterface *iface;

	g_return_val_if_fail (E_IS_SORTER (sorter), -1);
	g_return_val_if_fail (row >= 0, -1);

	iface = E_SORTER_GET_INTERFACE (sorter);
	g_return_val_if_fail (iface->model_to_sorted != NULL, -1);

	return iface->model_to_sorted (sorter, row);
}

gint
e_sorter_sorted_to_model (ESorter *sorter,
                          gint row)
{
	ESorterInterface *iface;

	g_return_val_if_fail (E_IS_SORTER (sorter), -1);
	g_return_val_if_fail (row >= 0, -1);

	iface = E_SORTER_GET_INTERFACE (sorter);
	g_return_val_if_fail (iface->sorted_to_model != NULL, -1);

	return iface->sorted_to_model (sorter, row);
}

void
e_sorter_get_model_to_sorted_array (ESorter *sorter,
                                    gint **array,
                                    gint *count)
{
	ESorterInterface *iface;

	g_return_if_fail (E_IS_SORTER (sorter));

	iface = E_SORTER_GET_INTERFACE (sorter);
	g_return_if_fail (iface->get_model_to_sorted_array != NULL);

	iface->get_model_to_sorted_array (sorter, array, count);
}

void
e_sorter_get_sorted_to_model_array (ESorter *sorter,
                                    gint **array,
                                    gint *count)
{
	ESorterInterface *iface;

	g_return_if_fail (E_IS_SORTER (sorter));

	iface = E_SORTER_GET_INTERFACE (sorter);
	g_return_if_fail (iface->get_sorted_to_model_array != NULL);

	iface->get_sorted_to_model_array (sorter, array, count);
}

gboolean
e_sorter_needs_sorting (ESorter *sorter)
{
	ESorterInterface *iface;

	g_return_val_if_fail (E_IS_SORTER (sorter), FALSE);

	iface = E_SORTER_GET_INTERFACE (sorter);
	g_return_val_if_fail (iface->needs_sorting != NULL, FALSE);

	return iface->needs_sorting (sorter);
}

