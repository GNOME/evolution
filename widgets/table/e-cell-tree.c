/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-cell-tree.c - Tree cell renderer
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Chris Toshok <toshok@helixcode.com>
 *
 * A majority of code taken from:
 *
 * the ECellText renderer.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 */

#include <config.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkinvisible.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomeui/gnome-canvas.h>
#include <stdio.h>
#include "e-table-sorted-variable.h"
#include "e-tree-model.h"
#include "e-cell-tree.h"
#include "e-util/e-util.h"
#include "e-table-item.h"

#include <gdk/gdkx.h> /* for BlackPixel */
#include <ctype.h>
#include <math.h>

#define PARENT_TYPE e_cell_get_type ()

typedef struct {
	ECellView    cell_view;
	ECellView   *subcell_view;
	GdkGC       *gc;

	GnomeCanvas *canvas;

} ECellTreeView;

static ECellClass *parent_class;

#define INDENT_AMOUNT 16

static int
visible_depth_of_node (ETreeModel *tree_model, ETreePath *path)
{
	return (e_tree_model_node_depth (tree_model, path) 
		- (e_tree_model_root_node_is_visible (tree_model) ? 0 : 1));
}

static gint
offset_of_node (ETreeModel *tree_model, ETreePath *path)
{
	return (visible_depth_of_node(tree_model, path) + 1) * INDENT_AMOUNT;
}

static ETreePath*
e_cell_tree_get_node (ETreeModel *tree_model, int row)
{
	return (ETreePath*)e_table_model_value_at (E_TABLE_MODEL(tree_model), -1, row);
}

static ETreeModel*
e_cell_tree_get_tree_model (ETableModel *table_model, int row)
{
	return (ETreeModel*)e_table_model_value_at (table_model, -2, row);
}

/*
 * ECell::new_view method
 */
static ECellView *
ect_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	ECellTree *ect = E_CELL_TREE (ecell);
	ECellTreeView *tree_view = g_new0 (ECellTreeView, 1);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (e_table_item_view)->canvas;
	
	tree_view->cell_view.ecell = ecell;
	tree_view->cell_view.e_table_model = table_model;
	tree_view->cell_view.e_table_item_view = e_table_item_view;
	
	/* create our subcell view */
	tree_view->subcell_view = e_cell_new_view (ect->subcell, table_model, e_table_item_view /* XXX */);

	tree_view->canvas = canvas;

	return (ECellView *)tree_view;
}

/*
 * ECell::kill_view method
 */
static void
ect_kill_view (ECellView *ecv)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecv;

	/* kill our subcell view */
	e_cell_kill_view (tree_view->subcell_view);

	g_free (tree_view);
}

/*
 * ECell::realize method
 */
static void
ect_realize (ECellView *ecell_view)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	/* realize our subcell view */
	e_cell_realize (tree_view->subcell_view);

	tree_view->gc = gdk_gc_new (GTK_WIDGET (tree_view->canvas)->window);

	gdk_gc_set_line_attributes (tree_view->gc, 1, 
				    GDK_LINE_ON_OFF_DASH, None, None);
	gdk_gc_set_dashes (tree_view->gc, 0, "\1\1", 2);

	if (parent_class->realize)
		(* parent_class->realize) (ecell_view);
}

/*
 * ECell::unrealize method
 */
static void
ect_unrealize (ECellView *ecv)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecv;

	/* unrealize our subcell view. */
	e_cell_unrealize (tree_view->subcell_view);

	gdk_gc_unref (tree_view->gc);
	tree_view->gc = NULL;

	if (parent_class->unrealize)
		(* parent_class->unrealize) (ecv);
}

/*
 * ECell::draw method
 */
static void
ect_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int model_col, int view_col, int row, gboolean selected,
	  int x1, int y1, int x2, int y2)
{
	ECellTreeView *tree_view = (ECellTreeView *)ecell_view;
	ETreeModel *tree_model = e_cell_tree_get_tree_model(ecell_view->e_table_model, row);
	ETreePath *node;
	GdkRectangle rect, *clip_rect;
	GtkWidget *canvas = GTK_WIDGET (tree_view->canvas);
	GdkGC *fg_gc = canvas->style->fg_gc[GTK_STATE_ACTIVE];
	GdkColor *background, *foreground;

	int offset, subcell_offset;
	gboolean expanded, expandable;

	/* only draw the tree effects if we're the active sort */
	if (/* XXX */ TRUE) {
		GdkPixbuf *node_image;
		int node_image_width = 0, node_image_height = 0;
		ETreePath *parent_node;

		node = e_cell_tree_get_node (tree_model, row);

		offset = offset_of_node (tree_model, node);
		expandable = e_tree_model_node_is_expandable (tree_model, node);
		expanded = e_tree_model_node_is_expanded (tree_model, node);
		subcell_offset = offset;

		if (expanded)
			node_image = e_tree_model_node_get_opened_pixbuf (tree_model, node);
		else
			node_image = e_tree_model_node_get_closed_pixbuf (tree_model, node);

		if (node_image) {
			node_image_width = gdk_pixbuf_get_width (node_image);
			node_image_height = gdk_pixbuf_get_height (node_image);
		}

		/*
		 * Be a nice citizen: clip to the region we are supposed to draw on
		 */
		rect.x = x1;
		rect.y = y1;
		rect.width = subcell_offset + node_image_width;
		rect.height = y2 - y1;
	
		gdk_gc_set_clip_rectangle (tree_view->gc, &rect);
		gdk_gc_set_clip_rectangle (fg_gc, &rect);
		clip_rect = &rect;

		if (selected){
			background = &canvas->style->bg [GTK_STATE_SELECTED];
			foreground = &canvas->style->text [GTK_STATE_SELECTED];
		} else {
			background = &canvas->style->base [GTK_STATE_NORMAL];
			foreground = &canvas->style->text [GTK_STATE_NORMAL];
		}
		gdk_gc_set_foreground (tree_view->gc, background);
		gdk_draw_rectangle (drawable, tree_view->gc, TRUE,
				    rect.x, rect.y, rect.width, rect.height);
		gdk_gc_set_foreground (tree_view->gc, foreground);

		/* draw our lines */
		if (E_CELL_TREE(tree_view->cell_view.ecell)->draw_lines) {

			if (!e_tree_model_node_is_root (tree_model, node)
			    || e_tree_model_node_get_children (tree_model, node, NULL) > 0)
				gdk_draw_line (drawable, tree_view->gc,
					       rect.x + offset - INDENT_AMOUNT / 2 + 1,
					       rect.y + rect.height / 2,
					       rect.x + offset,
					       rect.y + rect.height / 2);

			if (visible_depth_of_node (tree_model, node) != 0) {
				gdk_draw_line (drawable, tree_view->gc,
					       rect.x + offset - INDENT_AMOUNT / 2,
					       rect.y,
					       rect.x + offset - INDENT_AMOUNT / 2,
					       (e_tree_model_node_get_next (tree_model, node)
						? rect.y + rect.height
						: rect.y + rect.height / 2));
			}

			/* now traverse back up to the root of the tree, checking at
			   each level if the node has siblings, and drawing the
			   correct vertical pipe for it's configuration. */
			parent_node = e_tree_model_node_get_parent (tree_model, node);
			offset -= INDENT_AMOUNT;
			while (parent_node && visible_depth_of_node (tree_model, parent_node) != 0) {
				if (e_tree_model_node_get_next(tree_model, parent_node)) {
					gdk_draw_line (drawable, tree_view->gc,
						       rect.x + offset - INDENT_AMOUNT / 2,
						       rect.y,
						       rect.x + offset - INDENT_AMOUNT / 2,
						       rect.y + rect.height);
				}
				parent_node = e_tree_model_node_get_parent (tree_model, parent_node);
				offset -= INDENT_AMOUNT;
			}
		}

		/* now draw our icon if we're expandable */
		if (expandable) {
			GdkPixbuf *image;
			int image_width, image_height;

			image = (expanded 
				 ? E_CELL_TREE(tree_view->cell_view.ecell)->open_pixbuf
				 : E_CELL_TREE(tree_view->cell_view.ecell)->closed_pixbuf);

			image_width = gdk_pixbuf_get_width(image);
			image_height = gdk_pixbuf_get_height(image);

			gdk_pixbuf_render_to_drawable_alpha (image,
							     drawable,
							     0, 0,
							     x1 + subcell_offset - INDENT_AMOUNT / 2 - image_width / 2,
							     y1 + (y2 - y1) / 2 - image_height / 2,
							     image_width, image_height,
							     GDK_PIXBUF_ALPHA_BILEVEL,
							     128,
							     GDK_RGB_DITHER_NORMAL,
							     image_width, 0);
		}

		if (node_image) {
			gdk_pixbuf_render_to_drawable_alpha (node_image,
							     drawable,
							     0, 0,
							     x1 + subcell_offset,
							     y1 + (y2 - y1) / 2 - node_image_height / 2,
							     node_image_width, node_image_height,
							     GDK_PIXBUF_ALPHA_BILEVEL,
							     128,
							     GDK_RGB_DITHER_NORMAL,
							     node_image_width, 0);
			subcell_offset += node_image_width;
		}
	}

	/* Now cause our subcell to draw its contents, shifted by
	   subcell_offset pixels */
	e_cell_draw (tree_view->subcell_view, drawable,
		     model_col, view_col, row, selected,
		     x1 + subcell_offset, y1, x2, y2);
}

/*
 * ECell::event method
 */
static gint
ect_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
	ETreePath *node = e_cell_tree_get_node (tree_model, row);
	int offset = offset_of_node (tree_model, node);

	switch (event->type) {
	case GDK_BUTTON_PRESS: {
		/* if the event happened in our area of control (and
                   we care about it), handle it. */

		/* only activate the tree control if the click/release happens in the icon's area. */
		if (event->button.x > (offset - INDENT_AMOUNT) && event->button.x < offset) {
			if (e_tree_model_node_is_expandable (tree_model, node)) {
				e_tree_model_node_set_expanded (tree_model,
								node,
								!e_tree_model_node_is_expanded(tree_model, node));
			}
			return TRUE;
		}
		else if (event->button.x < (offset - INDENT_AMOUNT))
			return TRUE;
	}
	default:
		/* modify the event and pass it off to our subcell_view */
		switch (event->type) {
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			event->button.x -= offset;
			break;
		case GDK_MOTION_NOTIFY:
			event->motion.x -= offset;
			break;
		default:
			/* nada */
		}
		e_cell_event(tree_view->subcell_view, event, model_col, view_col, row);
		return TRUE;
	}
}

/*
 * ECell::height method
 */
static int
ect_height (ECellView *ecell_view, int model_col, int view_col, int row) 
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	return e_cell_height (tree_view->subcell_view, model_col, view_col, row);
}

/*
 * ECellView::enter_edit method
 */
static void *
ect_enter_edit (ECellView *ecell_view, int model_col, int view_col, int row)
{
	/* just defer to our subcell's view */
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	return e_cell_enter_edit (tree_view->subcell_view, model_col, view_col, row);
}

/*
 * ECellView::leave_edit method
 */
static void
ect_leave_edit (ECellView *ecell_view, int model_col, int view_col, int row, void *edit_context)
{
	/* just defer to our subcell's view */
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	e_cell_leave_edit (tree_view->subcell_view, model_col, view_col, row, edit_context);
}

static void
ect_print (ECellView *ecell_view, GnomePrintContext *context, 
	   int model_col, int view_col, int row,
	   double width, double height)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	if (/* XXX only if we're the active sort */ TRUE) {
		ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
		ETreePath *node = e_cell_tree_get_node (tree_model, row);
		int offset = offset_of_node (tree_model, node);
		int subcell_offset = offset;
		gboolean expandable = e_tree_model_node_is_expandable (tree_model, node);
		gboolean expanded = e_tree_model_node_is_expanded (tree_model, node);

		/* draw our lines */
		if (E_CELL_TREE(tree_view->cell_view.ecell)->draw_lines) {

			if (!e_tree_model_node_is_root (tree_model, node)
			    || e_tree_model_node_get_children (tree_model, node, NULL) > 0) {
				gnome_print_moveto (context,
						    offset - INDENT_AMOUNT / 2,
						    height / 2);

				gnome_print_lineto (context,
						    offset,
						    height / 2);
			}

			if (visible_depth_of_node (tree_model, node) != 0) {
				gnome_print_moveto (context,
						    offset - INDENT_AMOUNT / 2,
						    height);
				gnome_print_lineto (context,
						    offset - INDENT_AMOUNT / 2,
						    (e_tree_model_node_get_next (tree_model, node)
						     ? 0
						     : height / 2));
			}

			/* now traverse back up to the root of the tree, checking at
			   each level if the node has siblings, and drawing the
			   correct vertical pipe for it's configuration. */
			node = e_tree_model_node_get_parent (tree_model, node);
			offset -= INDENT_AMOUNT;
			while (node && visible_depth_of_node (tree_model, node) != 0) {
				if (e_tree_model_node_get_next(tree_model, node)) {
					gnome_print_moveto (context,
							    offset - INDENT_AMOUNT / 2,
							    height);
					gnome_print_lineto (context,
							    offset - INDENT_AMOUNT / 2,
							    0);
				}
				node = e_tree_model_node_get_parent (tree_model, node);
				offset -= INDENT_AMOUNT;
			}
		}

		/* now draw our icon if we're expandable */
		if (expandable) {
			double image_matrix [6] = {16, 0, 0, 16, 0, 0};
			GdkPixbuf *image = (expanded 
					    ? E_CELL_TREE(tree_view->cell_view.ecell)->open_pixbuf
					    : E_CELL_TREE(tree_view->cell_view.ecell)->closed_pixbuf);
			int image_width, image_height, image_rowstride;
			guchar *image_pixels;

			image_width = gdk_pixbuf_get_width(image);
			image_height = gdk_pixbuf_get_height(image);
			image_pixels = gdk_pixbuf_get_pixels(image);
			image_rowstride = gdk_pixbuf_get_rowstride(image);

			image_matrix [4] = subcell_offset - INDENT_AMOUNT / 2 - image_width / 2;
			image_matrix [5] = height / 2 - image_height / 2;

			gnome_print_gsave (context);
			gnome_print_concat (context, image_matrix);

			gnome_print_rgbaimage (context, image_pixels, image_width, image_height, image_rowstride);
			gnome_print_grestore (context);
		}

		gnome_print_stroke (context);

		if (gnome_print_translate(context, subcell_offset, 0) == -1)
				/* FIXME */;
		width -= subcell_offset;
	}


	e_cell_print (tree_view->subcell_view, context, model_col, view_col, row, width, height);
}

static gdouble
ect_print_height (ECellView *ecell_view, GnomePrintContext *context, 
		  int model_col, int view_col, int row,
		  double width)
{
	return 12; /* XXX */
}

/*
 * GtkObject::destroy method
 */
static void
ect_destroy (GtkObject *object)
{
	ECellTree *ect = E_CELL_TREE (object);

	/* destroy our subcell */
	gtk_object_destroy (GTK_OBJECT (ect->subcell));

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
e_cell_tree_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	object_class->destroy = ect_destroy;

	ecc->new_view   = ect_new_view;
	ecc->kill_view  = ect_kill_view;
	ecc->realize    = ect_realize;
	ecc->unrealize  = ect_unrealize;
	ecc->draw       = ect_draw;
	ecc->event      = ect_event;
	ecc->height     = ect_height;
	ecc->enter_edit = ect_enter_edit;
	ecc->leave_edit = ect_leave_edit;
	ecc->print      = ect_print;
	ecc->print_height = ect_print_height;

	parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_cell_tree, "ECellTree", ECellTree, e_cell_tree_class_init, NULL, PARENT_TYPE);

void
e_cell_tree_construct (ECellTree *ect,
		       GdkPixbuf *open_pixbuf,
		       GdkPixbuf *closed_pixbuf,
		       gboolean draw_lines,
		       ECell *subcell)
{		       
	ect->subcell = subcell;
	ect->open_pixbuf = open_pixbuf;
	ect->closed_pixbuf = closed_pixbuf;
	ect->draw_lines = draw_lines;
}


ECell *
e_cell_tree_new (ETableModel *etm,
		 GdkPixbuf *open_pixbuf,
		 GdkPixbuf *closed_pixbuf,
		 gboolean draw_lines,
		 ECell *subcell)
{
	ECellTree *ect = gtk_type_new (e_cell_tree_get_type ());

	e_cell_tree_construct (ect, open_pixbuf, closed_pixbuf, draw_lines, subcell);

	return (ECell *) ect;
}
