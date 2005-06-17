/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-combo.c: Combo cell renderer
 * Copyright 2001, Ximian, Inc.
 *
 * Authors:
 *   Damon Chaplin <damon@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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

#include "e-util/e-i18n.h"
#include "e-util/e-util.h"
#include "widgets/misc/e-unicode.h"

#include "e-table-item.h"
#include "e-cell-combo.h"
#include "e-cell-text.h"

#define d(x)


/* The height to make the popup list if there aren't any items in it. */
#define	E_CELL_COMBO_LIST_EMPTY_HEIGHT	15

/* The object data key used to store the UTF-8 text of the popup list items. */
#define E_CELL_COMBO_UTF8_KEY		"UTF-8-TEXT"


static void e_cell_combo_class_init	(GObjectClass	*object_class);
static void e_cell_combo_init		(ECellCombo	*ecc);
static void e_cell_combo_dispose	(GObject	*object);

static gint e_cell_combo_do_popup	(ECellPopup	*ecp,
					 GdkEvent	*event,
					 int             row,
					 int             view_col);
static void e_cell_combo_select_matching_item	(ECellCombo	*ecc);
static void e_cell_combo_show_popup	(ECellCombo	*ecc,
					 int             row,
					 int             view_col);
static void e_cell_combo_get_popup_pos	(ECellCombo	*ecc,
					 int             row,
					 int             view_col,
					 gint		*x,
					 gint		*y,
					 gint		*height,
					 gint		*width);

static void e_cell_combo_selection_changed (GtkWidget *popup_list, ECellCombo *ecc);

static gint e_cell_combo_list_button_press (GtkWidget *popup_list, GdkEvent *event, ECellCombo *ecc);

static gint e_cell_combo_button_press	(GtkWidget	*popup_window,
					 GdkEvent	*event,
					 ECellCombo	*ecc);
static gint e_cell_combo_button_release	(GtkWidget	*popup_window,
					 GdkEventButton	*event,
					 ECellCombo	*ecc);
static int e_cell_combo_key_press	(GtkWidget	*popup_window,
					 GdkEventKey	*event,
					 ECellCombo	*ecc);

static void e_cell_combo_update_cell	(ECellCombo	*ecc);
static void e_cell_combo_restart_edit	(ECellCombo	*ecc);


static ECellPopupClass *parent_class;


E_MAKE_TYPE (e_cell_combo, "ECellCombo", ECellCombo,
	     e_cell_combo_class_init, e_cell_combo_init,
	     e_cell_popup_get_type())


static void
e_cell_combo_class_init			(GObjectClass	*object_class)
{
	ECellPopupClass *ecpc = (ECellPopupClass *) object_class;

	object_class->dispose = e_cell_combo_dispose;

	ecpc->popup = e_cell_combo_do_popup;

	parent_class = g_type_class_ref (E_CELL_POPUP_TYPE);
}


static void
e_cell_combo_init			(ECellCombo	*ecc)
{
	GtkWidget *frame;
	AtkObject *a11y;

	/* We create one popup window for the ECell, since there will only
	   ever be one popup in use at a time. */
	ecc->popup_window = gtk_window_new (GTK_WINDOW_POPUP);

	gtk_window_set_policy (GTK_WINDOW (ecc->popup_window),
			       TRUE, TRUE, FALSE);
  
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

	ecc->popup_list = gtk_list_new ();
	gtk_list_set_selection_mode (GTK_LIST (ecc->popup_list),
				     GTK_SELECTION_BROWSE);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window), ecc->popup_list);
	gtk_container_set_focus_vadjustment (GTK_CONTAINER (ecc->popup_list),
					     gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window)));
	gtk_container_set_focus_hadjustment (GTK_CONTAINER (ecc->popup_list),
					     gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window)));
	gtk_widget_show (ecc->popup_list);

	a11y = gtk_widget_get_accessible (ecc->popup_list);
	atk_object_set_name (a11y, _("popup list"));

	g_signal_connect (ecc->popup_list,
			  "selection_changed",
			  G_CALLBACK (e_cell_combo_selection_changed),
			  ecc);
	g_signal_connect (ecc->popup_list,
			  "button_press_event",
			  G_CALLBACK (e_cell_combo_list_button_press),
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

	G_OBJECT_CLASS (parent_class)->dispose (object);
}



void
e_cell_combo_set_popdown_strings	(ECellCombo	*ecc, 
					 GList		*strings)
{
	GList *elem;
	GtkWidget *listitem;

	g_return_if_fail (E_IS_CELL_COMBO (ecc));
	g_return_if_fail (strings != NULL);

	gtk_list_clear_items (GTK_LIST (ecc->popup_list), 0, -1);
	elem = strings;
	while (elem) {
		char *utf8_text = elem->data;

		listitem = gtk_list_item_new_with_label (utf8_text);

		gtk_widget_show (listitem);
		gtk_container_add (GTK_CONTAINER (ecc->popup_list), listitem);

		g_object_set_data_full (G_OBJECT (listitem),
					E_CELL_COMBO_UTF8_KEY,
					g_strdup (utf8_text), g_free);

		elem = elem->next;
	}
}


static gint
e_cell_combo_do_popup			(ECellPopup	*ecp,
					 GdkEvent	*event,
					 int             row,
					 int             view_col)
{
	ECellCombo *ecc = E_CELL_COMBO (ecp);
	guint32 time;
	gint error_code;

	g_signal_handlers_block_by_func(ecc->popup_list, e_cell_combo_selection_changed, ecc);
	e_cell_combo_show_popup (ecc, row, view_col);
	e_cell_combo_select_matching_item (ecc);
	g_signal_handlers_unblock_by_func(ecc->popup_list, e_cell_combo_selection_changed, ecc);

	if (event->type == GDK_BUTTON_PRESS) {
		GTK_LIST (ecc->popup_list)->drag_selection = TRUE;
		time = event->button.time;
	} else {
		time = event->key.time;
	}

	error_code = gdk_pointer_grab (ecc->popup_list->window, TRUE,
				       GDK_ENTER_NOTIFY_MASK |
				       GDK_BUTTON_PRESS_MASK | 
				       GDK_BUTTON_RELEASE_MASK |
				       GDK_POINTER_MOTION_HINT_MASK |
				       GDK_BUTTON1_MOTION_MASK,
				       NULL, NULL, time);
	if (error_code != 0)
		g_warning ("Failed to get pointer grab (%i)", error_code);
	gtk_grab_add (ecc->popup_window);
	gdk_keyboard_grab (ecc->popup_list->window, TRUE, time);

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
	GtkList *list;
	GtkWidget *listitem;
	GList *elem;
	gboolean found = FALSE;
	char *cell_text, *list_item_text;

	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);
	cell_text = e_cell_text_get_text (ecell_text, ecv->e_table_model,
					  ecol->col_idx, ecp->popup_row);

	list = GTK_LIST (ecc->popup_list);
	elem = list->children;
	while (elem) {
		listitem = GTK_WIDGET (elem->data);

		/* We need to compare against the UTF-8 text. */
		list_item_text = g_object_get_data (G_OBJECT (listitem),
						    E_CELL_COMBO_UTF8_KEY);

		if (list_item_text && !strcmp (list_item_text, cell_text)) {
			found = TRUE;
			gtk_list_select_child (list, listitem);
			gtk_widget_grab_focus (listitem);
			break;
		}

		elem = elem->next;
	}

	if (!found) {
		gtk_list_unselect_all (list);
		if (list->children)
			gtk_widget_grab_focus (GTK_WIDGET (list->children->data));
	}

	e_cell_text_free_text (ecell_text, cell_text);
}


static void
e_cell_combo_show_popup			(ECellCombo	*ecc, int row, int view_col)
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

	gtk_widget_set_uposition (ecc->popup_window, x, y);
	gtk_widget_set_usize (ecc->popup_window, width, height);
	gtk_widget_realize (ecc->popup_window);
	gdk_window_resize (ecc->popup_window->window, width, height);
	gtk_widget_show (ecc->popup_window);

	e_cell_popup_set_shown (E_CELL_POPUP (ecc), TRUE);
	d(g_print("%s: popup_shown = TRUE\n", __FUNCTION__));
}


/* Calculates the size and position of the popup window (like GtkCombo). */
static void
e_cell_combo_get_popup_pos		(ECellCombo	*ecc,
					 int             row,
					 int             view_col,
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
		- (int)((GnomeCanvas *)canvas)->layout.vadjustment->value
		+ ((GnomeCanvas *)canvas)->zoom_yofs;

	scrollbar_width = popup->vscrollbar->requisition.width
		+ GTK_SCROLLED_WINDOW_CLASS (GTK_OBJECT_GET_CLASS (popup))->scrollbar_spacing;

	avail_height = gdk_screen_height () - *y;

	/* We'll use the entire screen width if needed, but we save space for
	   the vertical scrollbar in case we need to show that. */
	screen_width = gdk_screen_width ();
	avail_width = screen_width - scrollbar_width;
  
	gtk_widget_size_request (ecc->popup_list, &list_requisition);
	min_height = MIN (list_requisition.height, 
			  popup->vscrollbar->requisition.height);
	if (!GTK_LIST (ecc->popup_list)->children)
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
e_cell_combo_selection_changed(GtkWidget *popup_list, ECellCombo *ecc)
{
	if (!GTK_LIST(popup_list)->selection || !GTK_WIDGET_REALIZED(ecc->popup_window))
		return;

	e_cell_combo_restart_edit (ecc);
}

static gint
e_cell_combo_list_button_press(GtkWidget *popup_list, GdkEvent *event, ECellCombo *ecc)
{
	g_return_val_if_fail (GTK_IS_LIST(popup_list), FALSE);

	e_cell_combo_update_cell (ecc);
	gtk_grab_remove (ecc->popup_window);
	gdk_pointer_ungrab (event->button.time);
	gdk_keyboard_ungrab (event->button.time);
	gtk_widget_hide (ecc->popup_window);

	e_cell_popup_set_shown (E_CELL_POPUP (ecc), FALSE);
	d(g_print("%s: popup_shown = FALSE\n", __FUNCTION__));

	e_cell_combo_restart_edit (ecc);

	return TRUE;

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
		if (event_widget == ecc->popup_list)
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
	while (event_widget && event_widget != ecc->popup_list)
		event_widget = event_widget->parent;

	/* If it wasn't, then we just ignore the event. */
	if (event_widget != ecc->popup_list)
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
static int
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
	GtkList *list = GTK_LIST (ecc->popup_list);
	GtkListItem *listitem;
	gchar *text, *old_text;

	/* Return if no item is selected. */
	if (list->selection == NULL)
		return;

	/* Get the text of the selected item. */
	listitem = list->selection->data;
	text = g_object_get_data (G_OBJECT (listitem),
				  E_CELL_COMBO_UTF8_KEY);
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



