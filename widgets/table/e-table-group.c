/*
 * E-Table-Group.c: Implements the grouping objects for elements on a table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org()
 *
 * Copyright 1999, Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-group.h"
#include "e-table-item.h"
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "e-util/e-util.h"

#define TITLE_HEIGHT         16
#define GROUP_INDENT         10

#define PARENT_TYPE gnome_canvas_group_get_type ()

static GnomeCanvasGroupClass *etg_parent_class;

enum {
	HEIGHT_CHANGED,
	LAST_SIGNAL
};

static gint etg_signals [LAST_SIGNAL] = { 0, };

static void
etg_destroy (GtkObject *object)
{
	ETableGroup *etg = E_TABLE_GROUP (object);
	
	GTK_OBJECT_CLASS (etg_parent_class)->destroy (object);
}

static void
etg_dim (ETableGroup *etg, int *width, int *height)
{
	GSList *l;

	*width = *height = 0;
	
	for (l = etg->children; l; l = l->next){
		GnomeCanvasItem *child = l->data;

		*height += child->y2 - child->y1;
		*width  += child->x2 - child->x1;
	}

	if (!etg->transparent){
		*height += TITLE_HEIGHT;
		*width  += GROUP_INDENT;
	}
}

void
e_table_group_construct (GnomeCanvasGroup *parent, ETableGroup *etg, 
			 ETableCol *ecol, gboolean open,
			 gboolean transparent)
{
	gnome_canvas_item_constructv (GNOME_CANVAS_ITEM (etg), parent, 0, NULL);
	
	etg->ecol = ecol;
	etg->open = open;
	etg->transparent = transparent;
	
	etg_dim (etg, &etg->width, &etg->height);

	if (!etg->transparent)
		etg->rect = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (etg),
			gnome_canvas_rect_get_type (),
			"fill_color", "gray",
			"outline_color", "gray20",
			"x1", 0.0,
			"y1", 0.0,
			"x2", (double) etg->width,
			"y2", (double) etg->height,
			NULL);

#if 0
	/*
	 * Reparent the child into our space.
	 */
	gnome_canvas_item_reparent (child, GNOME_CANVAS_GROUP (etg));

	gnome_canvas_item_set (
		child,
		"x", (double) GROUP_INDENT,
		"y", (double) TITLE_HEIGHT,
		NULL);

	/*
	 * Force dimension computation
	 */
	GNOME_CANVAS_ITEM_CLASS (etg_parent_class)->update (
		GNOME_CANVAS_ITEM (etg), NULL, NULL, GNOME_CANVAS_UPDATE_REQUESTED);
#endif
}

GnomeCanvasItem *
e_table_group_new (GnomeCanvasGroup *parent, ETableCol *ecol, 
		   gboolean open, gboolean transparent)
{
	ETableGroup *etg;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (ecol != NULL, NULL);
	
	etg = gtk_type_new (e_table_group_get_type ());

	e_table_group_construct (parent, etg, ecol, open, transparent);

	return GNOME_CANVAS_ITEM (etg);
}

static void
etg_relayout (GnomeCanvasItem *eti, ETableGroup *etg)
{
	GSList *l;
	int height = etg->transparent ? 0 : GROUP_INDENT;
	gboolean move = FALSE;
	
	printf ("Relaying out\n");
	
	for (l = etg->children; l->next; l = l->next){
		GnomeCanvasItem *child = l->data;

		height += child->y2 - child->y1;

		if (child == eti)
			move = TRUE;

		if (move){
			printf ("Moving item %p\n", child);
			gnome_canvas_item_set (
				child,
				"y", (double) height,
				NULL);
		}
	}
	if (height != etg->height){
		etg->height = height;
		gtk_signal_emit (GTK_OBJECT (etg), etg_signals [HEIGHT_CHANGED]);
	}
}

void
e_table_group_add (ETableGroup *etg, GnomeCanvasItem *item)
{
	double x1, y1, x2, y2;
	
	g_return_if_fail (etg != NULL);
	g_return_if_fail (item != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	etg->children = g_slist_append (etg->children, item);

	GNOME_CANVAS_ITEM_CLASS (GTK_OBJECT (etg)->klass)->bounds (etg, &x1, &y1, &x2, &y2);
		
	if (GTK_OBJECT (etg)->flags & GNOME_CANVAS_ITEM_REALIZED){
		GSList *l;
		int height = etg->transparent ? 0 : TITLE_HEIGHT;
		int x = etg->transparent ? 0 : GROUP_INDENT;
		
		for (l = etg->children; l->next; l = l->next){
			GnomeCanvasItem *child = l->data;

			height += child->y2 - child->y1;
		}

		printf ("Positioning item %p at %d\n", item, height);
		gnome_canvas_item_set (
			item,
			"y", (double) height,
			"x", (double) x,
			NULL);

		
		if (E_IS_TABLE_ITEM (item)){
			
			printf ("Table item! ---------\n");
			gtk_signal_connect (GTK_OBJECT (item), "height_changed",
					    GTK_SIGNAL_FUNC (etg_relayout), etg);
		}
	}
}

static void
etg_realize (GnomeCanvasItem *item)
{
	ETableGroup *etg = E_TABLE_GROUP (item);
	GSList *l;
	int height = 0;
	
	GNOME_CANVAS_ITEM_CLASS (etg_parent_class)->realize (item);

	for (l = etg->children; l; l = l->next){
		GnomeCanvasItem *child = l->data;

		printf ("During realization for child %p -> %d\n", child, height);
		gnome_canvas_item_set (
			child,
			"y", (double) height,
			NULL);

		height += child->y2 - child->y1;
	}
}

static void
etg_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ETableGroup *etg = E_TABLE_GROUP (item);

	GNOME_CANVAS_ITEM_CLASS (etg_parent_class)->update (item, affine, clip_path, flags);

	if (!etg->transparent){
		int current_width, current_height;

		etg_dim (etg, &current_width, &current_height);
		
		if ((current_height != etg->height) || (current_width != etg->width)){
			etg->width = current_width;
			etg->height = current_height;
			
			gnome_canvas_item_set (
				etg->rect,
				"x1", 0.0,
				"y1", 0.0,
				"x2", (double) etg->width,
				"y2", (double) etg->height,
				NULL);
		}
	}
}

static void
etg_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;

	object_class->destroy = etg_destroy;

	item_class->realize = etg_realize;
	item_class->update = etg_update;
	
	etg_parent_class = gtk_type_class (PARENT_TYPE);

	etg_signals [HEIGHT_CHANGED] =
		gtk_signal_new ("height_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableGroupClass, height_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals (object_class, etg_signals, LAST_SIGNAL);
	
}

E_MAKE_TYPE (e_table_group, "ETableGroup", ETableGroup, etg_class_init, NULL, PARENT_TYPE);



