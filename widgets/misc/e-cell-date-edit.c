/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-cell-date-edit.h"

#include <string.h>
#include <time.h>
#include <glib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <gal/util/e-util.h>
#include <gal/e-table/e-table-item.h>
#include <gal/e-table/e-cell-text.h>

#include <libgnomeui/gnome-messagebox.h>
#include <libgnome/gnome-i18n.h>

#include "e-util/e-time-utils.h"
/* This depends on ECalendar which is why I didn't put it in gal. */
#include "e-calendar.h"

static void e_cell_date_edit_class_init		(GtkObjectClass	*object_class);
static void e_cell_date_edit_init		(ECellDateEdit	*ecde);
static void e_cell_date_edit_destroy		(GtkObject	*object);
static void e_cell_date_edit_get_arg		(GtkObject	*o,
						 GtkArg		*arg,
						 guint		 arg_id);
static void e_cell_date_edit_set_arg		(GtkObject	*o,
						 GtkArg		*arg,
						 guint		 arg_id);

static gint e_cell_date_edit_do_popup		(ECellPopup	*ecp,
						 GdkEvent	*event,
						 int             row,
						 int             view_col);
static void e_cell_date_edit_set_popup_values	(ECellDateEdit	*ecde);
static void e_cell_date_edit_select_matching_time(ECellDateEdit	*ecde,
						  char		*time);
static void e_cell_date_edit_show_popup		(ECellDateEdit	*ecde,
						 int             row,
						 int             view_col);
static void e_cell_date_edit_get_popup_pos	(ECellDateEdit	*ecde,
						 int             row,
						 int             view_col,
						 gint		*x,
						 gint		*y,
						 gint		*height,
						 gint		*width);

static void e_cell_date_edit_rebuild_time_list	(ECellDateEdit	*ecde);

static int e_cell_date_edit_key_press		(GtkWidget	*popup_window,
						 GdkEventKey	*event,
						 ECellDateEdit	*ecde);
static int  e_cell_date_edit_button_press	(GtkWidget	*popup_window,
						 GdkEventButton	*event,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_on_ok_clicked	(GtkWidget	*button,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_show_time_invalid_warning	(ECellDateEdit	*ecde);
static void e_cell_date_edit_on_now_clicked	(GtkWidget	*button,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_on_none_clicked	(GtkWidget	*button,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_on_today_clicked	(GtkWidget	*button,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_update_cell	(ECellDateEdit	*ecde,
						 char		*text);
static void e_cell_date_edit_on_time_selected	(GtkList	*list,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_hide_popup		(ECellDateEdit	*ecde);


/* Our arguments. */
enum {
	ARG_0,
	ARG_SHOW_TIME,
	ARG_SHOW_NOW_BUTTON,
	ARG_SHOW_TODAY_BUTTON,
	ARG_ALLOW_NO_DATE_SET,
	ARG_USE_24_HOUR_FORMAT,
	ARG_LOWER_HOUR,
	ARG_UPPER_HOUR
};

static ECellPopupClass *parent_class;


E_MAKE_TYPE (e_cell_date_edit, "ECellDateEdit", ECellDateEdit,
	     e_cell_date_edit_class_init, e_cell_date_edit_init,
	     e_cell_popup_get_type());


static void
e_cell_date_edit_class_init		(GtkObjectClass	*object_class)
{
	ECellPopupClass *ecpc = (ECellPopupClass *) object_class;

	gtk_object_add_arg_type ("ECellDateEdit::show_time",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE,
				 ARG_SHOW_TIME);
	gtk_object_add_arg_type ("ECellDateEdit::show_now_button",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE,
				 ARG_SHOW_NOW_BUTTON);
	gtk_object_add_arg_type ("ECellDateEdit::show_today_button",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE,
				 ARG_SHOW_TODAY_BUTTON);
	gtk_object_add_arg_type ("ECellDateEdit::allow_no_date_set",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE,
				 ARG_ALLOW_NO_DATE_SET);
	gtk_object_add_arg_type ("ECellDateEdit::use_24_hour_format",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE,
				 ARG_USE_24_HOUR_FORMAT);
	gtk_object_add_arg_type ("ECellDateEdit::lower_hour",
				 GTK_TYPE_INT, GTK_ARG_READWRITE,
				 ARG_LOWER_HOUR);
	gtk_object_add_arg_type ("ECellDateEdit::upper_hour",
				 GTK_TYPE_INT, GTK_ARG_READWRITE,
				 ARG_UPPER_HOUR);

	object_class->destroy = e_cell_date_edit_destroy;
	object_class->get_arg = e_cell_date_edit_get_arg;
	object_class->set_arg = e_cell_date_edit_set_arg;

	ecpc->popup = e_cell_date_edit_do_popup;

	parent_class = g_type_class_ref(e_cell_popup_get_type ());
}


static void
e_cell_date_edit_init			(ECellDateEdit	*ecde)
{
	GtkWidget *frame, *vbox, *hbox, *vbox2;
	GtkWidget *scrolled_window, *list, *bbox;
	GtkWidget *now_button, *today_button, *none_button, *ok_button;

	ecde->lower_hour = 0;
	ecde->upper_hour = 24;
	ecde->use_24_hour_format = TRUE;
	ecde->need_time_list_rebuild = TRUE;
	ecde->freeze_count = 0;
	ecde->time_callback = NULL;
	ecde->time_callback_data = NULL;
	ecde->time_callback_destroy = NULL;

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

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show (hbox);

	ecde->calendar = e_calendar_new ();
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (E_CALENDAR (ecde->calendar)->calitem),
			       "move_selection_when_moving", FALSE,
			       NULL);
	gtk_box_pack_start (GTK_BOX (hbox), ecde->calendar, TRUE, TRUE, 0);
	gtk_widget_show (ecde->calendar);

	vbox2 = gtk_vbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);
        gtk_widget_show (vbox2);

	ecde->time_entry = gtk_entry_new ();
	gtk_widget_set_size_request (ecde->time_entry, 50, -1);
	gtk_box_pack_start (GTK_BOX (vbox2), ecde->time_entry,
			    FALSE, FALSE, 0);
	gtk_widget_show (ecde->time_entry);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (vbox2), scrolled_window, TRUE, TRUE, 0);
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
	g_signal_connect((list), "selection-changed",
			    G_CALLBACK (e_cell_date_edit_on_time_selected),
			    ecde);

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
	g_signal_connect((now_button), "clicked",
			    G_CALLBACK (e_cell_date_edit_on_now_clicked),
			    ecde);
	ecde->now_button = now_button;

	today_button = gtk_button_new_with_label (_("Today"));
	gtk_container_add (GTK_CONTAINER (bbox), today_button);
        gtk_widget_show (today_button);
	g_signal_connect((today_button), "clicked",
			    G_CALLBACK (e_cell_date_edit_on_today_clicked),
			    ecde);
	ecde->today_button = today_button;

	none_button = gtk_button_new_with_label (_("None"));
	gtk_container_add (GTK_CONTAINER (bbox), none_button);
        gtk_widget_show (none_button);
	g_signal_connect((none_button), "clicked",
			    G_CALLBACK (e_cell_date_edit_on_none_clicked),
			    ecde);
	ecde->none_button = none_button;

	ok_button = gtk_button_new_with_label (_("OK"));
	gtk_container_add (GTK_CONTAINER (bbox), ok_button);
        gtk_widget_show (ok_button);
	g_signal_connect((ok_button), "clicked",
			    G_CALLBACK (e_cell_date_edit_on_ok_clicked),
			    ecde);


	g_signal_connect((ecde->popup_window),
			    "key_press_event",
			    G_CALLBACK (e_cell_date_edit_key_press),
			    ecde);
	g_signal_connect((ecde->popup_window),
			    "button_press_event",
			    G_CALLBACK (e_cell_date_edit_button_press),
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

	e_cell_date_edit_set_get_time_callback (ecde, NULL, NULL, NULL);

	gtk_widget_destroy (ecde->popup_window);
	ecde->popup_window = NULL;

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static void
e_cell_date_edit_get_arg		(GtkObject	*o,
					 GtkArg		*arg,
					 guint		 arg_id)
{
	ECellDateEdit *ecde;

	ecde = E_CELL_DATE_EDIT (o);

	switch (arg_id) {
	case ARG_SHOW_TIME:
		GTK_VALUE_BOOL (*arg) = GTK_WIDGET_VISIBLE (ecde->time_entry) ? TRUE : FALSE;
		break;
	case ARG_SHOW_NOW_BUTTON:
		GTK_VALUE_BOOL (*arg) = GTK_WIDGET_VISIBLE (ecde->now_button) ? TRUE : FALSE;
		break;
	case ARG_SHOW_TODAY_BUTTON:
		GTK_VALUE_BOOL (*arg) = GTK_WIDGET_VISIBLE (ecde->today_button) ? TRUE : FALSE;
		break;
	case ARG_ALLOW_NO_DATE_SET:
		GTK_VALUE_BOOL (*arg) = GTK_WIDGET_VISIBLE (ecde->none_button) ? TRUE : FALSE;
		break;
	case ARG_USE_24_HOUR_FORMAT:
		GTK_VALUE_BOOL (*arg) = ecde->use_24_hour_format;
		break;
	case ARG_LOWER_HOUR:
		GTK_VALUE_INT (*arg) = ecde->lower_hour;
		break;
	case ARG_UPPER_HOUR:
		GTK_VALUE_INT (*arg) = ecde->upper_hour;
		break;
	default:
		g_warning ("Invalid arg");
	}
}


static void
e_cell_date_edit_set_arg		(GtkObject	*o,
					 GtkArg		*arg,
					 guint		 arg_id)
{
	ECellDateEdit *ecde;
	gint ivalue;
	gboolean bvalue;

	ecde = E_CELL_DATE_EDIT (o);
	
	switch (arg_id){
	case ARG_SHOW_TIME:
		bvalue = GTK_VALUE_BOOL (*arg);
		if (bvalue) {
			gtk_widget_show (ecde->time_entry);
			gtk_widget_show (ecde->time_list);
		} else {
			gtk_widget_hide (ecde->time_entry);
			gtk_widget_hide (ecde->time_list);
		}
		break;
	case ARG_SHOW_NOW_BUTTON:
		bvalue = GTK_VALUE_BOOL (*arg);
		if (bvalue) {
			gtk_widget_show (ecde->now_button);
		} else {
			gtk_widget_hide (ecde->now_button);
		}
		break;
	case ARG_SHOW_TODAY_BUTTON:
		bvalue = GTK_VALUE_BOOL (*arg);
		if (bvalue) {
			gtk_widget_show (ecde->today_button);
		} else {
			gtk_widget_hide (ecde->today_button);
		}
		break;
	case ARG_ALLOW_NO_DATE_SET:
		bvalue = GTK_VALUE_BOOL (*arg);
		if (bvalue) {
			gtk_widget_show (ecde->none_button);
		} else {
			/* FIXME: What if we have no date set now. */
			gtk_widget_hide (ecde->none_button);
		}
		break;
	case ARG_USE_24_HOUR_FORMAT:
		bvalue = GTK_VALUE_BOOL (*arg);
		if (ecde->use_24_hour_format != bvalue) {
			ecde->use_24_hour_format = bvalue;
			ecde->need_time_list_rebuild = TRUE;
		}
		break;
	case ARG_LOWER_HOUR:
		ivalue = GTK_VALUE_INT (*arg);
		ivalue = CLAMP (ivalue, 0, 24);
		if (ecde->lower_hour != ivalue) {
			ecde->lower_hour = ivalue;
			ecde->need_time_list_rebuild = TRUE;
		}
		break;
	case ARG_UPPER_HOUR:
		ivalue = GTK_VALUE_INT (*arg);
		ivalue = CLAMP (ivalue, 0, 24);
		if (ecde->upper_hour != ivalue) {
			ecde->upper_hour = ivalue;
			ecde->need_time_list_rebuild = TRUE;
		}
		break;
	default:
		g_warning ("Invalid arg");
	}

#if 0
	if (ecde->need_time_list_rebuild && ecde->freeze_count == 0)
		e_cell_date_edit_rebuild_time_list (ecde);
#endif
}


static gint
e_cell_date_edit_do_popup		(ECellPopup	*ecp,
					 GdkEvent	*event,
					 int             row,
					 int             view_col)
{
	ECellDateEdit *ecde = E_CELL_DATE_EDIT (ecp);
	guint32 time;

	e_cell_date_edit_show_popup (ecde, row, view_col);
	e_cell_date_edit_set_popup_values (ecde);

	if (event->type == GDK_BUTTON_PRESS) {
		time = event->button.time;
	} else {
		time = event->key.time;
	}

	gtk_grab_add (ecde->popup_window);

	/* Set the focus to the first widget. */
	gtk_widget_grab_focus (ecde->time_entry);
	gdk_window_focus (ecde->popup_window->window, GDK_CURRENT_TIME);

	return TRUE;
}


static void
e_cell_date_edit_set_popup_values	(ECellDateEdit	*ecde)
{
	ECellPopup *ecp = E_CELL_POPUP (ecde);
	ECellText *ecell_text = E_CELL_TEXT (ecp->child);
	ECellView *ecv = (ECellView*) ecp->popup_cell_view;
	ETableItem *eti = E_TABLE_ITEM (ecp->popup_cell_view->cell_view.e_table_item_view);
	ETableCol *ecol;
	char *cell_text;
	ETimeParseStatus status;
	struct tm date_tm;
	GDate date;
	ECalendarItem *calitem;
	char buffer[64];
	gboolean is_date = TRUE;

	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);
	cell_text = e_cell_text_get_text (ecell_text, ecv->e_table_model,
					  ecol->col_idx, ecp->popup_row);

	/* Try to parse just a date first. If the value is only a date, we
	   use a DATE value. */
	status = e_time_parse_date (cell_text, &date_tm);
	if (status == E_TIME_PARSE_INVALID) {
		is_date = FALSE;
		status = e_time_parse_date_and_time (cell_text, &date_tm);
	}

	/* If there is no date and time set, or the date is invalid, we clear
	   the selections, else we select the appropriate date & time. */
	calitem = E_CALENDAR_ITEM (E_CALENDAR (ecde->calendar)->calitem);
	if (status == E_TIME_PARSE_NONE || status == E_TIME_PARSE_INVALID) {
		gtk_entry_set_text (GTK_ENTRY (ecde->time_entry), "");
		e_calendar_item_set_selection (calitem, NULL, NULL);
		gtk_list_unselect_all (GTK_LIST (ecde->time_list));
	} else {
		if (is_date) {
			buffer[0] = '\0';
		} else {
			e_time_format_time (&date_tm, ecde->use_24_hour_format,
					    FALSE, buffer, sizeof (buffer));
		}
		gtk_entry_set_text (GTK_ENTRY (ecde->time_entry), buffer);

		g_date_clear (&date, 1);
		g_date_set_dmy (&date, date_tm.tm_mday, date_tm.tm_mon + 1,
				date_tm.tm_year + 1900);
		e_calendar_item_set_selection (calitem, &date, &date);

		if (is_date) {
			gtk_list_unselect_all (GTK_LIST (ecde->time_list));
		} else {
			e_cell_date_edit_select_matching_time (ecde, buffer);
		}
	}

	e_cell_text_free_text (ecell_text, cell_text);
}


static void
e_cell_date_edit_select_matching_time	(ECellDateEdit	*ecde,
					 char		*time)
{
	GtkList *list;
	GtkWidget *listitem, *label;
	GList *elem;
	gboolean found = FALSE;
	char *list_item_text;

	list = GTK_LIST (ecde->time_list);
	elem = list->children;
	while (elem) {
		listitem = GTK_WIDGET (elem->data);
		label = GTK_BIN (listitem)->child;
		gtk_label_get (GTK_LABEL (label), &list_item_text);

		if (!strcmp (list_item_text, time)) {
			found = TRUE;
			gtk_list_select_child (list, listitem);
			break;
		}

		elem = elem->next;
	}

	if (!found)
		gtk_list_unselect_all (list);
}


static void
e_cell_date_edit_show_popup		(ECellDateEdit	*ecde,
					 int             row,
					 int             view_col)
{
	gint x, y, width, height, old_width, old_height;

	if (ecde->need_time_list_rebuild)
		e_cell_date_edit_rebuild_time_list (ecde);

	/* This code is practically copied from GtkCombo. */
	old_width = ecde->popup_window->allocation.width;
	old_height  = ecde->popup_window->allocation.height;

	e_cell_date_edit_get_popup_pos (ecde, row, view_col, &x, &y, &height, &width);

	gtk_widget_set_uposition (ecde->popup_window, x, y);
	gtk_widget_set_size_request (ecde->popup_window, width, height);
	gtk_widget_realize (ecde->popup_window);
	gdk_window_resize (ecde->popup_window->window, width, height);
	gtk_widget_show (ecde->popup_window);

	e_cell_popup_set_shown (E_CELL_POPUP (ecde), TRUE);
}


/* Calculates the size and position of the popup window (like GtkCombo). */
static void
e_cell_date_edit_get_popup_pos		(ECellDateEdit	*ecde,
					 int             row,
					 int             view_col,
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
	double x1, y1, wx, wy;
  
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
	/* The ETable positions don't include the grid lines, I think, so we
	   add 1. */
	*y += y1 + 1
		- (int)((GnomeCanvas *)canvas)->layout.vadjustment->value
		+ ((GnomeCanvas *)canvas)->zoom_yofs;

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
	/* If the Escape key is pressed we hide the popup. */
	if (event->keyval != GDK_Escape)
		return FALSE;

	e_cell_date_edit_hide_popup (ecde);

	return TRUE;
}


/* This handles button press events in the popup window. If the button is
   pressed outside the popup, we hide it and do not change the cell contents.
*/
static int
e_cell_date_edit_button_press		(GtkWidget	*popup_window,
					 GdkEventButton	*event,
					 ECellDateEdit	*ecde)
{
	GtkWidget *event_widget;

	event_widget = gtk_get_event_widget ((GdkEvent*) event);
	if (gtk_widget_get_toplevel (event_widget) != popup_window) {
		e_cell_date_edit_hide_popup (ecde);
	}

	return TRUE;
}


/* Clears the time list and rebuilds it using the lower_hour, upper_hour
   and use_24_hour_format settings. */
static void
e_cell_date_edit_rebuild_time_list		(ECellDateEdit	*ecde)
{
	GtkList *list;
	GtkWidget *listitem;
	char buffer[40];
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
			e_time_format_time (&tmp_tm, ecde->use_24_hour_format,
					    FALSE, buffer, sizeof (buffer));
			listitem = gtk_list_item_new_with_label (buffer);
			gtk_widget_show (listitem);
			gtk_container_add (GTK_CONTAINER (list), listitem);
		}
	}

	ecde->need_time_list_rebuild = FALSE;
}


static void
e_cell_date_edit_on_ok_clicked		(GtkWidget	*button,
					 ECellDateEdit	*ecde)
{
	ECalendarItem *calitem;
	GDate start_date, end_date;
	gboolean day_selected;
	struct tm date_tm;
	char buffer[64];
	const char *text;
	ETimeParseStatus status;
	gboolean is_date = FALSE;

	calitem = E_CALENDAR_ITEM (E_CALENDAR (ecde->calendar)->calitem);
	day_selected = e_calendar_item_get_selection (calitem, &start_date,
						      &end_date);

	text = gtk_entry_get_text (GTK_ENTRY (ecde->time_entry));
	status = e_time_parse_time (text, &date_tm);
	if (status == E_TIME_PARSE_INVALID) {
		e_cell_date_edit_show_time_invalid_warning (ecde);
		return;
	} else if (status == E_TIME_PARSE_NONE) {
		is_date = TRUE;
	}

	if (day_selected) {
		date_tm.tm_year = g_date_get_year (&start_date) - 1900;
		date_tm.tm_mon = g_date_get_month (&start_date) - 1;
		date_tm.tm_mday = g_date_get_day (&start_date);
		/* We need to call this to set the weekday. */
		mktime (&date_tm);
		e_time_format_date_and_time (&date_tm,
					     ecde->use_24_hour_format,
					     !is_date, FALSE,
					     buffer, sizeof (buffer));
	} else {
		buffer[0] = '\0';
	}

	e_cell_date_edit_update_cell (ecde, buffer);
	e_cell_date_edit_hide_popup (ecde);
}


static void
e_cell_date_edit_show_time_invalid_warning	(ECellDateEdit	*ecde)
{
	GtkWidget *dialog;
	struct tm date_tm;
	char buffer[64], *message;

	/* Create a useful error message showing the correct format. */
	date_tm.tm_year = 100;
	date_tm.tm_mon = 0;
	date_tm.tm_mday = 1;
	date_tm.tm_hour = 1;
	date_tm.tm_min = 30;
	date_tm.tm_sec = 0;
	date_tm.tm_isdst = -1;
	e_time_format_time (&date_tm, ecde->use_24_hour_format, FALSE,
			    buffer, sizeof (buffer));

	message = g_strdup_printf (_("The time must be in the format: %s"),
				   buffer);

	dialog = gnome_message_box_new (message, GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	/* FIXME: Fix transient settings - I'm not sure it works with popup
	   windows. Maybe we need to use a normal window without decorations.*/
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (ecde->popup_window));
	gnome_dialog_run (GNOME_DIALOG (dialog));

	g_free (message);
}


static void
e_cell_date_edit_on_now_clicked		(GtkWidget	*button,
					 ECellDateEdit	*ecde)
{
	struct tm tmp_tm;
	time_t t;
	char buffer[64];

	if (ecde->time_callback) {
		tmp_tm = (*ecde->time_callback) (ecde, ecde->time_callback_data);
	} else {
		t = time (NULL);
		tmp_tm = *localtime (&t);
	}
	e_time_format_date_and_time (&tmp_tm,
				     ecde->use_24_hour_format,
				     TRUE, FALSE,
				     buffer, sizeof (buffer));

	e_cell_date_edit_update_cell (ecde, buffer);
	e_cell_date_edit_hide_popup (ecde);
}


static void
e_cell_date_edit_on_none_clicked	(GtkWidget	*button,
					 ECellDateEdit	*ecde)
{
	e_cell_date_edit_update_cell (ecde, "");
	e_cell_date_edit_hide_popup (ecde);
}


static void
e_cell_date_edit_on_today_clicked	(GtkWidget	*button,
					 ECellDateEdit	*ecde)
{
	struct tm tmp_tm;
	time_t t;
	char buffer[64];

	if (ecde->time_callback) {
		tmp_tm = (*ecde->time_callback) (ecde, ecde->time_callback_data);
	} else {
		t = time (NULL);
		tmp_tm = *localtime (&t);
	}

	tmp_tm.tm_hour = 0;
	tmp_tm.tm_min = 0;
	tmp_tm.tm_sec = 0;
	e_time_format_date_and_time (&tmp_tm,
				     ecde->use_24_hour_format,
				     FALSE, FALSE,
				     buffer, sizeof (buffer));

	e_cell_date_edit_update_cell (ecde, buffer);
	e_cell_date_edit_hide_popup (ecde);
}


static void
e_cell_date_edit_update_cell		(ECellDateEdit	*ecde,
					 char		*text)
{
	ECellPopup *ecp = E_CELL_POPUP (ecde);
	ECellText *ecell_text = E_CELL_TEXT (ecp->child);
	ECellView *ecv = (ECellView*) ecp->popup_cell_view;
	ETableItem *eti = E_TABLE_ITEM (ecv->e_table_item_view);
	ETableCol *ecol;
	gchar *old_text;

	/* Compare the new text with the existing cell contents. */
	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);

	old_text = e_cell_text_get_text (ecell_text, ecv->e_table_model,
					 ecol->col_idx, ecp->popup_row);

	/* If they are different, update the cell contents. */
	if (strcmp (old_text, text)) {
		e_cell_text_set_value (ecell_text, ecv->e_table_model,
				       ecol->col_idx, ecp->popup_row, text);
	}

	e_cell_text_free_text (ecell_text, old_text);
}


static void
e_cell_date_edit_on_time_selected	(GtkList	*list,
					 ECellDateEdit	*ecde)
{
	GtkWidget *listitem, *label;
	char *list_item_text;

	if (!list->selection)
		return;

	listitem = list->selection->data;
	label = GTK_BIN (listitem)->child;
	gtk_label_get (GTK_LABEL (label), &list_item_text);
	gtk_entry_set_text (GTK_ENTRY (ecde->time_entry), list_item_text);
}


static void
e_cell_date_edit_hide_popup		(ECellDateEdit	*ecde)
{
	gtk_grab_remove (ecde->popup_window);
	gtk_widget_hide (ecde->popup_window);
	e_cell_popup_set_shown (E_CELL_POPUP (ecde), FALSE);
}


/* These freeze and thaw the rebuilding of the time list. They are useful when
   setting several properties which result in rebuilds of the list, e.g. the
   lower_hour, upper_hour and use_24_hour_format properties. */
void
e_cell_date_edit_freeze			(ECellDateEdit	*ecde)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	ecde->freeze_count++;
}


void
e_cell_date_edit_thaw			(ECellDateEdit	*ecde)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	if (ecde->freeze_count > 0) {
		ecde->freeze_count--;

		if (ecde->freeze_count == 0)
			e_cell_date_edit_rebuild_time_list (ecde);
	}
}


/* Sets a callback to use to get the current time. This is useful if the
   application needs to use its own timezone data rather than rely on the
   Unix timezone. */
void
e_cell_date_edit_set_get_time_callback (ECellDateEdit	*ecde,
					ECellDateEditGetTimeCallback cb,
					gpointer	 data,
					GtkDestroyNotify destroy)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	if (ecde->time_callback_data && ecde->time_callback_destroy)
		(*ecde->time_callback_destroy) (ecde->time_callback_data);

	ecde->time_callback = cb;
	ecde->time_callback_data = data;
	ecde->time_callback_destroy = destroy;
}
