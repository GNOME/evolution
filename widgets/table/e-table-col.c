/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include "gal/util/e-util.h"

#define PARENT_TYPE (gtk_object_get_type ())

static GtkObjectClass *parent_class;


enum {
	ARG_0,
	ARG_SORTABLE,
};

static void
etc_destroy (GtkObject *object)
{
	ETableCol *etc = E_TABLE_COL (object);

	gtk_object_unref (GTK_OBJECT(etc->ecell));

	if (etc->is_pixbuf)
		gdk_pixbuf_unref (etc->pixbuf);
	else
		g_free (etc->text);
	
	(*parent_class->destroy)(object);
}
 

static void
etc_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableCol *etc = E_TABLE_COL (o);
	
	switch (arg_id){
	case ARG_SORTABLE:
		etc->sortable = GTK_VALUE_BOOL(*arg);
		break;
	}
}

static void
etc_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableCol *etc = E_TABLE_COL (o);
	
	switch (arg_id){
	case ARG_SORTABLE:
		GTK_VALUE_BOOL(*arg) = etc->sortable;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
     
static void
e_table_col_class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);
	object_class->destroy = etc_destroy;
	object_class->get_arg = etc_get_arg;
	object_class->set_arg = etc_set_arg;

	gtk_object_add_arg_type ("ETableCol::sortable",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_SORTABLE);  
}

static void
e_table_col_init (ETableCol *etc)
{
	etc->width = 0;
	etc->sortable = 1;
	etc->groupable = 1;
	etc->justification = GTK_JUSTIFY_LEFT;
}

E_MAKE_TYPE(e_table_col, "ETableCol", ETableCol, e_table_col_class_init, e_table_col_init, PARENT_TYPE);

/** 
 * e_table_col_new:
 * @col_idx: the column we represent in the model
 * @text: a title for this column
 * @expansion: FIXME
 * @min_width: minimum width in pixels for this column
 * @ecell: the renderer to be used for this column
 * @compare: comparision function for the elements stored in this column
 * @resizable: whether the column can be resized interactively by the user
 *
 * The ETableCol represents a column to be used inside an ETable.  The
 * ETableCol objects are inserted inside an ETableHeader (which is just a collection
 * of ETableCols).  The ETableHeader is the definition of the order in which
 * columns are shown to the user. 
 *
 * The @text argument is the the text that will be shown as a header to the
 * user. @col_idx reflects where the data for this ETableCol object will
 * be fetch from an ETableModel.  So even if the user changes the order
 * of the columns being viewed (the ETableCols in the ETableHeader), the
 * column will always point to the same column inside the ETableModel.
 *
 * The @ecell argument is an ECell object that needs to know how to render the
 * data in the ETableModel for this specific row. 
 *
 * Returns: the newly created ETableCol object.
 */
ETableCol *
e_table_col_new (int col_idx, const char *text, double expansion, int min_width,
		 ECell *ecell, GCompareFunc compare, gboolean resizable)
{
	ETableCol *etc;
	
	g_return_val_if_fail (expansion >= 0, NULL);
	g_return_val_if_fail (min_width >= 0, NULL);
	g_return_val_if_fail (ecell != NULL, NULL);
	g_return_val_if_fail (compare != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	etc = gtk_type_new (E_TABLE_COL_TYPE);
       
	etc->is_pixbuf = FALSE;

	etc->col_idx = col_idx;
	etc->text = g_strdup (text);
	etc->pixbuf = NULL;
	etc->expansion = expansion;
	etc->min_width = min_width;
	etc->ecell = ecell;
	etc->compare = compare;

	etc->selected = 0;
	etc->resizeable = resizable;

	gtk_object_ref (GTK_OBJECT(etc->ecell));

	return etc;
}

/** 
 * e_table_col_new_with_pixbuf:
 * @col_idx: the column we represent in the model
 * @pixbuf: the image to be used for the header
 * @expansion: FIXME
 * @min_width: minimum width in pixels for this column
 * @ecell: the renderer to be used for this column
 * @compare: comparision function for the elements stored in this column
 * @resizable: whether the column can be resized interactively by the user
 *
 * The ETableCol represents a column to be used inside an ETable.  The
 * ETableCol objects are inserted inside an ETableHeader (which is just a collection
 * of ETableCols).  The ETableHeader is the definition of the order in which
 * columns are shown to the user. 
 *
 * The @text argument is the the text that will be shown as a header to the
 * user. @col_idx reflects where the data for this ETableCol object will
 * be fetch from an ETableModel.  So even if the user changes the order
 * of the columns being viewed (the ETableCols in the ETableHeader), the
 * column will always point to the same column inside the ETableModel.
 *
 * The @ecell argument is an ECell object that needs to know how to render the
 * data in the ETableModel for this specific row. 
 *
 * Returns: the newly created ETableCol object.
 */
ETableCol *
e_table_col_new_with_pixbuf (int col_idx, GdkPixbuf *pixbuf, double expansion, int min_width,
			     ECell *ecell, GCompareFunc compare, gboolean resizable)
{
	ETableCol *etc;
	
	g_return_val_if_fail (expansion >= 0, NULL);
	g_return_val_if_fail (min_width >= 0, NULL);
	g_return_val_if_fail (ecell != NULL, NULL);
	g_return_val_if_fail (compare != NULL, NULL);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	etc = gtk_type_new (E_TABLE_COL_TYPE);

	etc->is_pixbuf = TRUE;

	etc->col_idx = col_idx;
	etc->text = NULL;
	etc->pixbuf = pixbuf;
	etc->expansion = expansion;
	etc->min_width = min_width;
	etc->ecell = ecell;
	etc->compare = compare;

	etc->selected = 0;
	etc->resizeable = resizable;

	gtk_object_ref (GTK_OBJECT(etc->ecell));
	gdk_pixbuf_ref (etc->pixbuf);

	return etc;
}
