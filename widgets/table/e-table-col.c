/*
 * E-table-col.c: ETableCol implementation
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc
 */
#include <config.h>
#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include "e-table-col.h"

ETableCol *
e_table_col_new (const char *id, int width, int min_width,
		 ETableColRenderFn render, void *render_data,
		 GCompareFunc compare, gboolean resizable)
{
	ETableCol *etc;
	
	g_return_if_fail (id != NULL);
	g_return_if_fail (width >= 0);
	g_return_if_fail (min_width >= 0);
	g_return_if_fail (width >= min_width);
	g_return_if_fail (render != NULL);
	g_return_if_fail (compare != NULL);

	etc = g_new (ETableCol, 1);

	etc->id = g_strdup (id);
	etc->width = width;
	etc->min_width = min_width;
	etc->render = render;
	etc->render_data = render_data;
	etc->compare = compare;

	etc->selected = 0;
	etc->resizeable = 0;

	return etc;
}



