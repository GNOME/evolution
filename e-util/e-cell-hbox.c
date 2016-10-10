/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * A majority of code taken from:
 *
 * the ECellText renderer.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#include <gtk/gtk.h>

/* #include "a11y/gal-a11y-e-cell-registry.h" */
/* #include "a11y/gal-a11y-e-cell-vbox.h" */

#include "e-cell-hbox.h"
#include "e-table-item.h"

G_DEFINE_TYPE (ECellHbox, e_cell_hbox, E_TYPE_CELL)

#define INDENT_AMOUNT 16
#define MAX_CELL_SIZE 25

/*
 * ECell::new_view method
 */
static ECellView *
ecv_new_view (ECell *ecell,
              ETableModel *table_model,
              gpointer e_table_item_view)
{
	ECellHbox *ecv = E_CELL_HBOX (ecell);
	ECellHboxView *hbox_view = g_new0 (ECellHboxView, 1);
	gint i;

	hbox_view->cell_view.ecell = ecell;
	hbox_view->cell_view.e_table_model = table_model;
	hbox_view->cell_view.e_table_item_view = e_table_item_view;
	hbox_view->cell_view.kill_view_cb = NULL;
	hbox_view->cell_view.kill_view_cb_data = NULL;

	/* create our subcell view */
	hbox_view->subcell_view_count = ecv->subcell_count;
	hbox_view->subcell_views = g_new (ECellView *, hbox_view->subcell_view_count);
	hbox_view->model_cols = g_new (int, hbox_view->subcell_view_count);
	hbox_view->def_size_cols = g_new (int, hbox_view->subcell_view_count);

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		hbox_view->subcell_views[i] = e_cell_new_view (ecv->subcells[i], table_model, e_table_item_view /* XXX */);
		hbox_view->model_cols[i] = ecv->model_cols[i];
		hbox_view->def_size_cols[i] = ecv->def_size_cols[i];
	}

	return (ECellView *) hbox_view;
}

/*
 * ECell::kill_view method
 */
static void
ecv_kill_view (ECellView *ecv)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecv;
	gint i;

	if (hbox_view->cell_view.kill_view_cb)
	    (hbox_view->cell_view.kill_view_cb)(ecv, hbox_view->cell_view.kill_view_cb_data);

	if (hbox_view->cell_view.kill_view_cb_data)
	    g_list_free (hbox_view->cell_view.kill_view_cb_data);

	/* kill our subcell view */
	for (i = 0; i < hbox_view->subcell_view_count; i++)
		e_cell_kill_view (hbox_view->subcell_views[i]);

	g_free (hbox_view->model_cols);
	g_free (hbox_view->def_size_cols);
	g_free (hbox_view->subcell_views);
	g_free (hbox_view);
}

/*
 * ECell::realize method
 */
static void
ecv_realize (ECellView *ecell_view)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecell_view;
	gint i;

	/* realize our subcell view */
	for (i = 0; i < hbox_view->subcell_view_count; i++)
		e_cell_realize (hbox_view->subcell_views[i]);

	if (E_CELL_CLASS (e_cell_hbox_parent_class)->realize)
		(* E_CELL_CLASS (e_cell_hbox_parent_class)->realize) (ecell_view);
}

/*
 * ECell::unrealize method
 */
static void
ecv_unrealize (ECellView *ecv)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecv;
	gint i;

	/* unrealize our subcell view. */
	for (i = 0; i < hbox_view->subcell_view_count; i++)
		e_cell_unrealize (hbox_view->subcell_views[i]);

	if (E_CELL_CLASS (e_cell_hbox_parent_class)->unrealize)
		(* E_CELL_CLASS (e_cell_hbox_parent_class)->unrealize) (ecv);
}

/*
 * ECell::draw method
 */
static void
ecv_draw (ECellView *ecell_view,
          cairo_t *cr,
          gint model_col,
          gint view_col,
          gint row,
          ECellFlags flags,
          gint x1,
          gint y1,
          gint x2,
          gint y2)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecell_view;

	gint subcell_offset = 0;
	gint i;
	gint allotted_width = x2 - x1;

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		/* Now cause our subcells to draw their contents,
		 * shifted by subcell_offset pixels */
		gint width = allotted_width * hbox_view->def_size_cols[i] / 100;
			/* e_cell_max_width_by_row (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col, row);
		if (width < hbox_view->def_size_cols[i])
			width = hbox_view->def_size_cols[i];
		printf ("width of %d %d of %d\n", width,hbox_view->def_size_cols[i], allotted_width); */

		e_cell_draw (
			hbox_view->subcell_views[i], cr,
			hbox_view->model_cols[i], view_col, row, flags,
			x1 + subcell_offset , y1,
			x1 + subcell_offset + width, y2);

		subcell_offset += width; /* e_cell_max_width_by_row (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col, row); */
	}
}

/*
 * ECell::event method
 */
static gint
ecv_event (ECellView *ecell_view,
           GdkEvent *event,
           gint model_col,
           gint view_col,
           gint row,
           ECellFlags flags,
           ECellActions *actions)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecell_view;
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

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		gint width = e_cell_max_width_by_row (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col, row);
		if (width < hbox_view->def_size_cols[i])
			width = hbox_view->def_size_cols[i];
		if (y < subcell_offset + width)
			return e_cell_event (hbox_view->subcell_views[i], event, hbox_view->model_cols[i], view_col, row, flags, actions);
		subcell_offset += width;
	}
	return 0;
}

/*
 * ECell::height method
 */
static gint
ecv_height (ECellView *ecell_view,
            gint model_col,
            gint view_col,
            gint row)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecell_view;
	gint height = 0, max_height = 0;
	gint i;

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		height = e_cell_height (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col, row);
		max_height = MAX (max_height, height);
	}
	return max_height;
}

/*
 * ECell::max_width method
 */
static gint
ecv_max_width (ECellView *ecell_view,
               gint model_col,
               gint view_col)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecell_view;
	gint width = 0;
	gint i;

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		gint cell_width = e_cell_max_width (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col);

		if (cell_width < hbox_view->def_size_cols[i])
			cell_width = hbox_view->def_size_cols[i];
		width += cell_width;
	}

	return width;
}

/*
 * GObject::dispose method
 */
static void
ecv_dispose (GObject *object)
{
	ECellHbox *ecv = E_CELL_HBOX (object);
	gint i;

	/* destroy our subcell */
	for (i = 0; i < ecv->subcell_count; i++)
		if (ecv->subcells[i])
			g_object_unref (ecv->subcells[i]);
	g_free (ecv->subcells);
	ecv->subcells = NULL;
	ecv->subcell_count = 0;

	g_free (ecv->model_cols);
	ecv->model_cols = NULL;

	g_free (ecv->def_size_cols);
	ecv->def_size_cols = NULL;

	G_OBJECT_CLASS (e_cell_hbox_parent_class)->dispose (object);
}

static void
e_cell_hbox_class_init (ECellHboxClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	ECellClass *ecc = E_CELL_CLASS (class);

	object_class->dispose = ecv_dispose;

	ecc->new_view = ecv_new_view;
	ecc->kill_view = ecv_kill_view;
	ecc->realize = ecv_realize;
	ecc->unrealize = ecv_unrealize;
	ecc->draw = ecv_draw;
	ecc->event = ecv_event;
	ecc->height = ecv_height;

	ecc->max_width = ecv_max_width;

/*	gal_a11y_e_cell_registry_add_cell_type (NULL, E_TYPE_CELL_HBOX, gal_a11y_e_cell_hbox_new); */
}

static void
e_cell_hbox_init (ECellHbox *ecv)
{
	ecv->subcells = NULL;
	ecv->subcell_count = 0;
}

/**
 * e_cell_hbox_new:
 *
 * Creates a new ECell renderer that can be used to render multiple
 * child cells.
 *
 * Return value: an ECell object that can be used to render multiple
 * child cells.
 **/
ECell *
e_cell_hbox_new (void)
{
	return g_object_new (E_TYPE_CELL_HBOX, NULL);
}

void
e_cell_hbox_append (ECellHbox *hbox,
                    ECell *subcell,
                    gint model_col,
                    gint size)
{
	hbox->subcell_count++;

	hbox->subcells = g_renew (ECell *, hbox->subcells, hbox->subcell_count);
	hbox->model_cols = g_renew (int, hbox->model_cols, hbox->subcell_count);
	hbox->def_size_cols = g_renew (int, hbox->def_size_cols, hbox->subcell_count);

	hbox->subcells[hbox->subcell_count - 1]   = subcell;
	hbox->model_cols[hbox->subcell_count - 1] = model_col;
	hbox->def_size_cols[hbox->subcell_count - 1] = size;

	if (subcell)
		g_object_ref_sink (subcell);
}
