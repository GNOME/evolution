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
#include <e-util/e-util.h>

#define PARENT_TYPE (gtk_object_get_type ())

static GtkObjectClass *parent_class;

static void
etc_destroy (GtkObject *object)
{
	ETableCol *etc = E_TABLE_COL (object);

	g_free (etc->text);
	
	(*parent_class->destroy)(object);
}
      
static void
e_table_col_class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);
	object_class->destroy = etc_destroy;
}

E_MAKE_TYPE(e_table_col, "ETableCol", ETableCol, e_table_col_class_init, NULL, PARENT_TYPE);

ETableCol *
e_table_col_new (int col_idx, const char *text, int width, int min_width,
		 ECell *ecell, GCompareFunc compare, gboolean resizable)
{
	ETableCol *etc;
	
	g_return_val_if_fail (width >= 0, NULL);
	g_return_val_if_fail (min_width >= 0, NULL);
	g_return_val_if_fail (width >= min_width, NULL);
	g_return_val_if_fail (compare != NULL, NULL);

	etc = gtk_type_new (E_TABLE_COL_TYPE);

	etc->col_idx = col_idx;
	etc->text = g_strdup (text);
	etc->width = width;
	etc->min_width = min_width;
	etc->ecell = ecell;
	etc->compare = compare;

	etc->selected = 0;
	etc->resizeable = resizable;

	return etc;
}


