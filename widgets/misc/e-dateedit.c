/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 *
 * Based on the GnomeDateEdit, part of the Gnome Library.
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
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
 * EDateEdit - a widget based on GnomeDateEdit to provide a date & optional
 * time field with popups for entering a date.
 */

/* We need this for strptime. */
#define _XOPEN_SOURCE

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gtk/gtkmain.h>
#include "e-dateedit.h"
#include "e-calendar.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkwindow.h>

struct _EDateEditPrivate {
	GtkWidget *date_entry;
	GtkWidget *date_button;
	
	GtkWidget *space;

	GtkWidget *time_combo;

	GtkWidget *cal_popup;
	GtkWidget *calendar;
	GtkWidget *now_button;
	GtkWidget *today_button;
	GtkWidget *none_button;

	gboolean show_time;
	gboolean use_24_hour_format;

	gint lower_hour;
	gint upper_hour;
};

enum {
	DATE_CHANGED,
	TIME_CHANGED,
	LAST_SIGNAL
};


static gint date_edit_signals [LAST_SIGNAL] = { 0 };


static void e_date_edit_class_init	(EDateEditClass	*class);
static void e_date_edit_init		(EDateEdit	*dedit);
static void create_children		(EDateEdit	*dedit);
static void e_date_edit_destroy		(GtkObject	*object);
static void e_date_edit_forall		(GtkContainer   *container,
					 gboolean	 include_internals,
					 GtkCallback     callback,
					 gpointer	 callback_data);

static void on_date_button_clicked	(GtkWidget	*widget,
					 EDateEdit	*dedit);
static void position_date_popup		(EDateEdit	*dedit);
static void on_date_popup_none_button_clicked	(GtkWidget	*button,
						 EDateEdit	*dedit);
static void on_date_popup_today_button_clicked	(GtkWidget	*button,
						 EDateEdit	*dedit);
static void on_date_popup_now_button_clicked	(GtkWidget	*button,
						 EDateEdit	*dedit);
static gint on_date_popup_delete_event	(GtkWidget	*widget,
					 gpointer	 data);
static gint on_date_popup_key_press	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static gint on_date_popup_button_press	(GtkWidget	*widget,
					 GdkEventButton *event,
					 gpointer	 data);
static void on_date_popup_date_selected (ECalendarItem	*calitem,
					 EDateEdit	*dedit);
static void hide_date_popup		(EDateEdit	*dedit);
static void rebuild_time_popup		(EDateEdit	*dedit);
static void enable_time_combo		(EDateEdit	*dedit);
static void disable_time_combo		(EDateEdit	*dedit);
static gboolean date_is_none		(char		*date_text);


static GtkHBoxClass *parent_class;

/**
 * e_date_edit_get_type:
 *
 * Returns the GtkType for the EDateEdit widget
 */
guint
e_date_edit_get_type (void)
{
	static guint date_edit_type = 0;

	if (!date_edit_type){
		GtkTypeInfo date_edit_info = {
			"EDateEdit",
			sizeof (EDateEdit),
			sizeof (EDateEditClass),
			(GtkClassInitFunc) e_date_edit_class_init,
			(GtkObjectInitFunc) e_date_edit_init,
			NULL,
			NULL,
		};

		date_edit_type = gtk_type_unique (gtk_hbox_get_type (), &date_edit_info);
	}
	
	return date_edit_type;
}


static void
e_date_edit_class_init (EDateEditClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass *) class;
	GtkContainerClass *container_class = (GtkContainerClass *) class;

	object_class = (GtkObjectClass*) class;

	parent_class = gtk_type_class (gtk_hbox_get_type ());

	date_edit_signals [TIME_CHANGED] =
		gtk_signal_new ("time_changed",
				GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (EDateEditClass,
						   time_changed),
				gtk_signal_default_marshaller,
				GTK_TYPE_NONE, 0);
	
	date_edit_signals [DATE_CHANGED] =
		gtk_signal_new ("date_changed",
				GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (EDateEditClass,
						   date_changed),
				gtk_signal_default_marshaller,
				GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals (object_class, date_edit_signals,
				      LAST_SIGNAL);

	container_class->forall = e_date_edit_forall;

	object_class->destroy = e_date_edit_destroy;

	class->date_changed = NULL;
	class->time_changed = NULL;
}


static void
e_date_edit_init (EDateEdit *dedit)
{
	EDateEditPrivate *priv;

	dedit->_priv = priv = g_new0 (EDateEditPrivate, 1);

	priv->show_time = TRUE;
	priv->use_24_hour_format = TRUE;

	priv->lower_hour = 0;
	priv->upper_hour = 24;
}


/**
 * e_date_edit_new:
 *
 * Description: Creates a new #EDateEdit widget which can be used
 * to provide an easy to use way for entering dates and times.
 * 
 * Returns: a new #EDateEdit widget.
 */
GtkWidget *
e_date_edit_new (void)
{
	EDateEdit *dedit;

	dedit = gtk_type_new (e_date_edit_get_type ());

	create_children (dedit);
	e_date_edit_set_time (dedit, 0);

	return GTK_WIDGET (dedit);
}


static void
create_children (EDateEdit *dedit)
{
	EDateEditPrivate *priv;
	ECalendar *calendar;
	GtkWidget *frame, *arrow;
	GtkWidget *vbox, *bbox;

	priv = dedit->_priv;

	priv->date_entry  = gtk_entry_new ();
	gtk_widget_set_usize (priv->date_entry, 90, 0);
	gtk_box_pack_start (GTK_BOX (dedit), priv->date_entry, FALSE, TRUE, 0);
	gtk_widget_show (priv->date_entry);
	
	priv->date_button = gtk_button_new ();
	gtk_signal_connect (GTK_OBJECT (priv->date_button), "clicked",
			    GTK_SIGNAL_FUNC (on_date_button_clicked), dedit);
	gtk_box_pack_start (GTK_BOX (dedit), priv->date_button,
			    FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (priv->date_button), arrow);
	gtk_widget_show (arrow);

	gtk_widget_show (priv->date_button);

	/* This is just to create a space between the date & time parts. */
	priv->space = gtk_drawing_area_new ();
	gtk_box_pack_start (GTK_BOX (dedit), priv->space, FALSE, FALSE, 2);


	priv->time_combo = gtk_combo_new ();
	gtk_widget_set_usize (GTK_COMBO (priv->time_combo)->entry, 90, 0);
	gtk_box_pack_start (GTK_BOX (dedit), priv->time_combo, FALSE, TRUE, 0);
	rebuild_time_popup (dedit);

	if (priv->show_time) {
		gtk_widget_show (priv->space);
		gtk_widget_show (priv->time_combo);
	}

	priv->cal_popup = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_events (priv->cal_popup,
			       gtk_widget_get_events (priv->cal_popup)
			       | GDK_KEY_PRESS_MASK);
	gtk_signal_connect (GTK_OBJECT (priv->cal_popup), "delete_event",
			    (GtkSignalFunc) on_date_popup_delete_event,
			    dedit);
	gtk_signal_connect (GTK_OBJECT (priv->cal_popup), "key_press_event",
			    (GtkSignalFunc) on_date_popup_key_press,
			    dedit);
	gtk_signal_connect (GTK_OBJECT (priv->cal_popup), "button_press_event",
			    (GtkSignalFunc) on_date_popup_button_press,
			    dedit);
	gtk_window_set_policy (GTK_WINDOW (priv->cal_popup),
			       FALSE, FALSE, TRUE);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (priv->cal_popup), frame);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
        gtk_widget_show (vbox);

	priv->calendar = e_calendar_new ();
	calendar = E_CALENDAR (priv->calendar);
	/*e_calendar_set_buttons (calendar, TRUE, TRUE);*/
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (calendar->calitem),
			       "maximum_days_selected", 1,
			       "move_selection_when_moving", FALSE,
			       NULL);

	gtk_signal_connect (GTK_OBJECT (calendar->calitem),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (on_date_popup_date_selected), dedit);

	gtk_box_pack_start (GTK_BOX (vbox), priv->calendar, FALSE, FALSE, 0);
        gtk_widget_show (priv->calendar);

	bbox = gtk_hbutton_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (bbox), 4);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (bbox), 2);
	gtk_button_box_set_child_ipadding (GTK_BUTTON_BOX (bbox), 2, 0);
	gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), 0, 0);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
        gtk_widget_show (bbox);

	priv->now_button = gtk_button_new_with_label (_("Now"));
	gtk_container_add (GTK_CONTAINER (bbox), priv->now_button);
        gtk_widget_show (priv->now_button);
	gtk_signal_connect (GTK_OBJECT (priv->now_button), "clicked",
			    GTK_SIGNAL_FUNC (on_date_popup_now_button_clicked), dedit);

	priv->today_button = gtk_button_new_with_label (_("Today"));
	gtk_container_add (GTK_CONTAINER (bbox), priv->today_button);
        gtk_widget_show (priv->today_button);
	gtk_signal_connect (GTK_OBJECT (priv->today_button), "clicked",
			    GTK_SIGNAL_FUNC (on_date_popup_today_button_clicked), dedit);

	priv->none_button = gtk_button_new_with_label (_("None"));
	gtk_container_add (GTK_CONTAINER (bbox), priv->none_button);
	gtk_signal_connect (GTK_OBJECT (priv->none_button), "clicked",
			    GTK_SIGNAL_FUNC (on_date_popup_none_button_clicked), dedit);
}


static void
e_date_edit_destroy (GtkObject *object)
{
	EDateEdit *dedit;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_DATE_EDIT (object));

	dedit = E_DATE_EDIT (object);

	gtk_widget_destroy (dedit->_priv->cal_popup);
	gtk_widget_unref (dedit->_priv->cal_popup);

	g_free(dedit->_priv);
	dedit->_priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
e_date_edit_forall (GtkContainer *container, gboolean include_internals,
		    GtkCallback callback, gpointer callback_data)
{
	g_return_if_fail (container != NULL);
	g_return_if_fail (E_IS_DATE_EDIT (container));
	g_return_if_fail (callback != NULL);

	/* Let GtkBox handle things only if the internal widgets need to be
	 * poked.
	 */
	if (include_internals)
		if (GTK_CONTAINER_CLASS (parent_class)->forall)
			(* GTK_CONTAINER_CLASS (parent_class)->forall)
				(container,
				 include_internals,
				 callback,
				 callback_data);
}


/* The arrow button beside the date field has been clicked, so we show the
   popup with the ECalendar in. */
static void
on_date_button_clicked (GtkWidget *widget, EDateEdit *dedit)
{
	EDateEditPrivate *priv;
	ECalendar *calendar;
	struct tm mtm;
	gchar *date_text, *status;
	GDate selected_day;
	gboolean clear_selection = FALSE;

	priv = dedit->_priv;
	calendar = E_CALENDAR (priv->calendar);

	date_text = gtk_entry_get_text (GTK_ENTRY (dedit->_priv->date_entry));
	if (date_is_none (date_text)) {
		clear_selection = TRUE;
	} else {
		status = strptime (date_text, "%x", &mtm);
		if (!status || !status[0])
			clear_selection = TRUE;
	}

	if (clear_selection) {
		e_calendar_item_set_selection (calendar->calitem, NULL, NULL);
	} else {
		g_date_clear (&selected_day, 1);
		g_date_set_dmy (&selected_day, mtm.tm_mday, mtm.tm_mon + 1,
				mtm.tm_year + 1900);
		e_calendar_item_set_selection (calendar->calitem,
					       &selected_day, NULL);
	}

	/* FIXME: Hack. Change ECalendarItem so it doesn't queue signal
	   emissions. */
	calendar->calitem->selection_changed = FALSE;

        position_date_popup (dedit);
       
	gtk_widget_realize (dedit->_priv->cal_popup);
	gtk_widget_show (dedit->_priv->cal_popup);

	gtk_widget_grab_focus (dedit->_priv->cal_popup);

	gtk_grab_add (dedit->_priv->cal_popup);

	gdk_pointer_grab (dedit->_priv->cal_popup->window, TRUE,
			  (GDK_BUTTON_PRESS_MASK
			   | GDK_BUTTON_RELEASE_MASK
			   | GDK_POINTER_MOTION_MASK),
			  NULL, NULL, GDK_CURRENT_TIME);
}


/* This positions the date popup below and to the left of the arrow button,
   just before it is shown. */
static void
position_date_popup (EDateEdit *dedit)
{
	gint x, y;
	gint bwidth, bheight;
	GtkRequisition req;
	gint screen_width, screen_height;

	gtk_widget_size_request (dedit->_priv->cal_popup, &req);

	gdk_window_get_origin (dedit->_priv->date_button->window, &x, &y);
	gdk_window_get_size (dedit->_priv->date_button->window,
			     &bwidth, &bheight);

      	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

	x += bwidth - req.width;
	y += bheight;

	x = CLAMP (x, 0, MAX (0, screen_width - req.width));
	y = CLAMP (y, 0, MAX (0, screen_height - req.height));

	gtk_widget_set_uposition (dedit->_priv->cal_popup, x, y);
}


/* A date has been selected in the date popup, so we set the date field
   and hide the popup. */
static void
on_date_popup_date_selected (ECalendarItem *calitem, EDateEdit *dedit)
{
	GDate start_date, end_date;
	struct tm tmp_tm;
	char buffer [40];

	if (!e_calendar_item_get_selection (calitem, &start_date, &end_date))
		return;

	g_date_to_struct_tm (&start_date, &tmp_tm);

	strftime (buffer, sizeof (buffer), "%x", &tmp_tm);
	gtk_entry_set_text (GTK_ENTRY (dedit->_priv->date_entry), buffer);

	enable_time_combo (dedit);

	gtk_signal_emit (GTK_OBJECT (dedit), date_edit_signals [DATE_CHANGED]);
	hide_date_popup (dedit);
}


static void
on_date_popup_now_button_clicked	(GtkWidget	*button,
					 EDateEdit	*dedit)
{
	e_date_edit_set_time (dedit, time (NULL));
	gtk_signal_emit (GTK_OBJECT (dedit), date_edit_signals [DATE_CHANGED]);
	hide_date_popup (dedit);
}


static void
on_date_popup_today_button_clicked	(GtkWidget	*button,
					 EDateEdit	*dedit)
{
	struct tm *tmp_tm;
	time_t t;
	char buffer [40];

	t = time (NULL);
	tmp_tm = localtime (&t);
	strftime (buffer, sizeof (buffer), "%x", tmp_tm);
	gtk_entry_set_text (GTK_ENTRY (dedit->_priv->date_entry), buffer);

	enable_time_combo (dedit);

	gtk_signal_emit (GTK_OBJECT (dedit), date_edit_signals [DATE_CHANGED]);
	hide_date_popup (dedit);
}


static void
on_date_popup_none_button_clicked	(GtkWidget	*button,
					 EDateEdit	*dedit)
{
	e_date_edit_set_time (dedit, -1);
	gtk_signal_emit (GTK_OBJECT (dedit), date_edit_signals [DATE_CHANGED]);
	hide_date_popup (dedit);
}


/* A key has been pressed while the date popup is showing. If it is the Escape
   key we hide the popup. */
static gint
on_date_popup_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	EDateEdit *dedit;

	if (event->keyval != GDK_Escape)
		return FALSE;

	dedit = data;
	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "key_press_event");
	hide_date_popup (dedit);

	return TRUE;
}


/* A mouse button has been pressed while the date popup is showing.
   Any button press events used to select days etc. in the popup will have
   have been handled elsewhere, so here we just hide the popup.
   (This function is yanked from gtkcombo.c) */
static gint
on_date_popup_button_press (GtkWidget *widget,
			    GdkEventButton *event,
			    gpointer data)
{
	EDateEdit *dedit;
	GtkWidget *child;

	dedit = data;

	child = gtk_get_event_widget ((GdkEvent *) event);

	/* We don't ask for button press events on the grab widget, so
	 *  if an event is reported directly to the grab widget, it must
	 *  be on a window outside the application (and thus we remove
	 *  the popup window). Otherwise, we check if the widget is a child
	 *  of the grab widget, and only remove the popup window if it
	 *  is not.
	 */
	if (child != widget) {
		while (child) {
			if (child == widget)
				return FALSE;
			child = child->parent;
		}
	}

	hide_date_popup (dedit);

	return TRUE;
}


/* A delete event has been received for the date popup, so we hide it and
   return TRUE so it doesn't get destroyed. */
static gint
on_date_popup_delete_event (GtkWidget *widget, gpointer data)
{
	EDateEdit *dedit;

	dedit = data;
	hide_date_popup (dedit);

	return TRUE;
}


/* Hides the date popup, removing any grabs. */
static void
hide_date_popup (EDateEdit *dedit)
{
	gtk_widget_hide (dedit->_priv->cal_popup);
	gtk_grab_remove (dedit->_priv->cal_popup);
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
}


/**
 * e_date_edit_get_time:
 * @dedit: The EDateEdit widget
 *
 * Returns the time entered in the EDateEdit widget
 */
time_t
e_date_edit_get_time (EDateEdit *dedit)
{
	EDateEditPrivate *priv;
	struct tm date_tm = { 0 }, time_tm = { 0 };
	char *date_text, *time_text, *format;

	g_return_val_if_fail (dedit != NULL, -1);
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), -1);
	
	priv = dedit->_priv;

	date_text = gtk_entry_get_text (GTK_ENTRY (priv->date_entry));
	if (date_is_none (date_text))
		return -1;

	strptime (date_text, "%x", &date_tm);

	if (dedit->_priv->show_time) {
		time_text = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->time_combo)->entry));

		if (priv->use_24_hour_format)
			format = "%H:%M";
		else
			format = "%I:%M %p";

		strptime (time_text, format, &time_tm);

		date_tm.tm_hour = time_tm.tm_hour;
		date_tm.tm_min = time_tm.tm_min;
	}

	date_tm.tm_isdst = -1;

	return mktime (&date_tm);
}


/**
 * e_date_edit_set_time:
 * @dedit: the EDateEdit widget
 * @the_time: The time and date that should be set on the widget
 *
 * Description:  Changes the displayed date and time in the EDateEdit
 * widget to be the one represented by @the_time.  If @the_time is 0
 * then current time is used.
 */
void
e_date_edit_set_time (EDateEdit *dedit, time_t the_time)
{
	EDateEditPrivate *priv;
	struct tm *mytm;
	char buffer[40], *format;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->_priv;

	if (the_time == -1) {
		gtk_entry_set_text (GTK_ENTRY (priv->date_entry), _("None"));
		disable_time_combo (dedit);
		return;
	}

	enable_time_combo (dedit);

	if (the_time == 0)
		the_time = time (NULL);

	mytm = localtime (&the_time);

	/* Set the date */
	strftime (buffer, sizeof (buffer), "%x", mytm);
	gtk_entry_set_text (GTK_ENTRY (priv->date_entry), buffer);

	/* Set the time */
	if (priv->use_24_hour_format)
		format = "%H:%M";
	else
		format = "%I:%M %p";

	strftime (buffer, sizeof (buffer), format, mytm);
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->time_combo)->entry),
			    buffer);
}


/* Whether we show the time field. */
gboolean
e_date_edit_get_show_time		(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), TRUE);

	return dedit->_priv->show_time;
}


void
e_date_edit_set_show_time		(EDateEdit	*dedit,
					 gboolean	 show_time)
{
	EDateEditPrivate *priv;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->_priv;

	if (priv->show_time == show_time)
		return;

	priv->show_time = show_time;

	if (show_time) {
		gtk_widget_show (priv->space);
		gtk_widget_show (priv->time_combo);
	} else {
		gtk_widget_hide (priv->space);
		gtk_widget_hide (priv->time_combo);
		gtk_widget_hide (priv->now_button);
	}
}


/* The week start day, used in the date popup. 0 (Sun) to 6 (Sat). */
gint
e_date_edit_get_week_start_day		(EDateEdit	*dedit)
{
	gint week_start_day;

	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), 1);

	gtk_object_get (GTK_OBJECT (E_CALENDAR (dedit->_priv->calendar)->calitem),
			"week_start_day", &week_start_day,
			NULL);

	return week_start_day;
}


void
e_date_edit_set_week_start_day		(EDateEdit	*dedit,
					 gint		 week_start_day)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (E_CALENDAR (dedit->_priv->calendar)->calitem),
			"week_start_day", week_start_day,
			NULL);
}


/* Whether we show week numbers in the date popup. */
gboolean
e_date_edit_get_show_week_numbers	(EDateEdit	*dedit)
{
	gboolean show_week_numbers;

	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), FALSE);

	gtk_object_get (GTK_OBJECT (E_CALENDAR (dedit->_priv->calendar)->calitem),
			"show_week_numbers", &show_week_numbers,
			NULL);

	return show_week_numbers;
}


void
e_date_edit_set_show_week_numbers	(EDateEdit	*dedit,
					 gboolean	 show_week_numbers)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (E_CALENDAR (dedit->_priv->calendar)->calitem),
			"show_week_numbers", show_week_numbers,
			NULL);
}


/* Whether we use 24 hour format in the time field & popup. */
gboolean
e_date_edit_get_use_24_hour_format	(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), TRUE);

	return dedit->_priv->use_24_hour_format;
}


void
e_date_edit_set_use_24_hour_format	(EDateEdit	*dedit,
					 gboolean	 use_24_hour_format)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	if (dedit->_priv->use_24_hour_format == use_24_hour_format)
		return;

	dedit->_priv->use_24_hour_format = use_24_hour_format;

	rebuild_time_popup (dedit);
}


/* Whether we allow the date to be set to 'None'. e_date_edit_get_time() will
   return (time_t) -1 in this case. */
gboolean
e_date_edit_get_allow_no_date_set	(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), FALSE);

	return GTK_WIDGET_VISIBLE (dedit->_priv->none_button);
}


void
e_date_edit_set_allow_no_date_set	(EDateEdit	*dedit,
					 gboolean	 allow_no_date_set)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	if (allow_no_date_set) {
		gtk_widget_show (dedit->_priv->none_button);
	} else {
		gtk_widget_hide (dedit->_priv->none_button);

		/* If currently set to 'None' set to the current time. */
		if (e_date_edit_get_time (dedit) == -1)
			e_date_edit_set_time (dedit, time (NULL));
	}
}


/* The range of time to show in the time combo popup. */
void
e_date_edit_get_time_popup_range	(EDateEdit	*dedit,
					 gint		*lower_hour,
					 gint		*upper_hour)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	*lower_hour = dedit->_priv->lower_hour;
	*upper_hour = dedit->_priv->upper_hour;
}


void
e_date_edit_set_time_popup_range	(EDateEdit	*dedit,
					 gint		 lower_hour,
					 gint		 upper_hour)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	if (dedit->_priv->lower_hour == lower_hour
	    && dedit->_priv->upper_hour == upper_hour)
		return;

	dedit->_priv->lower_hour = lower_hour;
	dedit->_priv->upper_hour = upper_hour;

	rebuild_time_popup (dedit);
}


static void
rebuild_time_popup			(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	GtkList *list;
	GtkWidget *listitem;
	char buffer[40], *format;
	struct tm tmp_tm;
	gint hour, min;

	priv = dedit->_priv;

	list = GTK_LIST (GTK_COMBO (priv->time_combo)->list);

	gtk_list_clear_items (list, 0, -1);

	/* Fill the struct tm with some sane values. */
	tmp_tm.tm_year = 2000;
	tmp_tm.tm_mon = 0;
	tmp_tm.tm_mday = 1;
	tmp_tm.tm_sec  = 0;
	tmp_tm.tm_isdst = 0;

	for (hour = priv->lower_hour; hour <= priv->upper_hour; hour++) {

		/* We don't want to display midnight at the end, since that is
		   really in the next day. */
		if (hour == 24)
			break;

		/* We want to finish on upper_hour, with min == 0. */
		for (min = 0;
		     min == 0 || (min < 60 && hour != priv->upper_hour);
		     min += 30) {
			tmp_tm.tm_hour = hour;
			tmp_tm.tm_min  = min;

			if (priv->use_24_hour_format)
				format = "%H:%M";
			else
				format = "%I:%M %p";

			strftime (buffer, sizeof (buffer), format, &tmp_tm);

			listitem = gtk_list_item_new_with_label (buffer);
			gtk_widget_show (listitem);
			gtk_container_add (GTK_CONTAINER (list), listitem);
		}
	}
}


static void
enable_time_combo		(EDateEdit	*dedit)
{
	gtk_widget_set_sensitive (dedit->_priv->time_combo, TRUE);
}


static void
disable_time_combo		(EDateEdit	*dedit)
{
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (dedit->_priv->time_combo)->entry), "");
	gtk_widget_set_sensitive (dedit->_priv->time_combo, FALSE);
}


static gboolean
date_is_none			(char		*date_text)
{
	char *pos, *none_string;

	pos = date_text;
	while (isspace (*pos))
		pos++;

	none_string = _("None");

	if (*pos == '\0' || !strncmp (pos, none_string, strlen (none_string)))
		return TRUE;
	return FALSE;
}

