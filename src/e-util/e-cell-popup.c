/*
 * e-cell-popup.c: Popup cell renderer
 *
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellPopup - an abstract ECell class used to support popup selections like
 * a GtkCombo widget. It contains a child ECell, e.g. an ECellText, but when
 * selected it displays an arrow on the right edge which the user can click to
 * show a popup. Subclasses implement the popup class function to show the
 * popup.
 */

#include "evolution-config.h"

#include <gdk/gdkkeysyms.h>

#include "gal-a11y-e-cell-popup.h"
#include "gal-a11y-e-cell-registry.h"

#include "e-cell-popup.h"
#include "e-table-item.h"
#include <gtk/gtk.h>

#define E_CELL_POPUP_ARROW_SIZE		16
#define E_CELL_POPUP_ARROW_PAD		3

static void	e_cell_popup_dispose	(GObject	*object);

static ECellView * ecp_new_view		(ECell		*ecell,
					 ETableModel	*table_model,
					 void		*e_table_item_view);
static void	ecp_kill_view		(ECellView	*ecv);
static void	ecp_realize		(ECellView	*ecv);
static void	ecp_unrealize		(ECellView	*ecv);
static void	ecp_draw		(ECellView	*ecv,
					 cairo_t	*cr,
					 gint		 model_col,
					 gint		 view_col,
					 gint		 row,
					 ECellFlags	 flags,
					 gint		 x1,
					 gint		 y1,
					 gint		 x2,
					 gint		 y2);
static gint	ecp_event		(ECellView	*ecv,
					 GdkEvent	*event,
					 gint		 model_col,
					 gint		 view_col,
					 gint		 row,
					 ECellFlags	 flags,
					 ECellActions	*actions);
static gint	ecp_height		(ECellView	*ecv,
					 gint		 model_col,
					 gint		 view_col,
					 gint		 row);
static gpointer	ecp_enter_edit		(ECellView	*ecv,
					 gint		 model_col,
					 gint		 view_col,
					 gint		 row);
static void	ecp_leave_edit		(ECellView	*ecv,
					 gint		 model_col,
					 gint		 view_col,
					 gint		 row,
					 void		*edit_context);
static void	ecp_print		(ECellView	*ecv,
					 GtkPrintContext *context,
					 gint		 model_col,
					 gint		 view_col,
					 gint		 row,
					 gdouble		 width,
					 gdouble		 height);
static gdouble	ecp_print_height	(ECellView	*ecv,
					 GtkPrintContext *context,
					 gint		 model_col,
					 gint		 view_col,
					 gint		 row,
					 gdouble		 width);
static gint	ecp_max_width		(ECellView	*ecv,
					 gint		 model_col,
					 gint		 view_col);
static gchar *ecp_get_bg_color (ECellView *ecell_view, gint row);

static gint e_cell_popup_do_popup	(ECellPopupView	*ecp_view,
					 GdkEvent	*event,
					 gint             row,
					 gint             model_col);

G_DEFINE_TYPE (ECellPopup, e_cell_popup, E_TYPE_CELL)

static void
e_cell_popup_class_init (ECellPopupClass *class)
{
	ECellClass *ecc = E_CELL_CLASS (class);

	G_OBJECT_CLASS (class)->dispose = e_cell_popup_dispose;

	ecc->new_view = ecp_new_view;
	ecc->kill_view = ecp_kill_view;
	ecc->realize = ecp_realize;
	ecc->unrealize = ecp_unrealize;
	ecc->draw = ecp_draw;
	ecc->event = ecp_event;
	ecc->height = ecp_height;
	ecc->enter_edit = ecp_enter_edit;
	ecc->leave_edit = ecp_leave_edit;
	ecc->print = ecp_print;
	ecc->print_height = ecp_print_height;
	ecc->max_width = ecp_max_width;
	ecc->get_bg_color = ecp_get_bg_color;

	gal_a11y_e_cell_registry_add_cell_type (
		NULL, E_TYPE_CELL_POPUP,
		gal_a11y_e_cell_popup_new);
}

static void
e_cell_popup_init (ECellPopup *ecp)
{
	ecp->popup_shown = FALSE;
	ecp->popup_model = NULL;
}

/**
 * e_cell_popup_new:
 *
 * Creates a new ECellPopup renderer.
 *
 * Returns: an ECellPopup object.
 */
ECell *
e_cell_popup_new (void)
{
	return g_object_new (E_TYPE_CELL_POPUP, NULL);
}

static void
e_cell_popup_dispose (GObject *object)
{
	ECellPopup *ecp = E_CELL_POPUP (object);

	g_clear_object (&ecp->child);

	G_OBJECT_CLASS (e_cell_popup_parent_class)->dispose (object);
}

/*
 * ECell::new_view method
 */
static ECellView *
ecp_new_view (ECell *ecell,
              ETableModel *table_model,
              gpointer e_table_item_view)
{
	ECellPopup *ecp = E_CELL_POPUP (ecell);
	ECellPopupView *ecp_view;

	/* We must have a child ECell before we create any views. */
	g_return_val_if_fail (ecp->child != NULL, NULL);

	ecp_view = g_new0 (ECellPopupView, 1);

	ecp_view->cell_view.ecell = g_object_ref (ecell);
	ecp_view->cell_view.e_table_model = table_model;
	ecp_view->cell_view.e_table_item_view = e_table_item_view;
	ecp_view->cell_view.kill_view_cb = NULL;
	ecp_view->cell_view.kill_view_cb_data = NULL;

	ecp_view->child_view = e_cell_new_view (
		ecp->child, table_model,
		e_table_item_view);

	return (ECellView *) ecp_view;
}

/*
 * ECell::kill_view method
 */
static void
ecp_kill_view (ECellView *ecv)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	if (E_IS_CELL_POPUP (ecp_view->cell_view.ecell)) {
		ECellPopup *ecp = E_CELL_POPUP (ecp_view->cell_view.ecell);

		if (ecp->popup_cell_view == ecp_view)
			ecp->popup_cell_view = NULL;
	}

	g_clear_object (&ecp_view->cell_view.ecell);

	if (ecp_view->cell_view.kill_view_cb)
		ecp_view->cell_view.kill_view_cb (
			ecv, ecp_view->cell_view.kill_view_cb_data);

	if (ecp_view->cell_view.kill_view_cb_data)
		g_list_free (ecp_view->cell_view.kill_view_cb_data);

	if (ecp_view->child_view)
		e_cell_kill_view (ecp_view->child_view);

	g_free (ecp_view);
}

/*
 * ECell::realize method
 */
static void
ecp_realize (ECellView *ecv)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	e_cell_realize (ecp_view->child_view);

	if (E_CELL_CLASS (e_cell_popup_parent_class)->realize)
		(* E_CELL_CLASS (e_cell_popup_parent_class)->realize) (ecv);
}

/*
 * ECell::unrealize method
 */
static void
ecp_unrealize (ECellView *ecv)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	e_cell_unrealize (ecp_view->child_view);

	if (E_CELL_CLASS (e_cell_popup_parent_class)->unrealize)
		(* E_CELL_CLASS (e_cell_popup_parent_class)->unrealize) (ecv);
}

/*
 * ECell::draw method
 */
static void
ecp_draw (ECellView *ecv,
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
	ECellPopup *ecp = E_CELL_POPUP (ecv->ecell);
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;
	GtkWidget *canvas;
	gboolean show_popup_arrow;

	cairo_save (cr);

	canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (ecv->e_table_item_view)->canvas);

	/* Display the popup arrow if we are the cursor cell, or the popup
	 * is shown for this cell. */
	show_popup_arrow =
		e_table_model_is_cell_editable (
			ecv->e_table_model, model_col, row) &&
		(flags & E_CELL_CURSOR ||
			(ecp->popup_shown && ecp->popup_view_col == view_col
			&& ecp->popup_row == row
			&& ecp->popup_model == ((ECellView *) ecp_view)->e_table_model));

	if (flags & E_CELL_CURSOR)
		ecp->popup_arrow_shown = show_popup_arrow;

	if (show_popup_arrow) {
		GtkStyleContext *style_context;
		gint arrow_x;
		gint arrow_y;
		gint arrow_size;
		gint midpoint_y;

		e_cell_draw (
			ecp_view->child_view, cr, model_col,
			view_col, row, flags,
			x1, y1, x2 - E_CELL_POPUP_ARROW_SIZE, y2);

		midpoint_y = y1 + ((y2 - y1 + 1) / 2);

		arrow_x = x2 - E_CELL_POPUP_ARROW_SIZE;
		arrow_y = midpoint_y - E_CELL_POPUP_ARROW_SIZE / 2;
		arrow_size = E_CELL_POPUP_ARROW_SIZE;

		style_context = gtk_widget_get_style_context (canvas);

		gtk_style_context_save (style_context);

		gtk_style_context_add_class (
			style_context, GTK_STYLE_CLASS_CELL);

		cairo_save (cr);
		gtk_render_background (
			style_context, cr,
			(gdouble) arrow_x,
			(gdouble) arrow_y,
			(gdouble) arrow_size,
			(gdouble) arrow_size);
		cairo_restore (cr);

		arrow_x += E_CELL_POPUP_ARROW_PAD;
		arrow_y += E_CELL_POPUP_ARROW_PAD;
		arrow_size -= (E_CELL_POPUP_ARROW_PAD * 2);

		cairo_save (cr);
		gtk_render_arrow (
			style_context, cr, G_PI,
			(gdouble) arrow_x,
			(gdouble) arrow_y,
			(gdouble) arrow_size);
		cairo_restore (cr);

		gtk_style_context_restore (style_context);
	} else {
		e_cell_draw (
			ecp_view->child_view, cr, model_col,
			view_col, row, flags, x1, y1, x2, y2);
	}

	cairo_restore (cr);
}

/*
 * ECell::event method
 */
static gint
ecp_event (ECellView *ecv,
           GdkEvent *event,
           gint model_col,
           gint view_col,
           gint row,
           ECellFlags flags,
           ECellActions *actions)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;
	ECellPopup *ecp = E_CELL_POPUP (ecp_view->cell_view.ecell);
	ETableItem *eti = E_TABLE_ITEM (ecv->e_table_item_view);
	gint width;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (e_table_model_is_cell_editable (ecv->e_table_model, model_col, row) &&
		    flags & E_CELL_CURSOR
		    && ecp->popup_arrow_shown) {
			width = e_table_header_col_diff (
				eti->header, view_col,
				view_col + 1);

			/* FIXME: The event coords seem to be relative to the
			 * text within the cell, so we have to add 4. */
			if (event->button.x + 4 >= width - E_CELL_POPUP_ARROW_SIZE) {
				return e_cell_popup_do_popup (ecp_view, event, row, view_col);
			}
		}
		break;
	case GDK_KEY_PRESS:
		if (e_table_model_is_cell_editable (ecv->e_table_model, model_col, row) &&
		    event->key.state & GDK_MOD1_MASK
		    && event->key.keyval == GDK_KEY_Down) {
			return e_cell_popup_do_popup (ecp_view, event, row, view_col);
		}
		break;
	default:
		break;
	}

	return e_cell_event (
		ecp_view->child_view, event, model_col, view_col,
		row, flags, actions);
}

/*
 * ECell::height method
 */
static gint
ecp_height (ECellView *ecv,
            gint model_col,
            gint view_col,
            gint row)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	return e_cell_height (ecp_view->child_view, model_col, view_col, row);
}

/*
 * ECellView::enter_edit method
 */
static gpointer
ecp_enter_edit (ECellView *ecv,
                gint model_col,
                gint view_col,
                gint row)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	return e_cell_enter_edit (ecp_view->child_view, model_col, view_col, row);
}

/*
 * ECellView::leave_edit method
 */
static void
ecp_leave_edit (ECellView *ecv,
                gint model_col,
                gint view_col,
                gint row,
                gpointer edit_context)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	e_cell_leave_edit (
		ecp_view->child_view, model_col, view_col, row,
		edit_context);
}

static void
ecp_print (ECellView *ecv,
           GtkPrintContext *context,
           gint model_col,
           gint view_col,
           gint row,
           gdouble width,
           gdouble height)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	e_cell_print (
		ecp_view->child_view, context, model_col, view_col, row,
		width, height);
}

static gdouble
ecp_print_height (ECellView *ecv,
                  GtkPrintContext *context,
                  gint model_col,
                  gint view_col,
                  gint row,
                  gdouble width)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	return e_cell_print_height (
		ecp_view->child_view, context, model_col,
		view_col, row, width);
}

static gint
ecp_max_width (ECellView *ecv,
               gint model_col,
               gint view_col)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecv;

	return e_cell_max_width (ecp_view->child_view, model_col, view_col);
}

static gchar *
ecp_get_bg_color (ECellView *ecell_view,
                  gint row)
{
	ECellPopupView *ecp_view = (ECellPopupView *) ecell_view;

	return e_cell_get_bg_color (ecp_view->child_view, row);
}

ECell *
e_cell_popup_get_child (ECellPopup *ecp)
{
	g_return_val_if_fail (E_IS_CELL_POPUP (ecp), NULL);

	return ecp->child;
}

void
e_cell_popup_set_child (ECellPopup *ecp,
                        ECell *child)
{
	g_return_if_fail (E_IS_CELL_POPUP (ecp));

	if (ecp->child)
		g_object_unref (ecp->child);

	ecp->child = child;
	g_object_ref (child);
}

static gint
e_cell_popup_do_popup (ECellPopupView *ecp_view,
                       GdkEvent *event,
                       gint row,
                       gint view_col)
{
	ECellPopup *ecp = E_CELL_POPUP (ecp_view->cell_view.ecell);
	gint (*popup_func) (ECellPopup *ecp, GdkEvent *event, gint row, gint view_col);

	ecp->popup_cell_view = ecp_view;

	popup_func = E_CELL_POPUP_CLASS (G_OBJECT_GET_CLASS (ecp))->popup;

	ecp->popup_view_col = view_col;
	ecp->popup_row = row;
	ecp->popup_model = ((ECellView *) ecp_view)->e_table_model;

	return popup_func ? popup_func (ecp, event, row, view_col) : FALSE;
}

/* This redraws the popup cell. Only use this if you know popup_view_col and
 * popup_row are valid. */
void
e_cell_popup_queue_cell_redraw (ECellPopup *ecp)
{
	ETableItem *eti;

	g_return_if_fail (ecp->popup_cell_view != NULL);

	eti = E_TABLE_ITEM (ecp->popup_cell_view->cell_view.e_table_item_view);

	e_table_item_redraw_range (
		eti, ecp->popup_view_col, ecp->popup_row,
		ecp->popup_view_col, ecp->popup_row);
}

void
e_cell_popup_set_shown (ECellPopup *ecp,
                        gboolean shown)
{
	ecp->popup_shown = shown;
	e_cell_popup_queue_cell_redraw (ecp);
}
