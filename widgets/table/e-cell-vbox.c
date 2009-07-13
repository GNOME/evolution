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
 *		Chris Toshok <toshok@ximian.com>
 *		Chris Lahey  <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "a11y/e-table/gal-a11y-e-cell-registry.h"
#include "a11y/e-table/gal-a11y-e-cell-vbox.h"
#include "e-util/e-util.h"

#include "e-cell-vbox.h"
#include "e-table-item.h"

G_DEFINE_TYPE (ECellVbox, e_cell_vbox, E_CELL_TYPE)

#define INDENT_AMOUNT 16

/*
 * ECell::new_view method
 */
static ECellView *
ecv_new_view (ECell *ecell, ETableModel *table_model, gpointer e_table_item_view)
{
	ECellVbox *ecv = E_CELL_VBOX (ecell);
	ECellVboxView *vbox_view = g_new0 (ECellVboxView, 1);
	gint i;

	vbox_view->cell_view.ecell = ecell;
	vbox_view->cell_view.e_table_model = table_model;
	vbox_view->cell_view.e_table_item_view = e_table_item_view;
        vbox_view->cell_view.kill_view_cb = NULL;
        vbox_view->cell_view.kill_view_cb_data = NULL;

	/* create our subcell view */
	vbox_view->subcell_view_count = ecv->subcell_count;
	vbox_view->subcell_views = g_new (ECellView *, vbox_view->subcell_view_count);
	vbox_view->model_cols = g_new (int, vbox_view->subcell_view_count);

	for (i = 0; i < vbox_view->subcell_view_count; i++) {
		vbox_view->subcell_views[i] = e_cell_new_view (ecv->subcells[i], table_model, e_table_item_view /* XXX */);
		vbox_view->model_cols[i] = ecv->model_cols[i];
	}

	return (ECellView *)vbox_view;
}

/*
 * ECell::kill_view method
 */
static void
ecv_kill_view (ECellView *ecv)
{
	ECellVboxView *vbox_view = (ECellVboxView *) ecv;
	gint i;

        if (vbox_view->cell_view.kill_view_cb)
		(vbox_view->cell_view.kill_view_cb)(ecv, vbox_view->cell_view.kill_view_cb_data);

        if (vbox_view->cell_view.kill_view_cb_data)
            g_list_free(vbox_view->cell_view.kill_view_cb_data);

	/* kill our subcell view */
	for (i = 0; i < vbox_view->subcell_view_count; i++)
		e_cell_kill_view (vbox_view->subcell_views[i]);

	g_free (vbox_view->model_cols);
	g_free (vbox_view->subcell_views);
	g_free (vbox_view);
}

/*
 * ECell::realize method
 */
static void
ecv_realize (ECellView *ecell_view)
{
	ECellVboxView *vbox_view = (ECellVboxView *) ecell_view;
	gint i;

	/* realize our subcell view */
	for (i = 0; i < vbox_view->subcell_view_count; i++)
		e_cell_realize (vbox_view->subcell_views[i]);

	if (E_CELL_CLASS (e_cell_vbox_parent_class)->realize)
		(* E_CELL_CLASS (e_cell_vbox_parent_class)->realize) (ecell_view);
}

/*
 * ECell::unrealize method
 */
static void
ecv_unrealize (ECellView *ecv)
{
	ECellVboxView *vbox_view = (ECellVboxView *) ecv;
	gint i;

	/* unrealize our subcell view. */
	for (i = 0; i < vbox_view->subcell_view_count; i++)
		e_cell_unrealize (vbox_view->subcell_views[i]);

	if (E_CELL_CLASS (e_cell_vbox_parent_class)->unrealize)
		(* E_CELL_CLASS (e_cell_vbox_parent_class)->unrealize) (ecv);
}

/*
 * ECell::draw method
 */
static void
ecv_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  gint model_col, gint view_col, gint row, ECellFlags flags,
	  gint x1, gint y1, gint x2, gint y2)
{
	ECellVboxView *vbox_view = (ECellVboxView *)ecell_view;

	gint subcell_offset = 0;
	gint i;

	for (i = 0; i < vbox_view->subcell_view_count; i++) {
		/* Now cause our subcells to draw their contents,
		   shifted by subcell_offset pixels */
		gint height = e_cell_height (vbox_view->subcell_views[i], vbox_view->model_cols[i], view_col, row);
		e_cell_draw (vbox_view->subcell_views[i], drawable,
			     vbox_view->model_cols[i], view_col, row, flags,
			     x1, y1 + subcell_offset, x2, y1 + subcell_offset + height);

		subcell_offset += e_cell_height (vbox_view->subcell_views[i], vbox_view->model_cols[i], view_col, row);
	}
}

/*
 * ECell::event method
 */
static gint
ecv_event (ECellView *ecell_view, GdkEvent *event, gint model_col, gint view_col, gint row, ECellFlags flags, ECellActions *actions)
{
	ECellVboxView *vbox_view = (ECellVboxView *)ecell_view;
	gint y = 0;
	gint i;
	gint subcell_offset = 0;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		y = event->button.y;
		break;
	case GDK_MOTION_NOTIFY:
		y = event->motion.y;
		break;
	default:
		/* nada */
		break;
	}

	for (i = 0; i < vbox_view->subcell_view_count; i++) {
		gint height = e_cell_height (vbox_view->subcell_views[i], vbox_view->model_cols[i], view_col, row);
		if (y < subcell_offset + height)
			return e_cell_event(vbox_view->subcell_views[i], event, vbox_view->model_cols[i], view_col, row, flags, actions);
		subcell_offset += height;
	}
	return 0;
}

/*
 * ECell::height method
 */
static gint
ecv_height (ECellView *ecell_view, gint model_col, gint view_col, gint row)
{
	ECellVboxView *vbox_view = (ECellVboxView *)ecell_view;
	gint height = 0;
	gint i;

	for (i = 0; i < vbox_view->subcell_view_count; i++) {
		height += e_cell_height (vbox_view->subcell_views[i], vbox_view->model_cols[i], view_col, row);
	}
	return height;
}

/*
 * ECell::max_width method
 */
static gint
ecv_max_width (ECellView *ecell_view, gint model_col, gint view_col)
{
	ECellVboxView *vbox_view = (ECellVboxView *)ecell_view;
	gint max_width = 0;
	gint i;

	for (i = 0; i < vbox_view->subcell_view_count; i++) {
		gint width = e_cell_max_width (vbox_view->subcell_views[i], vbox_view->model_cols[i], view_col);
		max_width = MAX(width, max_width);
	}

	return max_width;
}

#if 0
/*
 * ECellView::show_tooltip method
 */
static void
ecv_show_tooltip (ECellView *ecell_view, gint model_col, gint view_col, gint row,
		  gint col_width, ETableTooltip *tooltip)
{
	ECellVboxView *vbox_view = (ECellVboxView *) ecell_view;
	EVboxModel *vbox_model = e_cell_vbox_get_vbox_model (ecell_view->e_table_model, row);
	EVboxPath node = e_cell_vbox_get_node (ecell_view->e_table_model, row);
	gint offset = offset_of_node (ecell_view->e_table_model, row);
	GdkPixbuf *node_image;

	node_image = e_vbox_model_icon_at (vbox_model, node);
	if (node_image)
		offset += gdk_pixbuf_get_width (node_image);

	tooltip->x += offset;
	e_cell_show_tooltip (vbox_view->subcell_view, model_col, view_col, row, col_width - offset, tooltip);
}

/*
 * ECellView::get_bg_color method
 */
static gchar *
ecv_get_bg_color (ECellView *ecell_view, gint row)
{
	ECellVboxView *vbox_view = (ECellVboxView *) ecell_view;

	return e_cell_get_bg_color (vbox_view->subcell_views[0], row);
}

/*
 * ECellView::enter_edit method
 */
static gpointer
ecv_enter_edit (ECellView *ecell_view, gint model_col, gint view_col, gint row)
{
	/* just defer to our subcell's view */
	ECellVboxView *vbox_view = (ECellVboxView *) ecell_view;

	return e_cell_enter_edit (vbox_view->subcell_view, model_col, view_col, row);
}

/*
 * ECellView::leave_edit method
 */
static void
ecv_leave_edit (ECellView *ecell_view, gint model_col, gint view_col, gint row, gpointer edit_context)
{
	/* just defer to our subcell's view */
	ECellVboxView *vbox_view = (ECellVboxView *) ecell_view;

	e_cell_leave_edit (vbox_view->subcell_view, model_col, view_col, row, edit_context);
}

static void
ecv_print (ECellView *ecell_view, GnomePrintContext *context,
	   gint model_col, gint view_col, gint row,
	   double width, double height)
{
	ECellVboxView *vbox_view = (ECellVboxView *) ecell_view;

	if (/* XXX only if we're the active sort */ TRUE) {
		EVboxModel *vbox_model = e_cell_vbox_get_vbox_model (ecell_view->e_table_model, row);
		EVboxTableAdapter *vbox_table_adapter = e_cell_vbox_get_vbox_table_adapter(ecell_view->e_table_model, row);
		EVboxPath node = e_cell_vbox_get_node (ecell_view->e_table_model, row);
		gint offset = offset_of_node (ecell_view->e_table_model, row);
		gint subcell_offset = offset;
		gboolean expandable = e_vbox_model_node_is_expandable (vbox_model, node);
		gboolean expanded = e_vbox_table_adapter_node_is_expanded (vbox_table_adapter, node);

		/* draw our lines */
		if (E_CELL_VBOX(vbox_view->cell_view.ecell)->draw_lines) {
			gint depth;

			if (!e_vbox_model_node_is_root (vbox_model, node)
			    || e_vbox_model_node_get_children (vbox_model, node, NULL) > 0) {
				gnome_print_moveto (context,
						    offset - INDENT_AMOUNT / 2,
						    height / 2);

				gnome_print_lineto (context,
						    offset,
						    height / 2);
			}

			if (visible_depth_of_node (ecell_view->e_table_model, row) != 0) {
				gnome_print_moveto (context,
						    offset - INDENT_AMOUNT / 2,
						    height);
				gnome_print_lineto (context,
						    offset - INDENT_AMOUNT / 2,
						    (e_vbox_model_node_get_next (vbox_model, node)
						     ? 0
						     : height / 2));
			}

			/* now traverse back up to the root of the vbox, checking at
			   each level if the node has siblings, and drawing the
			   correct vertical pipe for it's configuration. */
			node = e_vbox_model_node_get_parent (vbox_model, node);
			depth = visible_depth_of_node (ecell_view->e_table_model, row) - 1;
			offset -= INDENT_AMOUNT;
			while (node && depth != 0) {
				if (e_vbox_model_node_get_next(vbox_model, node)) {
					gnome_print_moveto (context,
							    offset - INDENT_AMOUNT / 2,
							    height);
					gnome_print_lineto (context,
							    offset - INDENT_AMOUNT / 2,
							    0);
				}
				node = e_vbox_model_node_get_parent (vbox_model, node);
				depth --;
				offset -= INDENT_AMOUNT;
			}
		}

		/* now draw our icon if we're expandable */
		if (expandable) {
			double image_matrix [6] = {16, 0, 0, 16, 0, 0};
			GdkPixbuf *image = (expanded
					    ? E_CELL_VBOX(vbox_view->cell_view.ecell)->open_pixbuf
					    : E_CELL_VBOX(vbox_view->cell_view.ecell)->closed_pixbuf);
			gint image_width, image_height, image_rowstride;
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

	e_cell_print (vbox_view->subcell_view, context, model_col, view_col, row, width, height);
}

static gdouble
ecv_print_height (ECellView *ecell_view, GnomePrintContext *context,
		  gint model_col, gint view_col, gint row,
		  double width)
{
	return 12; /* XXX */
}
#endif

/*
 * GObject::dispose method
 */
static void
ecv_dispose (GObject *object)
{
	ECellVbox *ecv = E_CELL_VBOX (object);
	gint i;

	/* destroy our subcell */
	for (i = 0; i < ecv->subcell_count; i++)
		if (ecv->subcells[i])
			g_object_unref (ecv->subcells[i]);
	g_free (ecv->subcells);
	ecv->subcells = NULL;
	ecv->subcell_count = 0;

	G_OBJECT_CLASS (e_cell_vbox_parent_class)->dispose (object);
}

static void
ecv_finalize (GObject *object)
{
	ECellVbox *ecv = E_CELL_VBOX (object);

	g_free (ecv->model_cols);

	G_OBJECT_CLASS (e_cell_vbox_parent_class)->finalize (object);
}

static void
e_cell_vbox_class_init (ECellVboxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ECellClass *ecc = E_CELL_CLASS (klass);

	object_class->dispose = ecv_dispose;
	object_class->finalize = ecv_finalize;

	ecc->new_view         = ecv_new_view;
	ecc->kill_view        = ecv_kill_view;
	ecc->realize          = ecv_realize;
	ecc->unrealize        = ecv_unrealize;
	ecc->draw             = ecv_draw;
	ecc->event            = ecv_event;
	ecc->height           = ecv_height;
#if 0
	ecc->enter_edit       = ecv_enter_edit;
	ecc->leave_edit       = ecv_leave_edit;
	ecc->print            = ecv_print;
	ecc->print_height     = ecv_print_height;
#endif
	ecc->max_width        = ecv_max_width;
#if 0
	ecc->show_tooltip     = ecv_show_tooltip;
	ecc->get_bg_color     = ecv_get_bg_color;
#endif

	gal_a11y_e_cell_registry_add_cell_type (NULL, E_CELL_VBOX_TYPE, gal_a11y_e_cell_vbox_new);
}

static void
e_cell_vbox_init (ECellVbox *ecv)
{
	ecv->subcells = NULL;
	ecv->subcell_count = 0;
}

/**
 * e_cell_vbox_new:
 *
 * Creates a new ECell renderer that can be used to render multiple
 * child cells.
 *
 * Return value: an ECell object that can be used to render multiple
 * child cells.
 **/
ECell *
e_cell_vbox_new (void)
{
	ECellVbox *ecv = g_object_new (E_CELL_VBOX_TYPE, NULL);

	return (ECell *) ecv;
}

void
e_cell_vbox_append (ECellVbox *vbox, ECell *subcell, gint model_col)
{
	vbox->subcell_count ++;

	vbox->subcells   = g_renew (ECell *, vbox->subcells,   vbox->subcell_count);
	vbox->model_cols = g_renew (int,     vbox->model_cols, vbox->subcell_count);

	vbox->subcells[vbox->subcell_count - 1]   = subcell;
	vbox->model_cols[vbox->subcell_count - 1] = model_col;

	if (subcell)
		g_object_ref (subcell);
}
