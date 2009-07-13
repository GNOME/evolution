/*
 * e-cell-combo.c: Combo cell renderer
 *
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellCombo - a subclass of ECellPopup used to support popup lists like a
 * GtkCombo widget. It only supports a basic popup list of strings at present,
 * with no auto-completion.
 */

/*
 * Notes: (handling pointer grabs and GTK+ grabs is a nightmare!)
 *
 * o We must grab the pointer when we show the popup, so that if any buttons
 *   are pressed outside the application we hide the popup.
 *
 * o We have to be careful when popping up any widgets which also grab the
 *   pointer at some point, since we will lose our own pointer grab.
 *   When we pop up a list it will grab the pointer itself when an item is
 *   selected, and release the grab when the button is released.
 *   Fortunately we hide the popup at this point, so it isn't a problem.
 *   But for other types of widgets in the popup it could cause trouble.
 *   - I think GTK+ should provide help for this (nested pointer grabs?).
 *
 * o We must set the 'owner_events' flag of the pointer grab to TRUE so that
 *   pointer events get reported to all the application windows as normal.
 *   If we don't do this then the widgets in the popup may not work properly.
 *
 * o We must do a gtk_grab_add() so that we only allow events to go to the
 *   widgets within the popup (though some special events still get reported
 *   to the widget owning the window). Doing th gtk_grab_add() on the toplevel
 *   popup window should be fine. We can then check for any events that should
 *   close the popup, like the Escape key, or a button press outside the popup.
 */

#include <config.h>

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"
#include "misc/e-unicode.h"

#include "e-table-item.h"
#include "e-cell-combo.h"
#include "e-cell-text.h"

#define d(x)

/* The height to make the popup list if there aren't any items in it. */
#define	E_CELL_COMBO_LIST_EMPTY_HEIGHT	15

static void e_cell_combo_dispose	(GObject	*object);

static gint e_cell_combo_do_popup	(ECellPopup	*ecp,
					 GdkEvent	*event,
					 gint             row,
					 gint             view_col);
static void e_cell_combo_select_matching_item	(ECellCombo	*ecc);
static void e_cell_combo_show_popup	(ECellCombo	*ecc,
					 gint             row,
					 gint             view_col);
static void e_cell_combo_get_popup_pos	(ECellCombo	*ecc,
					 gint             row,
					 gint             view_col,
					 gint		*x,
					 gint		*y,
					 gint		*height,
					 gint		*width);

static void e_cell_combo_selection_changed (GtkTreeSelection *selection, ECellCombo *ecc);

static gint e_cell_combo_button_press	(GtkWidget	*popup_window,
					 GdkEvent	*event,
					 ECellCombo	*ecc);
static gint e_cell_combo_button_release	(GtkWidget	*popup_window,
					 GdkEventButton	*event,
					 ECellCombo	*ecc);
static gint e_cell_combo_key_press	(GtkWidget	*popup_window,
					 GdkEventKey	*event,
					 ECellCombo	*ecc);

static void e_cell_combo_update_cell	(ECellCombo	*ecc);
static void e_cell_combo_restart_edit	(ECellCombo	*ecc);

G_DEFINE_TYPE (ECellCombo, e_cell_combo, E_CELL_POPUP_TYPE)

static void
e_cell_combo_class_init			(ECellComboClass	*klass)
{
	ECellPopupClass *ecpc = E_CELL_POPUP_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = e_cell_combo_dispose;

	ecpc->popup = e_cell_combo_do_popup;
}

static void
e_cell_combo_init			(ECellCombo	*ecc)
{
	GtkWidget *frame;
	AtkObject *a11y;
	GtkListStore *store;
	GtkTreeSelection *selection;

	/* We create one popup window for the ECell, since there will only
	   ever be one popup in use at a time. */
	ecc->popup_window = gtk_window_new (GTK_WINDOW_POPUP);

	gtk_window_set_type_hint (GTK_WINDOW (ecc->popup_window), GDK_WINDOW_TYPE_HINT_COMBO);
	gtk_window_set_resizable (GTK_WINDOW (ecc->popup_window), TRUE);

	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (ecc->popup_window), frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_widget_show (frame);

	ecc->popup_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	GTK_WIDGET_UNSET_FLAGS (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window)->hscrollbar, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window)->vscrollbar, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (frame), ecc->popup_scrolled_window);
	gtk_widget_show (ecc->popup_scrolled_window);

	store = gtk_list_store_new (1, G_TYPE_STRING);
	ecc->popup_tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_tree_view_append_column (
		GTK_TREE_VIEW (ecc->popup_tree_view),
		gtk_tree_view_column_new_with_attributes ("Text", gtk_cell_renderer_text_new (), "text", 0, NULL));

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ecc->popup_tree_view), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ecc->popup_tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window), ecc->popup_tree_view);
	gtk_container_set_focus_vadjustment (GTK_CONTAINER (ecc->popup_tree_view),
					     gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window)));
	gtk_container_set_focus_hadjustment (GTK_CONTAINER (ecc->popup_tree_view),
					     gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window)));
	gtk_widget_show (ecc->popup_tree_view);

	a11y = gtk_widget_get_accessible (ecc->popup_tree_view);
	atk_object_set_name (a11y, _("popup list"));

	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (e_cell_combo_selection_changed),
			  ecc);
	g_signal_connect (ecc->popup_window,
			  "button_press_event",
			  G_CALLBACK (e_cell_combo_button_press),
			  ecc);
	/* We use connect_after here so the list updates the selection before
	   we hide the popup and update the cell. */
	g_signal_connect (ecc->popup_window,
			  "button_release_event",
			  G_CALLBACK (e_cell_combo_button_release),
			  ecc);
	g_signal_connect (ecc->popup_window,
			  "key_press_event",
			  G_CALLBACK (e_cell_combo_key_press), ecc);
}

/**
 * e_cell_combo_new:
 *
 * Creates a new ECellCombo renderer.
 *
 * Returns: an ECellCombo object.
 */
ECell *
e_cell_combo_new			(void)
{
	ECellCombo *ecc = g_object_new (E_CELL_COMBO_TYPE, NULL);

	return (ECell*) ecc;
}

/*
 * GObject::dispose method
 */
static void
e_cell_combo_dispose			(GObject *object)
{
	ECellCombo *ecc = E_CELL_COMBO (object);

	if (ecc->popup_window)
		gtk_widget_destroy (ecc->popup_window);
	ecc->popup_window = NULL;

	G_OBJECT_CLASS (e_cell_combo_parent_class)->dispose (object);
}

void
e_cell_combo_set_popdown_strings	(ECellCombo	*ecc,
					 GList		*strings)
{
	GList *elem;
	GtkListStore *store;

	g_return_if_fail (E_IS_CELL_COMBO (ecc));
	g_return_if_fail (strings != NULL);

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (ecc->popup_tree_view)));
	gtk_list_store_clear (store);

	for (elem = strings; elem; elem = elem->next) {
		GtkTreeIter iter;
		gchar *utf8_text = elem->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, utf8_text, -1);
	}
}

static gint
e_cell_combo_do_popup			(ECellPopup	*ecp,
					 GdkEvent	*event,
					 gint             row,
					 gint             view_col)
{
	ECellCombo *ecc = E_CELL_COMBO (ecp);
	guint32 time;
	gint error_code;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ecc->popup_tree_view));

	g_signal_handlers_block_by_func (selection, e_cell_combo_selection_changed, ecc);
	e_cell_combo_show_popup (ecc, row, view_col);
	e_cell_combo_select_matching_item (ecc);
	g_signal_handlers_unblock_by_func (selection, e_cell_combo_selection_changed, ecc);

	if (event->type == GDK_BUTTON_PRESS) {
		time = event->button.time;
	} else {
		time = event->key.time;
	}

	error_code = gdk_pointer_grab (gtk_widget_get_window (ecc->popup_tree_view), TRUE,
				       GDK_ENTER_NOTIFY_MASK |
				       GDK_BUTTON_PRESS_MASK |
				       GDK_BUTTON_RELEASE_MASK |
				       GDK_POINTER_MOTION_HINT_MASK |
				       GDK_BUTTON1_MOTION_MASK,
				       NULL, NULL, time);
	if (error_code != 0)
		g_warning ("Failed to get pointer grab (%i)", error_code);
	gtk_grab_add (ecc->popup_window);
	gdk_keyboard_grab (gtk_widget_get_window (ecc->popup_tree_view), TRUE, time);

	return TRUE;
}

static void
e_cell_combo_select_matching_item	(ECellCombo	*ecc)
{
	ECellPopup *ecp = E_CELL_POPUP (ecc);
	ECellView *ecv = (ECellView*) ecp->popup_cell_view;
	ECellText *ecell_text = E_CELL_TEXT (ecp->child);
	ETableItem *eti = E_TABLE_ITEM (ecp->popup_cell_view->cell_view.e_table_item_view);
	ETableCol *ecol;
	gboolean found = FALSE;
	gchar *cell_text;
	gboolean valid;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;

	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);
	cell_text = e_cell_text_get_text (ecell_text, ecv->e_table_model,
					  ecol->col_idx, ecp->popup_row);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ecc->popup_tree_view));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ecc->popup_tree_view));

	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid && !found;
	     valid = gtk_tree_model_iter_next (model, &iter)) {
		gchar *str = NULL;

		gtk_tree_model_get (model, &iter, 0, &str, -1);

		if (str && g_str_equal (str, cell_text)) {
			GtkTreePath *path = gtk_tree_model_get_path (model, &iter);

			gtk_tree_view_set_cursor (GTK_TREE_VIEW (ecc->popup_tree_view), path, NULL, FALSE);
			gtk_tree_path_free (path);

			found = TRUE;
		}

		g_free (str);
	}

	if (!found)
		gtk_tree_selection_unselect_all (selection);

	e_cell_text_free_text (ecell_text, cell_text);
}

static void
e_cell_combo_show_popup			(ECellCombo	*ecc, gint row, gint view_col)
{
	gint x, y, width, height, old_width, old_height;

	/* This code is practically copied from GtkCombo. */
	old_width = ecc->popup_window->allocation.width;
	old_height  = ecc->popup_window->allocation.height;

	e_cell_combo_get_popup_pos (ecc, row, view_col, &x, &y, &height, &width);

	/* workaround for gtk_scrolled_window_size_allocate bug */
	if (old_width != width || old_height != height) {
		gtk_widget_hide (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window)->hscrollbar);
		gtk_widget_hide (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window)->vscrollbar);
	}

	gtk_window_move (GTK_WINDOW (ecc->popup_window), x, y);
	gtk_widget_set_size_request (ecc->popup_window, width, height);
	gtk_widget_realize (ecc->popup_window);
	gdk_window_resize (ecc->popup_window->window, width, height);
	gtk_widget_show (ecc->popup_window);

	e_cell_popup_set_shown (E_CELL_POPUP (ecc), TRUE);
	d(g_print("%s: popup_shown = TRUE\n", __FUNCTION__));
}

/* Calculates the size and position of the popup window (like GtkCombo). */
static void
e_cell_combo_get_popup_pos		(ECellCombo	*ecc,
					 gint             row,
					 gint             view_col,
					 gint		*x,
					 gint		*y,
					 gint		*height,
					 gint		*width)
{
	ECellPopup *ecp = E_CELL_POPUP (ecc);
	ETableItem *eti = E_TABLE_ITEM (ecp->popup_cell_view->cell_view.e_table_item_view);
	GtkWidget *canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (eti)->canvas);
	GtkBin *popwin;
	GtkScrolledWindow *popup;
	GtkRequisition list_requisition;
	gboolean show_vscroll = FALSE, show_hscroll = FALSE;
	gint avail_height, avail_width, min_height, work_height, screen_width;
	gint column_width, row_height, scrollbar_width;
	double x1, y1;
	double wx, wy;

	/* This code is practically copied from GtkCombo. */
	popup  = GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window);
	popwin = GTK_BIN (ecc->popup_window);

	gdk_window_get_origin (canvas->window, x, y);

	x1 = e_table_header_col_diff (eti->header, 0, view_col + 1);
	y1 = e_table_item_row_diff (eti, 0, row + 1);
	column_width = e_table_header_col_diff (eti->header, view_col,
						view_col + 1);
	row_height = e_table_item_row_diff (eti, row,
					    row + 1);
	gnome_canvas_item_i2w (GNOME_CANVAS_ITEM (eti), &x1, &y1);

	gnome_canvas_world_to_window (GNOME_CANVAS (canvas),
				      x1,
				      y1,
				      &wx,
				      &wy);

	x1 = wx;
	y1 = wy;

	*x += x1;
	/* The ETable positions don't include the grid lines, I think, so we add 1. */
	*y += y1 + 1
		- (gint)((GnomeCanvas *)canvas)->layout.vadjustment->value
		+ ((GnomeCanvas *)canvas)->zoom_yofs;

	scrollbar_width = popup->vscrollbar->requisition.width
		+ GTK_SCROLLED_WINDOW_CLASS (GTK_OBJECT_GET_CLASS (popup))->scrollbar_spacing;

	avail_height = gdk_screen_height () - *y;

	/* We'll use the entire screen width if needed, but we save space for
	   the vertical scrollbar in case we need to show that. */
	screen_width = gdk_screen_width ();
	avail_width = screen_width - scrollbar_width;

	gtk_widget_size_request (ecc->popup_tree_view, &list_requisition);
	min_height = MIN (list_requisition.height,
			  popup->vscrollbar->requisition.height);
	if (!gtk_tree_model_iter_n_children (gtk_tree_view_get_model (GTK_TREE_VIEW (ecc->popup_tree_view)), NULL))
		list_requisition.height += E_CELL_COMBO_LIST_EMPTY_HEIGHT;

	/* Calculate the desired width. */
	*width = list_requisition.width
		+ 2 * popwin->child->style->xthickness
		+ 2 * GTK_CONTAINER (popwin->child)->border_width
		+ 2 * GTK_CONTAINER (popup)->border_width
		+ 2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width
		+ 2 * GTK_BIN (popup)->child->style->xthickness;

	/* Use at least the same width as the column. */
	if (*width < column_width)
		*width = column_width;

	/* If it is larger than the available width, use that instead and show
	   the horizontal scrollbar. */
	if (*width > avail_width) {
		*width = avail_width;
		show_hscroll = TRUE;
	}

	/* Calculate all the borders etc. that we need to add to the height. */
	work_height = (2 * popwin->child->style->ythickness
		       + 2 * GTK_CONTAINER (popwin->child)->border_width
		       + 2 * GTK_CONTAINER (popup)->border_width
		       + 2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width
		       + 2 * GTK_BIN (popup)->child->style->xthickness);

	/* Add on the height of the horizontal scrollbar if we need it. */
	if (show_hscroll)
		work_height += popup->hscrollbar->requisition.height +
			GTK_SCROLLED_WINDOW_CLASS (GTK_OBJECT_GET_CLASS (popup))->scrollbar_spacing;

	/* Check if it fits in the available height. */
	if (work_height + list_requisition.height > avail_height) {
		/* It doesn't fit, so we see if we have the minimum space
		   needed. */
		if (work_height + min_height > avail_height
		    && *y - row_height > avail_height) {
			/* We don't, so we show the popup above the cell
			   instead of below it. */
			avail_height = *y - row_height;
			*y -= (work_height + list_requisition.height
			       + row_height);
			if (*y < 0)
				*y = 0;
		}
	}

	/* Check if we still need the vertical scrollbar. */
	if (work_height + list_requisition.height > avail_height) {
		*width += scrollbar_width;
		show_vscroll = TRUE;
	}

	/* We try to line it up with the right edge of the column, but we don't
	   want it to go off the edges of the screen. */
	if (*x > screen_width)
		*x = screen_width;
	*x -= *width;
	if (*x < 0)
		*x = 0;

	if (show_vscroll)
		*height = avail_height;
	else
		*height = work_height + list_requisition.height;
}

static void
e_cell_combo_selection_changed (GtkTreeSelection *selection, ECellCombo *ecc)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (!GTK_WIDGET_REALIZED (ecc->popup_window) || !gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	e_cell_combo_update_cell (ecc);
	e_cell_combo_restart_edit (ecc);
}

/* This handles button press events in the popup window.
   Note that since we have a pointer grab on this window, we also get button
   press events for windows outside the application here, so we hide the popup
   window if that happens. We also get propagated events from child widgets
   which we ignore. */
static gint
e_cell_combo_button_press		(GtkWidget	*popup_window,
					 GdkEvent	*event,
					 ECellCombo	*ecc)
{
	GtkWidget *event_widget;

	event_widget = gtk_get_event_widget (event);

	/* If the button press was for a widget inside the popup list, but
	   not the popup window itself, then we ignore the event and return
	   FALSE. Otherwise we will hide the popup.
	   Note that since we have a pointer grab on the popup list, button
	   presses outside the application will be reported to this window,
	   which is why we hide the popup in this case. */
	while (event_widget) {
		event_widget = event_widget->parent;
		if (event_widget == ecc->popup_tree_view)
			return FALSE;
	}

	gtk_grab_remove (ecc->popup_window);
	gdk_pointer_ungrab (event->button.time);
	gdk_keyboard_ungrab (event->button.time);
	gtk_widget_hide (ecc->popup_window);

	e_cell_popup_set_shown (E_CELL_POPUP (ecc), FALSE);
	d(g_print("%s: popup_shown = FALSE\n", __FUNCTION__));

	/* We don't want to update the cell here. Since the list is in browse
	   mode there will always be one item selected, so when we popup the
	   list one item is selected even if it doesn't match the current text
	   in the cell. So if you click outside the popup (which is what has
	   happened here) it is better to not update the cell. */
	/*e_cell_combo_update_cell (ecc);*/
	e_cell_combo_restart_edit (ecc);

	return TRUE;
}

/* This handles button release events in the popup window. If the button is
   released inside the list, we want to hide the popup window and update the
   cell with the new selection. */
static gint
e_cell_combo_button_release		(GtkWidget	*popup_window,
					 GdkEventButton	*event,
					 ECellCombo	*ecc)
{
	GtkWidget *event_widget;

	event_widget = gtk_get_event_widget ((GdkEvent*) event);

	/* See if the button was released in the list (or its children). */
	while (event_widget && event_widget != ecc->popup_tree_view)
		event_widget = event_widget->parent;

	/* If it wasn't, then we just ignore the event. */
	if (event_widget != ecc->popup_tree_view)
		return FALSE;

	/* The button was released inside the list, so we hide the popup and
	   update the cell to reflect the new selection. */
	gtk_grab_remove (ecc->popup_window);
	gdk_pointer_ungrab (event->time);
	gdk_keyboard_ungrab (event->time);
	gtk_widget_hide (ecc->popup_window);

	e_cell_popup_set_shown (E_CELL_POPUP (ecc), FALSE);
	d(g_print("%s: popup_shown = FALSE\n", __FUNCTION__));

	e_cell_combo_update_cell (ecc);
	e_cell_combo_restart_edit (ecc);

	return TRUE;
}

/* This handles key press events in the popup window. If the Escape key is
   pressed we hide the popup, and do not change the cell contents. */
static gint
e_cell_combo_key_press			(GtkWidget	*popup_window,
					 GdkEventKey	*event,
					 ECellCombo	*ecc)
{
	/* If the Escape key is pressed we hide the popup. */
	if (event->keyval != GDK_Escape
	    && event->keyval != GDK_Return
	    && event->keyval != GDK_KP_Enter
	    && event->keyval != GDK_ISO_Enter
	    && event->keyval != GDK_3270_Enter)
		return FALSE;

	if (event->keyval == GDK_Escape && (!ecc->popup_window||!GTK_WIDGET_VISIBLE (ecc->popup_window)))
		return FALSE;

	gtk_grab_remove (ecc->popup_window);
	gdk_pointer_ungrab (event->time);
	gdk_keyboard_ungrab (event->time);
	gtk_widget_hide (ecc->popup_window);

	e_cell_popup_set_shown (E_CELL_POPUP (ecc), FALSE);
	d(g_print("%s: popup_shown = FALSE\n", __FUNCTION__));

	if (event->keyval != GDK_Escape)
		e_cell_combo_update_cell (ecc);

	e_cell_combo_restart_edit (ecc);

	return TRUE;
}

static void
e_cell_combo_update_cell		(ECellCombo	*ecc)
{
	ECellPopup *ecp = E_CELL_POPUP (ecc);
	ECellView *ecv = (ECellView*) ecp->popup_cell_view;
	ECellText *ecell_text = E_CELL_TEXT (ecp->child);
	ETableItem *eti = E_TABLE_ITEM (ecv->e_table_item_view);
	ETableCol *ecol;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ecc->popup_tree_view));
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text = NULL, *old_text;

	/* Return if no item is selected. */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	/* Get the text of the selected item. */
	gtk_tree_model_get (model, &iter, 0, &text, -1);
	g_return_if_fail (text != NULL);

	/* Compare it with the existing cell contents. */
	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);

	old_text = e_cell_text_get_text (ecell_text, ecv->e_table_model,
					 ecol->col_idx, ecp->popup_row);

	/* If they are different, update the cell contents. */
	if (old_text && strcmp (old_text, text)) {
		e_cell_text_set_value (ecell_text, ecv->e_table_model,
				       ecol->col_idx, ecp->popup_row, text);
	}

	e_cell_text_free_text (ecell_text, old_text);
	g_free (text);
}

static void
e_cell_combo_restart_edit		(ECellCombo	*ecc)
{
	/* This doesn't work. ETable stops the edit straight-away again. */
#if 0
	ECellView *ecv = (ECellView*) ecc->popup_cell_view;
	ETableItem *eti = E_TABLE_ITEM (ecv->e_table_item_view);

	e_table_item_enter_edit (eti, ecc->popup_view_col, ecc->popup_row);
#endif
}

