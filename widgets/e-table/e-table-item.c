/*
 * E-table-item.c: A view of a Table.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, Helix Code, Inc.
 */
#include <config.h>
#include "e-table-item.h"

#define PARENT_OBJECT_TYPE gnome_canvas_item_get_type ()

static GnomeCanvasItemClass *eti_parent_class;

enum {
	ARG_0,
	ARG_TABLE_HEADER,
	ARG_TABLE_MODEL,
	ARG_TABLE_X,
	ARG_TABLE_Y,
};

static void
eti_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ETableItem *eti = E_TABLE_ITEM (item);
	
	if (GNOME_CANVAS_ITEM_CLASS (eti_parent_class)->update)
		(*GNOME_CANVAS_ITEM_CLASS (eti_parent_class)->update)(item, affine, clip_path, flags);

	item->x1 = eti->x1;
	item->y1 = eti->y1;
	item->x2 = INT_MAX;
	item->y2 = eti->x1 + eti->height;

	gnome_canvas_group_child_bounds (GNOME_CANVAS_GROUP (item->parent), item);
}

static void
eti_remove_table_model (ETableItem *eti)
{
	if (!eti->table_model)
		return;

	gtk_signal_disconnect (eti->table_model_change_id);
	gtk_object_unref (GTK_OBJECT (eti->table_model));

	eti->table_model_change_id = 0;
	eti->table_model = NULL;
}

static void
eti_remove_header_model (ETableItem *eti)
{
	if (!eti->header)
		return;

	gtk_signal_disconnect (eti->header_structure_change_id);
	gtk_signal_disconnect (eti->header_dim_change_id);
	gtk_object_unref (GTK_OBJECT (eti->header));

	eti->header_structure_change_id = 0;
	eti->header_dim_change_id = 0;
	eti->header = NULL;
}

static void
eti_table_model_changed (ETableModel *table_model, ETableItem *eti)
{
	eti->height = e_table_model_height (eti->table_model);

	eti_update (GNOME_CANVAS_ITEM (eti), NULL, NULL, 0);
}

static void
eti_add_table_model (ETableItem *eti, ETableModel *table_model)
{
	g_assert (eti->table_model == NULL);
	
	eti->table_model = table_model;
	gtk_object_ref (GTK_OBJECT (table_model));
	eti->table_model_change_id = gtk_signal_connect (
		GTK_OBJECT (table_model), "model_changed",
		GTK_SIGNAL_FUNC (eti_table_model_changed), eti);
}

static void
eti_header_dim_changed (ETableHeader *eth, int col, ETableItem *eti)
{
	printf ("NOTIFY: Dimension changed");
}

static void
eti_header_structure_changed (ETableHeader *eth, ETableItem *eti)
{
	printf ("NOTIFY: Structure changed");
}

static void
eti_add_header_model (ETableItem *eti, ETableHeader *header)
{
	g_assert (eti->header == NULL);
	
	eti->header = header;
	gtk_object_ref (GTK_OBJECT (header));
	
	eti->header_dim_change_id = gtk_signal_connect (
		GTK_OBJECT (header), "dimension_change",
		GTK_SIGNAL_FUNC (eti_header_dim_changed), eti);

	eti->header_structure_change_id = gtk_signal_connect (
		GTK_OBJECT (header), "structure_changed",
		GTK_SIGNAL_FUNC (eti_header_structure_changed), eti);
}

static void
eti_destroy (GtkObject *object)
{
	ETableItem *eti = E_TABLE_ITEM (object);

	eti_remove_header_model (eti);
	eti_remove_table_model (eti);
	
	if (GTK_OBJECT_CLASS (eti_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (eti_parent_class)->destroy) (object);
}

static void
eti_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ETableItem *eti;
	int v;

	item = GNOME_CANVAS_ITEM (o);
	eti = E_TABLE_ITEM (o);

	switch (arg_id){
	case ARG_TABLE_HEADER:
		eti->header = GTK_VALUE_POINTER (*arg);
		gtk_object_ref (GTK_OBJECT (eti->header));
		break;

	case ARG_TABLE_MODEL:
		eti_remove_table_model (eti);
		eti->height = e_table_model_height (eti->table_model);
		break;
		
	case ARG_TABLE_X:
		eti->x1 = GTK_VALUE_INT (*arg);
		break;

	case ARG_TABLE_Y:
		eti->y1 = GTK_VALUE_INT (*arg);
		break;

	}
	eti_update (item, NULL, NULL, 0);
}

static void
eti_init (GnomeCanvasItem *item)
{
	ETableItem *eti = E_TABLE_ITEM (item);
}

static void
eti_realize (GnomeCanvasItem *item)
{
}

static void
eti_unrealize (GnomeCanvasItem *item)
{
}

static void
eti_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x1, int y1, int width, int height)
{
	
}

static double
eti_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
	   GnomeCanvasItem **actual_item)
{
	*actual_item = item;

	return 0.0;
}

static int
eti_event (GnomeCanvasItem *item, GdkEvent *e)
{
	return FALSE;
}
	
static void
eti_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;

	eti_parent_class = gtk_type_class (PARENT_OBJECT_TYPE);
	
	object_class->destroy = eti_destroy;
	object_class->set_arg = eti_set_arg;

	item_class->update      = eti_update;
	item_class->realize     = eti_realize;
	item_class->unrealize   = eti_unrealize;
	item_class->draw        = eti_draw;
	item_class->point       = eti_point;
	item_class->event       = eti_event;
	
	gtk_object_add_arg_type ("ETableHeaderItem::ETableHeader", GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE, ARG_TABLE_HEADER);
	gtk_object_add_arg_type ("ETableHeaderItem::ETableModel", GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE, ARG_TABLE_MODEL);
	gtk_object_add_arg_type ("ETableHeaderItem::x", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_TABLE_X);
	gtk_object_add_arg_type ("ETableHeaderItem::y", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_TABLE_Y);
}

GtkType
e_table_item_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableItem",
			sizeof (ETableItem),
			sizeof (ETableItemClass),
			(GtkClassInitFunc) eti_class_init,
			(GtkObjectInitFunc) eti_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_OBJECT_TYPE, &info);
	}

	return type;
}

