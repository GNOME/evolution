/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ECellDateEdit - a subclass of ECellPopup used to show a date with a popup
 * window to edit it.
 */

#include <config.h>
#include <gal/util/e-i18n.h>
#include <gdk/gdkkeysyms.h>
#include "gal/util/e-util.h"
#include <gal/e-table/e-table-item.h>
#include "e-cell-date-edit.h"

/* This depends on ECalendar which is why I didn't put it in gal. */
#include "e-calendar.h"

static void e_cell_date_edit_class_init		(GtkObjectClass	*object_class);
static void e_cell_date_edit_init		(ECellDateEdit	*ecde);
static void e_cell_date_edit_destroy		(GtkObject	*object);

static gint e_cell_date_edit_do_popup		(ECellPopup	*ecp,
						 GdkEvent	*event);
static void e_cell_date_edit_show_popup		(ECellDateEdit	*ecde);
static void e_cell_date_edit_get_popup_pos	(ECellDateEdit	*ecde,
						 gint		*x,
						 gint		*y,
						 gint		*height,
						 gint		*width);

static void e_cell_date_edit_rebuild_time_list	(ECellDateEdit	*ecde);

static int e_cell_date_edit_key_press		(GtkWidget	*popup_window,
						 GdkEventKey	*event,
						 ECellDateEdit	*ecde);

static ECellPopupClass *parent_class;


E_MAKE_TYPE (e_cell_date_edit, "ECellDateEdit", ECellDateEdit,
	     e_cell_date_edit_class_init, e_cell_date_edit_init,
	     e_cell_popup_get_type());


static void
e_cell_date_edit_class_init		(GtkObjectClass	*object_class)
{
	ECellPopupClass *ecpc = (ECellPopupClass *) object_class;

	object_class->destroy = e_cell_date_edit_destroy;

	ecpc->popup = e_cell_date_edit_do_popup;

	parent_class = gtk_type_class (e_cell_popup_get_type ());
}


static void
e_cell_date_edit_init			(ECellDateEdit	*ecde)
{
	GtkWidget *frame, *vbox, *table, *label, *entry;
	GtkWidget *calendar, *scrolled_window, *list, *bbox;
	GtkWidget *now_button, *today_button, *none_button;

	ecde->use_24_hour_format = TRUE;

	ecde->lower_hour = 0;
	ecde->upper_hour = 24;

	/* We create one popup window for the ECell, since there will only
	   ever be one popup in use at a time. */
	ecde->popup_window = gtk_window_new (GTK_WINDOW_POPUP);

	gtk_window_set_policy (GTK_WINDOW (ecde->popup_window),
			       TRUE, TRUE, FALSE);
  
	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (ecde->popup_window), frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
        gtk_widget_show (vbox);

	table = gtk_table_new (3, 2, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
        gtk_widget_show (table);

#if 0
	label = gtk_label_new (_("Date:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_widget_show (label);

	entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), entry, 0, 1, 1, 2,
			  GTK_FILL, 0, 0, 0);
	gtk_widget_show (entry);
#endif

	calendar = e_calendar_new ();
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (E_CALENDAR (calendar)->calitem),
			       "move_selection_when_moving", FALSE,
			       NULL);
	gtk_table_attach (GTK_TABLE (table), calendar, 0, 1, 0, 3,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (calendar);


#if 0
	label = gtk_label_new (_("Time:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_widget_show (label);

	entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_widget_show (entry);
#endif

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_table_attach (GTK_TABLE (table), scrolled_window, 1, 2, 0, 3,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_NEVER,
					GTK_POLICY_ALWAYS);
	gtk_widget_show (scrolled_window);

	list = gtk_list_new ();
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), list);
	gtk_container_set_focus_vadjustment (GTK_CONTAINER (list),
					     gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
	gtk_widget_show (list);
	ecde->time_list = list;
	e_cell_date_edit_rebuild_time_list (ecde);

	bbox = gtk_hbutton_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (bbox), 4);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (bbox), 2);
	gtk_button_box_set_child_ipadding (GTK_BUTTON_BOX (bbox), 2, 0);
	gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), 0, 0);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
        gtk_widget_show (bbox);

	now_button = gtk_button_new_with_label (_("Now"));
	gtk_container_add (GTK_CONTAINER (bbox), now_button);
        gtk_widget_show (now_button);
#if 0
	gtk_signal_connect (GTK_OBJECT (now_button), "clicked",
			    GTK_SIGNAL_FUNC (on_date_popup_now_button_clicked), dedit);
#endif

	today_button = gtk_button_new_with_label (_("Today"));
	gtk_container_add (GTK_CONTAINER (bbox), today_button);
        gtk_widget_show (today_button);
#if 0
	gtk_signal_connect (GTK_OBJECT (today_button), "clicked",
			    GTK_SIGNAL_FUNC (on_date_popup_today_button_clicked), dedit);
#endif

	/* Note that we don't show this here, since by default a 'None' date
	   is not permitted. */
	none_button = gtk_button_new_with_label (_("None"));
	gtk_container_add (GTK_CONTAINER (bbox), none_button);
#if 0
	gtk_signal_connect (GTK_OBJECT (none_button), "clicked",
			    GTK_SIGNAL_FUNC (on_date_popup_none_button_clicked), dedit);
#endif



	gtk_signal_connect (GTK_OBJECT (ecde->popup_window),
			    "key_press_event",
			    GTK_SIGNAL_FUNC (e_cell_date_edit_key_press),
			    ecde);
}


/**
 * e_cell_date_edit_new:
 *
 * Creates a new ECellDateEdit renderer.
 *
 * Returns: an ECellDateEdit object.
 */
ECell *
e_cell_date_edit_new			(void)
{
	ECellDateEdit *ecde = gtk_type_new (e_cell_date_edit_get_type ());

	return (ECell*) ecde;
}


/*
 * GtkObject::destroy method
 */
static void
e_cell_date_edit_destroy		(GtkObject *object)
{
	ECellDateEdit *ecde = E_CELL_DATE_EDIT (object);

	gtk_widget_unref (ecde->popup_window);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static gint
e_cell_date_edit_do_popup		(ECellPopup	*ecp,
					 GdkEvent	*event)
{
	ECellDateEdit *ecde = E_CELL_DATE_EDIT (ecp);
	guint32 time;
	gint error_code;

	e_cell_date_edit_show_popup (ecde);

	if (event->type == GDK_BUTTON_PRESS) {
		time = event->button.time;
	} else {
		time = event->key.time;
	}

	error_code = gdk_pointer_grab (ecde->popup_window->window, TRUE,
				       GDK_ENTER_NOTIFY_MASK |
				       GDK_BUTTON_PRESS_MASK | 
				       GDK_BUTTON_RELEASE_MASK |
				       GDK_POINTER_MOTION_HINT_MASK |
				       GDK_BUTTON1_MOTION_MASK,
				       NULL, NULL, time);
	if (error_code != 0)
		g_warning ("Failed to get pointer grab");

	gtk_grab_add (ecde->popup_window);

	return TRUE;
}


static void
e_cell_date_edit_show_popup		(ECellDateEdit	*ecde)
{
	gint x, y, width, height, old_width, old_height;

	g_print ("In e_cell_popup_popup_list\n");

	/* This code is practically copied from GtkCombo. */
	old_width = ecde->popup_window->allocation.width;
	old_height  = ecde->popup_window->allocation.height;

	e_cell_date_edit_get_popup_pos (ecde, &x, &y, &height, &width);

	gtk_widget_set_uposition (ecde->popup_window, x, y);
	gtk_widget_set_usize (ecde->popup_window, width, height);
	gtk_widget_realize (ecde->popup_window);
	gdk_window_resize (ecde->popup_window->window, width, height);
	gtk_widget_show (ecde->popup_window);

	/* FIXME: Set the focus to the first widget. */
	gtk_widget_grab_focus (ecde->popup_window);

	E_CELL_POPUP (ecde)->popup_shown = TRUE;
}


/* Calculates the size and position of the popup window (like GtkCombo). */
static void
e_cell_date_edit_get_popup_pos		(ECellDateEdit	*ecde,
					 gint		*x,
					 gint		*y,
					 gint		*height,
					 gint		*width)
{
	ECellPopup *ecp = E_CELL_POPUP (ecde);
	ETableItem *eti = E_TABLE_ITEM (ecp->popup_cell_view->cell_view.e_table_item_view);
	GtkWidget *canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (eti)->canvas);
	GtkRequisition popup_requisition;
	gint avail_height, screen_width, column_width, row_height;
	double x1, y1;
  
	gdk_window_get_origin (canvas->window, x, y);

	x1 = e_table_header_col_diff (eti->header, 0, eti->editing_col + 1);
	y1 = eti_row_diff (eti, 0, eti->editing_row + 1);
	column_width = e_table_header_col_diff (eti->header, eti->editing_col,
						eti->editing_col + 1);
	row_height = eti_row_diff (eti, eti->editing_row,
				   eti->editing_row + 1);
	gnome_canvas_item_i2w (GNOME_CANVAS_ITEM (eti), &x1, &y1);

	*x += x1;
	/* The ETable positions don't include the grid lines, I think, so we
	   add 1. */
	*y += y1 + 1;

	avail_height = gdk_screen_height () - *y;

	/* We'll use the entire screen width if needed, but we save space for
	   the vertical scrollbar in case we need to show that. */
	screen_width = gdk_screen_width ();
  
	gtk_widget_size_request (ecde->popup_window, &popup_requisition);
  
	/* Calculate the desired width. */
	*width = popup_requisition.width;

	/* Use at least the same width as the column. */
	if (*width < column_width)
		*width = column_width;

	/* Check if it fits in the available height. */
	if (popup_requisition.height > avail_height) {
		/* It doesn't fit, so we see if we have the minimum space
		   needed. */
		if (*y - row_height > avail_height) {
			/* We don't, so we show the popup above the cell
			   instead of below it. */
			avail_height = *y - row_height;
			*y -= (popup_requisition.height + row_height);
			if (*y < 0)
				*y = 0;
		}
	}

	/* We try to line it up with the right edge of the column, but we don't
	   want it to go off the edges of the screen. */
	if (*x > screen_width)
		*x = screen_width;
	*x -= *width;
	if (*x < 0)
		*x = 0;

	*height = popup_requisition.height;
}


/* This handles key press events in the popup window. If the Escape key is
   pressed we hide the popup, and do not change the cell contents. */
static int
e_cell_date_edit_key_press		(GtkWidget	*popup_window,
					 GdkEventKey	*event,
					 ECellDateEdit	*ecde)
{
	g_print ("In e_cell_date_edit_key_press\n");

	/* If the Escape key is pressed we hide the popup. */
	if (event->keyval != GDK_Escape)
		return FALSE;

	gtk_grab_remove (ecde->popup_window);
	gdk_pointer_ungrab (event->time);
	gtk_widget_hide (ecde->popup_window);

	E_CELL_POPUP (ecde)->popup_shown = FALSE;

	return TRUE;
}


/* Clears the time list and rebuilds it using the lower_hour, upper_hour
   and use_24_hour_format settings. */
static void
e_cell_date_edit_rebuild_time_list		(ECellDateEdit	*ecde)
{
	GtkList *list;
	GtkWidget *listitem;
	char buffer[40], *format;
	struct tm tmp_tm;
	gint hour, min;

	list = GTK_LIST (ecde->time_list);

	gtk_list_clear_items (list, 0, -1);

	/* Fill the struct tm with some sane values. */
	tmp_tm.tm_year = 2000;
	tmp_tm.tm_mon = 0;
	tmp_tm.tm_mday = 1;
	tmp_tm.tm_sec  = 0;
	tmp_tm.tm_isdst = 0;

	for (hour = ecde->lower_hour; hour <= ecde->upper_hour; hour++) {

		/* We don't want to display midnight at the end, since that is
		   really in the next day. */
		if (hour == 24)
			break;

		/* We want to finish on upper_hour, with min == 0. */
		for (min = 0;
		     min == 0 || (min < 60 && hour != ecde->upper_hour);
		     min += 30) {
			tmp_tm.tm_hour = hour;
			tmp_tm.tm_min  = min;

			if (ecde->use_24_hour_format)
				/* This is a strftime() format. %H = hour (0-23), %M = minute. */
				format = _("%H:%M");
			else
				/* This is a strftime() format. %I = hour (1-12), %M = minute, %p = am/pm string. */
				format = _("%I:%M %p");

			strftime (buffer, sizeof (buffer), format, &tmp_tm);

			listitem = gtk_list_item_new_with_label (buffer);
			gtk_widget_show (listitem);
			gtk_container_add (GTK_CONTAINER (list), listitem);
		}
	}
}


