/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-Table-Group.c: Implements the grouping objects for elements on a table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org ()
 *
 * Copyright 1999, Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-group-leaf.h"
#include "e-table-item.h"
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "e-util/e-util.h"
#include "e-util/e-canvas.h"

#define TITLE_HEIGHT         16
#define GROUP_INDENT         10

#define PARENT_TYPE e_table_group_get_type ()

static GnomeCanvasGroupClass *etgl_parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_HEIGHT,
	ARG_WIDTH,
	ARG_FROZEN
};

static void etgl_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void etgl_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void
etgl_destroy (GtkObject *object)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF(object);
	if (etgl->subset)
		gtk_object_unref (GTK_OBJECT(etgl->subset));
	if (etgl->item)
		gtk_object_destroy (GTK_OBJECT(etgl->item));
	if (GTK_OBJECT_CLASS (etgl_parent_class)->destroy)
		GTK_OBJECT_CLASS (etgl_parent_class)->destroy (object);
}

static void
e_table_group_leaf_construct (GnomeCanvasGroup *parent, ETableGroupLeaf *etgl,
			      ETableHeader *full_header,
			      ETableHeader     *header,
			      ETableModel *model,
			      int          col,
			      int          ascending)
{
	etgl->subset = E_TABLE_SUBSET_VARIABLE(e_table_sorted_variable_new (model, col, ascending, e_table_header_get_column(full_header, col)->compare));
	e_table_group_construct (parent, E_TABLE_GROUP (etgl), full_header, header, model);
}

ETableGroup *
e_table_group_leaf_new       (GnomeCanvasGroup *parent, ETableHeader *full_header,
			      ETableHeader     *header,
			      ETableModel *model,
			      int          col,
			      int          ascending)
{
	ETableGroupLeaf *etgl;

	g_return_val_if_fail (parent != NULL, NULL);
	
	etgl = gtk_type_new (e_table_group_leaf_get_type ());

	e_table_group_leaf_construct (parent, etgl, full_header,
				      header, model, col, ascending);
	return E_TABLE_GROUP (etgl);
}

static void
etgl_row_selection (GtkObject *object, gint row, gboolean selected, ETableGroupLeaf *etgl)
{
	if (row < E_TABLE_SUBSET(etgl->subset)->n_map)
		e_table_group_row_selection (E_TABLE_GROUP(etgl), E_TABLE_SUBSET(etgl->subset)->map_table[row], selected);
}

static void
etgl_reflow (GnomeCanvasItem *item, gint flags)
{
	e_canvas_item_request_parent_reflow (item);
}

static void
etgl_realize (GnomeCanvasItem *item)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF(item);

	if (GNOME_CANVAS_ITEM_CLASS (etgl_parent_class)->realize)
		GNOME_CANVAS_ITEM_CLASS (etgl_parent_class)->realize (item);

	etgl->item = E_TABLE_ITEM(gnome_canvas_item_new (GNOME_CANVAS_GROUP(etgl),
							e_table_item_get_type (),
							"ETableHeader", E_TABLE_GROUP(etgl)->header,
							"ETableModel", etgl->subset,
							"drawgrid", TRUE,
							"drawfocus", TRUE,
							"spreadsheet", TRUE,
							"width", etgl->width,
							NULL));

	gtk_signal_connect (GTK_OBJECT(etgl->item), "row_selection",
			   GTK_SIGNAL_FUNC(etgl_row_selection), etgl);
	e_canvas_item_request_parent_reflow (item);
}

static int
etgl_event (GnomeCanvasItem *item, GdkEvent *event)
{
	gboolean return_val = TRUE;

	switch (event->type) {

	default:
		return_val = FALSE;
	}
	if (return_val == FALSE){
		if (GNOME_CANVAS_ITEM_CLASS(etgl_parent_class)->event)
			return GNOME_CANVAS_ITEM_CLASS(etgl_parent_class)->event (item, event);
	}
	return return_val;

}

static void
etgl_add (ETableGroup *etg, gint row)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	e_table_subset_variable_add (etgl->subset, row);
}

static gboolean
etgl_remove (ETableGroup *etg, gint row)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	return e_table_subset_variable_remove (etgl->subset, row);
}

static void
etgl_increment (ETableGroup *etg, gint position, gint amount)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	e_table_subset_variable_increment (etgl->subset, position, amount);
}

static void
etgl_set_focus (ETableGroup *etg, EFocus direction, gint view_col)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	if (direction == E_FOCUS_END) {
		e_table_item_focus (etgl->item, view_col, e_table_model_row_count(E_TABLE_MODEL(etgl->subset)) - 1);
	} else {
		e_table_item_focus (etgl->item, view_col, 0);
	}
}

static gint
etgl_get_focus_column (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	return e_table_item_get_focused_column (etgl->item);
}

static void
etgl_set_width (ETableGroup *etg, gdouble width)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	etgl->width = width;
#if 0
	if (etgl->item){
		gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
				      "width", width,
				      NULL);
	}		
#endif
}

static gdouble
etgl_get_width (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	gtk_object_get (GTK_OBJECT(etgl->item),
		       "width", &etgl->width,
		       NULL);
	return etgl->width;
}

static gdouble
etgl_get_height (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	gdouble height;
	if (etgl->item)
		gtk_object_get (GTK_OBJECT(etgl->item),
			       "height", &height,
			       NULL);
	else
		height = 1;
	return height;
}

static void
etgl_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableGroup *etg = E_TABLE_GROUP (object);

	switch (arg_id) {
	case ARG_FROZEN:
		if (GTK_VALUE_BOOL (*arg))
			etg->frozen = TRUE;
		else {
			etg->frozen = FALSE;
		}
		break;
	case ARG_WIDTH:
		if (E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->set_width)
			E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->set_width (etg, GTK_VALUE_DOUBLE (*arg));
		break;
	default:
		break;
	}
}

static void
etgl_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableGroup *etg = E_TABLE_GROUP (object);

	switch (arg_id) {
	case ARG_FROZEN:
		GTK_VALUE_BOOL (*arg) = etg->frozen;
		break;
	case ARG_HEIGHT:
		if (E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->get_height)
			GTK_VALUE_DOUBLE (*arg) = E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->get_height (etg);
		else
			arg->type = GTK_TYPE_INVALID;
		break;
	case ARG_WIDTH:	
		if (E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->get_width)
			GTK_VALUE_DOUBLE (*arg) = E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->get_width (etg);
		else
			arg->type = GTK_TYPE_INVALID;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
etgl_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;
	ETableGroupClass *e_group_class = E_TABLE_GROUP_CLASS(object_class);

	object_class->destroy = etgl_destroy;
	object_class->set_arg = etgl_set_arg;
	object_class->get_arg = etgl_get_arg;

	item_class->realize = etgl_realize;
	item_class->event = etgl_event;
	
	etgl_parent_class = gtk_type_class (PARENT_TYPE);

	e_group_class->add = etgl_add;
	e_group_class->remove = etgl_remove;
	e_group_class->increment = etgl_increment;
	e_group_class->set_focus = etgl_set_focus;
	e_group_class->get_focus_column = etgl_get_focus_column;

	e_group_class->get_width = etgl_get_width;
	e_group_class->set_width = etgl_set_width;
	e_group_class->get_height = etgl_get_height;

	gtk_object_add_arg_type ("ETableGroupLeaf::height", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READABLE, ARG_HEIGHT);
	gtk_object_add_arg_type ("ETableGroupLeaf::width", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("ETableGroupLeaf::frozen", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_FROZEN);
}

static void
etgl_init (GtkObject *object)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (object);

	etgl->width = 1;
	etgl->subset = NULL;
	etgl->item = NULL;
	
	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM(object), etgl_reflow);
}

E_MAKE_TYPE (e_table_group_leaf, "ETableGroupLeaf", ETableGroupLeaf, etgl_class_init, etgl_init, PARENT_TYPE);
