/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 *
 * Based on the GnomeDateEdit, part of the Gnome Library.
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
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
 * EDateEdit - a widget based on GnomeDateEdit to provide a date & optional
 * time field with popups for entering a date.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-dateedit.h"
#include "e-util-marshal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtklist.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkvbox.h>
#include <atk/atkrelation.h>
#include <atk/atkrelationset.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include "e-util/e-time-utils.h"
#include "e-calendar.h"



struct _EDateEditPrivate {
	GtkWidget *date_entry;
	GtkWidget *date_button;
	
	GtkWidget *space;

	GtkWidget *time_combo;

	GtkWidget *cal_popup;
	GtkWidget *calendar;
	GtkWidget *now_button;
	GtkWidget *today_button;
	GtkWidget *none_button;		/* This will only be visible if a
					   'None' date/time is permitted. */

	gboolean show_date;
	gboolean show_time;
	gboolean use_24_hour_format;

	/* This is TRUE if we want to make the time field insensitive rather
	   than hide it when set_show_time() is called. */
	gboolean make_time_insensitive;

	/* This is the range of hours we show in the time popup. */
	gint lower_hour;
	gint upper_hour;

	/* This indicates whether the last date committed was invalid.
	   (A date is committed by hitting Return, moving the keyboard focus,
	   or selecting a date in the popup). Note that this only indicates
	   that the date couldn't be parsed. A date set to 'None' is valid
	   here, though e_date_edit_date_is_valid() will return FALSE if an
	   empty date isn't actually permitted. */
	gboolean date_is_valid;

	/* This is the last valid date which was set. If the date was set to
	   'None' or empty, date_set_to_none will be TRUE and the other fields
	   are undefined, so don't use them. */
	gboolean date_set_to_none;
	gint year;
	gint month;
	gint day;

	/* This indicates whether the last time committed was invalid.
	   (A time is committed by hitting Return, moving the keyboard focus,
	   or selecting a time in the popup). Note that this only indicates
	   that the time couldn't be parsed. An empty/None time is valid
	   here, though e_date_edit_time_is_valid() will return FALSE if an
	   empty time isn't actually permitted. */
	gboolean time_is_valid;

	/* This is the last valid time which was set. If the time was set to
	   'None' or empty, time_set_to_none will be TRUE and the other fields
	   are undefined, so don't use them. */
	gboolean time_set_to_none;
	gint hour;
	gint minute;

	EDateEditGetTimeCallback time_callback;
	gpointer time_callback_data;
	GtkDestroyNotify time_callback_destroy;
};

enum {
	CHANGED,
	LAST_SIGNAL
};


static gint date_edit_signals [LAST_SIGNAL] = { 0 };


static void e_date_edit_class_init		(EDateEditClass	*class);
static void e_date_edit_init			(EDateEdit	*dedit);
static void create_children			(EDateEdit	*dedit);
static void e_date_edit_dispose			(GObject	*object);
static gboolean e_date_edit_mnemonic_activate	(GtkWidget	*widget,
						 gboolean	 group_cycling);
static void e_date_edit_grab_focus		(GtkWidget	*widget);

static gint on_date_entry_key_press		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 EDateEdit	*dedit);
static void on_date_button_clicked		(GtkWidget	*widget,
						 EDateEdit	*dedit);
static void e_date_edit_show_date_popup		(EDateEdit	*dedit);
static void position_date_popup			(EDateEdit	*dedit);
static void on_date_popup_none_button_clicked	(GtkWidget	*button,
						 EDateEdit	*dedit);
static void on_date_popup_today_button_clicked	(GtkWidget	*button,
						 EDateEdit	*dedit);
static void on_date_popup_now_button_clicked	(GtkWidget	*button,
						 EDateEdit	*dedit);
static gint on_date_popup_delete_event		(GtkWidget	*widget,
						 EDateEdit	*dedit);
static gint on_date_popup_key_press		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 EDateEdit	*dedit);
static gint on_date_popup_button_press		(GtkWidget	*widget,
						 GdkEventButton *event,
						 gpointer	 data);
static void on_date_popup_date_selected		(ECalendarItem	*calitem,
						 EDateEdit	*dedit);
static void hide_date_popup			(EDateEdit	*dedit);
static void rebuild_time_popup			(EDateEdit	*dedit);
static gboolean field_set_to_none		(const char	*text);
static gboolean e_date_edit_parse_date		(EDateEdit	*dedit,
						 const char	*date_text,
						 struct tm	*date_tm);
static gboolean e_date_edit_parse_time		(EDateEdit	*dedit,
						 const gchar	*time_text,
						 struct tm	*time_tm);
static void on_date_edit_time_selected		(GtkList	*list,
						 EDateEdit	*dedit);
static gint on_time_entry_key_press		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 EDateEdit	*dedit);
static gint on_date_entry_focus_out		(GtkEntry	*entry,
						 GdkEventFocus  *event,
						 EDateEdit	*dedit);
static gint on_time_entry_focus_out		(GtkEntry	*entry,
						 GdkEventFocus  *event,
						 EDateEdit	*dedit);
static void e_date_edit_update_date_entry	(EDateEdit	*dedit);
static void e_date_edit_update_time_entry	(EDateEdit	*dedit);
static void e_date_edit_update_time_combo_state	(EDateEdit	*dedit);
static void e_date_edit_check_date_changed	(EDateEdit	*dedit);
static void e_date_edit_check_time_changed	(EDateEdit	*dedit);
static gboolean e_date_edit_set_date_internal	(EDateEdit	*dedit,
						 gboolean	 valid,
						 gboolean	 none,
						 gint		 year,
						 gint		 month,
						 gint		 day);
static gboolean e_date_edit_set_time_internal	(EDateEdit	*dedit,
						 gboolean	 valid,
						 gboolean	 none,
						 gint		 hour,
						 gint		 minute);


static GtkHBoxClass *parent_class;

/**
 * e_date_edit_get_type:
 *
 * Returns the GtkType for the EDateEdit widget
 */
GType
e_date_edit_get_type		(void)
{
	static GType date_edit_type = 0;

	if (!date_edit_type){
		static const GTypeInfo date_edit_info =  {
			sizeof (EDateEditClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_date_edit_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EDateEdit),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_date_edit_init,
		};

		date_edit_type = g_type_register_static (GTK_TYPE_HBOX, "EDateEdit", &date_edit_info, 0);
	}
	
	return date_edit_type;
}


static void
e_date_edit_class_init		(EDateEditClass	*class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;

	parent_class = g_type_class_ref (GTK_TYPE_HBOX);

	date_edit_signals [CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EDateEditClass, changed),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      GTK_TYPE_NONE, 0);

	object_class->dispose = e_date_edit_dispose;

	widget_class->mnemonic_activate = e_date_edit_mnemonic_activate;
	widget_class->grab_focus = e_date_edit_grab_focus;

	class->changed = NULL;
}


static void
e_date_edit_init		(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;

	dedit->priv = priv = g_new0 (EDateEditPrivate, 1);

	priv->show_date = TRUE;
	priv->show_time = TRUE;
	priv->use_24_hour_format = TRUE;

	priv->make_time_insensitive = FALSE;

	priv->lower_hour = 0;
	priv->upper_hour = 24;

	priv->date_is_valid = TRUE;
	priv->date_set_to_none = TRUE;
	priv->time_is_valid = TRUE;
	priv->time_set_to_none = TRUE;
	priv->time_callback = NULL;
	priv->time_callback_data = NULL;
	priv->time_callback_destroy = NULL;

	create_children (dedit);

	/* Set it to the current time. */
	e_date_edit_set_time (dedit, 0);
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
e_date_edit_new			(void)
{
	EDateEdit *dedit;
	AtkObject *a11y;

	dedit = g_object_new (E_TYPE_DATE_EDIT, NULL);
	a11y = gtk_widget_get_accessible (GTK_WIDGET (dedit));
	atk_object_set_name (a11y, _("Date and Time Entry"));

	return GTK_WIDGET (dedit);
}


static void
create_children			(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	ECalendar *calendar;
	GtkWidget *frame, *arrow;
	GtkWidget *vbox, *bbox;
	AtkObject *a11y;

	priv = dedit->priv;

	priv->date_entry  = gtk_entry_new ();
	a11y = gtk_widget_get_accessible (priv->date_entry);
	atk_object_set_description (a11y, _("Text entry to input date"));
	atk_object_set_name (a11y, _("Text Date Entry"));
	gtk_box_pack_start (GTK_BOX (dedit), priv->date_entry, FALSE, TRUE, 0);
	
	g_signal_connect (priv->date_entry, "key_press_event",
			  G_CALLBACK (on_date_entry_key_press),
			  dedit);
	g_signal_connect_after (priv->date_entry,
				"focus_out_event",
				G_CALLBACK (on_date_entry_focus_out),
				dedit);

	priv->date_button = gtk_button_new ();
	g_signal_connect (priv->date_button, "clicked",
			  G_CALLBACK (on_date_button_clicked), dedit);
	gtk_box_pack_start (GTK_BOX (dedit), priv->date_button,
			    FALSE, FALSE, 0);
	a11y = gtk_widget_get_accessible (priv->date_button);
	atk_object_set_description (a11y, _("Click this button to show a calendar"));
	atk_object_set_name (a11y, _("Date Button"));

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (priv->date_button), arrow);
	gtk_widget_show (arrow);

	if (priv->show_date) {
		gtk_widget_show (priv->date_entry);
		gtk_widget_show (priv->date_button);
	}

	/* This is just to create a space between the date & time parts. */
	priv->space = gtk_drawing_area_new ();
	gtk_box_pack_start (GTK_BOX (dedit), priv->space, FALSE, FALSE, 2);


	priv->time_combo = gtk_combo_new ();
	gtk_box_pack_start (GTK_BOX (dedit), priv->time_combo, FALSE, TRUE, 0);
	rebuild_time_popup (dedit);
	a11y = gtk_widget_get_accessible (priv->time_combo);
	atk_object_set_description (a11y, _("Combo box to select time"));
	atk_object_set_name (a11y, _("Time Combo Box"));

	g_signal_connect (GTK_COMBO (priv->time_combo)->entry,
			  "key_press_event",
			  G_CALLBACK (on_time_entry_key_press),
			  dedit);
	g_signal_connect_after (GTK_COMBO (priv->time_combo)->entry,
				"focus_out_event",
				G_CALLBACK (on_time_entry_focus_out),
				dedit);
	g_signal_connect_after (GTK_COMBO (priv->time_combo)->list,
				"selection_changed",
				G_CALLBACK (on_date_edit_time_selected),
				dedit);

	if (priv->show_time || priv->make_time_insensitive)
		gtk_widget_show (priv->time_combo);

	if (!priv->show_time && priv->make_time_insensitive)
		gtk_widget_set_sensitive (priv->time_combo, FALSE);

	if (priv->show_date
	    && (priv->show_time || priv->make_time_insensitive))
		gtk_widget_show (priv->space);

	priv->cal_popup = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_events (priv->cal_popup,
			       gtk_widget_get_events (priv->cal_popup)
			       | GDK_KEY_PRESS_MASK);
	g_signal_connect (priv->cal_popup, "delete_event",
			  G_CALLBACK (on_date_popup_delete_event),
			  dedit);
	g_signal_connect (priv->cal_popup, "key_press_event",
			  G_CALLBACK (on_date_popup_key_press),
			  dedit);
	g_signal_connect (priv->cal_popup, "button_press_event",
			  G_CALLBACK (on_date_popup_button_press),
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
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (calendar->calitem),
			       "maximum_days_selected", 1,
			       "move_selection_when_moving", FALSE,
			       NULL);

	g_signal_connect (calendar->calitem,
			  "selection_changed",
			  G_CALLBACK (on_date_popup_date_selected), dedit);

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
	g_signal_connect (priv->now_button, "clicked",
			  G_CALLBACK (on_date_popup_now_button_clicked), dedit);

	priv->today_button = gtk_button_new_with_label (_("Today"));
	gtk_container_add (GTK_CONTAINER (bbox), priv->today_button);
        gtk_widget_show (priv->today_button);
	g_signal_connect (priv->today_button, "clicked",
			  G_CALLBACK (on_date_popup_today_button_clicked), dedit);

	/* Note that we don't show this here, since by default a 'None' date
	   is not permitted. */
	priv->none_button = gtk_button_new_with_label (_("None"));
	gtk_container_add (GTK_CONTAINER (bbox), priv->none_button);
	g_signal_connect (priv->none_button, "clicked",
			  G_CALLBACK (on_date_popup_none_button_clicked), dedit);
}


static void
e_date_edit_dispose		(GObject	*object)
{
	EDateEdit *dedit;

	g_return_if_fail (E_IS_DATE_EDIT (object));

	dedit = E_DATE_EDIT (object);

	if (dedit->priv) {
		e_date_edit_set_get_time_callback (dedit, NULL, NULL, NULL);

		gtk_widget_destroy (dedit->priv->cal_popup);
		dedit->priv->cal_popup = NULL;

		g_free (dedit->priv);
		dedit->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}


/* GtkWidget::mnemonic_activate() handler for the EDateEdit */
static gboolean
e_date_edit_mnemonic_activate (GtkWidget *widget, gboolean group_cycling)
{
	e_date_edit_grab_focus (widget);
	return TRUE;
}

/* Grab_focus handler for the EDateEdit. If the date field is being shown, we
   grab the focus to that, otherwise we grab it to the time field. */
static void
e_date_edit_grab_focus		(GtkWidget	*widget)
{
	EDateEdit *dedit;

	g_return_if_fail (E_IS_DATE_EDIT (widget));

	dedit = E_DATE_EDIT (widget);

	if (dedit->priv->show_date)
		gtk_widget_grab_focus (dedit->priv->date_entry);
	else
		gtk_widget_grab_focus (GTK_COMBO (dedit->priv->time_combo)->entry);
}


/**
 * e_date_edit_set_editable:
 * @dedit: an #EDateEdit widget.
 * @editable: whether or not the widget should accept edits.
 *
 * Allows the programmer to disallow editing (and the popping up of
 * the calendar widget), while still allowing the user to select the
 * date from the GtkEntry.
 */
void
e_date_edit_set_editable (EDateEdit *dedit, gboolean editable)
{
	EDateEditPrivate *priv;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	gtk_entry_set_editable (GTK_ENTRY (priv->date_entry), editable);
	gtk_widget_set_sensitive (priv->date_button, editable);
}


/**
 * e_date_edit_get_time:
 * @dedit: an #EDateEdit widget.
 * @the_time: returns the last valid time entered.
 * @Returns: the last valid time entered, or -1 if the time is not set.
 *
 * Returns the last valid time entered. If empty times are valid, by calling
 * e_date_edit_set_allow_no_date_set(), then it may return -1.
 *
 * Note that the last time entered may actually have been invalid. You can
 * check this with e_date_edit_time_is_valid().
 */
time_t
e_date_edit_get_time		(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	struct tm tmp_tm = { 0 };

	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), -1);
	
	priv = dedit->priv;

	/* Try to parse any new value now. */
	e_date_edit_check_date_changed (dedit);
	e_date_edit_check_time_changed (dedit);

	if (priv->date_set_to_none)
		return -1;

	tmp_tm.tm_year = priv->year;
	tmp_tm.tm_mon = priv->month;
	tmp_tm.tm_mday = priv->day;

	if (!priv->show_time || priv->time_set_to_none) {
		tmp_tm.tm_hour = 0;
		tmp_tm.tm_min = 0;
	} else {
		tmp_tm.tm_hour = priv->hour;
		tmp_tm.tm_min = priv->minute;
	}
	tmp_tm.tm_sec = 0;
	tmp_tm.tm_isdst = -1;

	return mktime (&tmp_tm);
}


/**
 * e_date_edit_set_time:
 * @dedit: the EDateEdit widget
 * @the_time: The time and date that should be set on the widget
 *
 * Description:  Changes the displayed date and time in the EDateEdit
 * widget to be the one represented by @the_time.  If @the_time is 0
 * then current time is used. If it is -1, then the date is set to None.
 *
 * Note that the time is converted to local time using the Unix timezone,
 * so if you are using your own timezones then you should use
 * e_date_edit_set_date() and e_date_edit_set_time_of_day() instead.
 */
void
e_date_edit_set_time		(EDateEdit	*dedit,
				 time_t		 the_time)
{
	EDateEditPrivate *priv;
	struct tm tmp_tm;
	gboolean date_changed = FALSE, time_changed = FALSE;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	if (the_time == -1) {
		date_changed = e_date_edit_set_date_internal (dedit, TRUE,
							      TRUE, 0, 0, 0);
		time_changed = e_date_edit_set_time_internal (dedit, TRUE,
							      TRUE, 0, 0);
	} else {
		if (the_time == 0) {
			if (priv->time_callback) {
				tmp_tm = (*priv->time_callback) (dedit, priv->time_callback_data);
			} else {
				the_time = time (NULL);
				tmp_tm = *localtime (&the_time);
			}
		} else {
			tmp_tm = *localtime (&the_time);
		}

		date_changed = e_date_edit_set_date_internal (dedit, TRUE,
							      FALSE,
							      tmp_tm.tm_year,
							      tmp_tm.tm_mon,
							      tmp_tm.tm_mday);
		time_changed = e_date_edit_set_time_internal (dedit, TRUE,
							      FALSE,
							      tmp_tm.tm_hour,
							      tmp_tm.tm_min);
	}

	e_date_edit_update_date_entry (dedit);
	e_date_edit_update_time_entry (dedit);
	e_date_edit_update_time_combo_state (dedit);

	/* Emit the signals if the date and/or time has actually changed. */
	if (date_changed || time_changed)
		g_signal_emit (dedit,
			       date_edit_signals [CHANGED], 0);
}


/**
 * e_date_edit_get_date:
 * @dedit: an #EDateEdit widget.
 * @year: returns the year set.
 * @month: returns the month set (1 - 12).
 * @day: returns the day set (1 - 31).
 * @Returns: TRUE if a time was set, or FALSE if the field is empty or 'None'.
 *
 * Returns the last valid date entered into the date field.
 */
gboolean
e_date_edit_get_date		(EDateEdit	*dedit,
				 gint		*year,
				 gint		*month,
				 gint		*day)
{
	EDateEditPrivate *priv;

	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), FALSE);

	priv = dedit->priv;

	/* Try to parse any new value now. */
	e_date_edit_check_date_changed (dedit);

	*year = priv->year + 1900;
	*month = priv->month + 1;
	*day = priv->day;

	if (priv->date_set_to_none
	    && e_date_edit_get_allow_no_date_set (dedit))
		return FALSE;

	return TRUE;
}


/**
 * e_date_edit_set_date:
 * @dedit: an #EDateEdit widget.
 * @year: the year to set.
 * @month: the month to set (1 - 12).
 * @day: the day to set (1 - 31).
 *
 * Sets the date in the date field.
 */
void
e_date_edit_set_date		(EDateEdit	*dedit,
				 gint		 year,
				 gint		 month,
				 gint		 day)
{
	EDateEditPrivate *priv;
	gboolean date_changed = FALSE;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	date_changed = e_date_edit_set_date_internal (dedit, TRUE, FALSE,
						      year - 1900, month - 1,
						      day);

	e_date_edit_update_date_entry (dedit);
	e_date_edit_update_time_combo_state (dedit);

	/* Emit the signals if the date has actually changed. */
	if (date_changed)
		g_signal_emit (dedit,
			       date_edit_signals [CHANGED], 0);
}


/**
 * e_date_edit_get_time_of_day:
 * @dedit: an #EDateEdit widget.
 * @hour: returns the hour set, or 0 if the time isn't set.
 * @minute: returns the minute set, or 0 if the time isn't set.
 * @Returns: TRUE if a time was set, or FALSE if the field is empty or 'None'.
 *
 * Returns the last valid time entered into the time field.
 */
gboolean
e_date_edit_get_time_of_day		(EDateEdit	*dedit,
					 gint		*hour,
					 gint		*minute)
{
	EDateEditPrivate *priv;

	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), FALSE);

	priv = dedit->priv;

	/* Try to parse any new value now. */
	e_date_edit_check_time_changed (dedit);

	if (priv->time_set_to_none) {
		*hour = 0;
		*minute = 0;
		return FALSE;
	} else {
		*hour = priv->hour;
		*minute = priv->minute;
		return TRUE;
	}
}


/**
 * e_date_edit_set_time_of_day:
 * @dedit: an #EDateEdit widget.
 * @hour: the hour to set, or -1 to set the time to None (i.e. empty).
 * @minute: the minute to set.
 *
 * Description: Sets the time in the time field.
 */
void
e_date_edit_set_time_of_day		(EDateEdit	*dedit,
					 gint		 hour,
					 gint		 minute)
{
	EDateEditPrivate *priv;
	gboolean time_changed = FALSE;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	if (hour == -1) {
		gboolean allow_no_date_set = e_date_edit_get_allow_no_date_set (dedit);
		g_return_if_fail (allow_no_date_set);
		if (!priv->time_set_to_none) {
			priv->time_set_to_none = TRUE;
			time_changed = TRUE;
		}
	} else if (priv->time_set_to_none
		   || priv->hour != hour
		   || priv->minute != minute) {
		priv->time_set_to_none = FALSE;
		priv->hour = hour;
		priv->minute = minute;
		time_changed = TRUE;
	}

	e_date_edit_update_time_entry (dedit);

	if (time_changed)
		g_signal_emit (dedit,
			       date_edit_signals [CHANGED], 0);
}

void 
e_date_edit_set_date_and_time_of_day       (EDateEdit      *dedit,
					    gint            year,
					    gint            month,
					    gint            day,
					    gint            hour,
					    gint            minute)
{
	EDateEditPrivate *priv;
	gboolean date_changed, time_changed;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	date_changed = e_date_edit_set_date_internal (dedit, TRUE, FALSE,
						      year - 1900, month - 1, day);
	time_changed = e_date_edit_set_time_internal (dedit, TRUE, FALSE,
						      hour, minute);
	
	e_date_edit_update_date_entry (dedit);
	e_date_edit_update_time_entry (dedit);
	e_date_edit_update_time_combo_state (dedit);

	if (date_changed || time_changed)
		g_signal_emit (dedit,
			       date_edit_signals [CHANGED], 0);
}

/**
 * e_date_edit_get_show_date:
 * @dedit: an #EDateEdit widget.
 * @Returns: Whether the date field is shown.
 *
 * Description: Returns TRUE if the date field is currently shown.
 */
gboolean
e_date_edit_get_show_date		(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), TRUE);

	return dedit->priv->show_date;
}


/**
 * e_date_edit_set_show_date:
 * @dedit: an #EDateEdit widget.
 * @show_date: TRUE if the date field should be shown.
 *
 * Description: Specifies whether the date field should be shown. The date
 * field would be hidden if only a time needed to be entered.
 */
void
e_date_edit_set_show_date		(EDateEdit	*dedit,
					 gboolean	 show_date)
{
	EDateEditPrivate *priv;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	if (priv->show_date == show_date)
		return;

	priv->show_date = show_date;

	if (show_date) {
		gtk_widget_show (priv->date_entry);
		gtk_widget_show (priv->date_button);
	} else {
		gtk_widget_hide (priv->date_entry);
		gtk_widget_hide (priv->date_button);
	}

	e_date_edit_update_time_combo_state (dedit);

	if (priv->show_date
	    && (priv->show_time || priv->make_time_insensitive))
		gtk_widget_show (priv->space);
	else
		gtk_widget_hide (priv->space);
}


/**
 * e_date_edit_get_show_time:
 * @dedit: an #EDateEdit widget
 * @Returns: Whether the time field is shown.
 *
 * Description: Returns TRUE if the time field is currently shown.
 */
gboolean
e_date_edit_get_show_time		(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), TRUE);

	return dedit->priv->show_time;
}


/**
 * e_date_edit_set_show_time:
 * @dedit: an #EDateEdit widget
 * @show_time: TRUE if the time field should be shown.
 *
 * Description: Specifies whether the time field should be shown. The time
 * field would be hidden if only a date needed to be entered.
 */
void
e_date_edit_set_show_time		(EDateEdit	*dedit,
					 gboolean	 show_time)
{
	EDateEditPrivate *priv;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	if (priv->show_time == show_time)
		return;

	priv->show_time = show_time;

	e_date_edit_update_time_combo_state (dedit);
}


/**
 * e_date_edit_get_make_time_insensitive:
 * @dedit: an #EDateEdit widget
 * @Returns: Whether the time field is be made insensitive instead of hiding
 * it.
 *
 * Description: Returns TRUE if the time field is made insensitive instead of
 * hiding it.
 */
gboolean
e_date_edit_get_make_time_insensitive	(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), TRUE);

	return dedit->priv->make_time_insensitive;
}


/**
 * e_date_edit_set_make_time_insensitive:
 * @dedit: an #EDateEdit widget
 * @make_insensitive: TRUE if the time field should be made insensitive instead
 * of hiding it.
 *
 * Description: Specifies whether the time field should be made insensitive
 * rather than hiding it. Note that this doesn't make it insensitive - you
 * need to call e_date_edit_set_show_time() with FALSE as show_time to do that.
 *
 * This is useful if you want to disable the time field, but don't want it to
 * disappear as that may affect the layout of the widgets.
 */
void
e_date_edit_set_make_time_insensitive	(EDateEdit	*dedit,
					 gboolean	 make_insensitive)
{
	EDateEditPrivate *priv;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	if (priv->make_time_insensitive == make_insensitive)
		return;

	priv->make_time_insensitive = make_insensitive;

	e_date_edit_update_time_combo_state (dedit);
}


/**
 * e_date_edit_get_week_start_day:
 * @dedit: an #EDateEdit widget
 * @Returns: the week start day, from 0 (Sunday) to 6 (Saturday).
 *
 * Description: Returns the week start day currently used in the calendar
 * popup.
 */
gint
e_date_edit_get_week_start_day		(EDateEdit	*dedit)
{
	gint week_start_day;

	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), 1);

	g_object_get (E_CALENDAR (dedit->priv->calendar)->calitem,
		      "week_start_day", &week_start_day,
		      NULL);

	return week_start_day;
}


/**
 * e_date_edit_set_week_start_day:
 * @dedit: an #EDateEdit widget
 * @week_start_day: the week start day, from 0 (Sunday) to 6 (Saturday).
 *
 * Description: Sets the week start day to use in the calendar popup.
 */
void
e_date_edit_set_week_start_day		(EDateEdit	*dedit,
					 gint		 week_start_day)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (E_CALENDAR (dedit->priv->calendar)->calitem),
			"week_start_day", week_start_day,
			NULL);
}


/* Whether we show week numbers in the date popup. */
gboolean
e_date_edit_get_show_week_numbers	(EDateEdit	*dedit)
{
	gboolean show_week_numbers;

	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), FALSE);

	g_object_get (E_CALENDAR (dedit->priv->calendar)->calitem,
		      "show_week_numbers", &show_week_numbers,
		      NULL);

	return show_week_numbers;
}


void
e_date_edit_set_show_week_numbers	(EDateEdit	*dedit,
					 gboolean	 show_week_numbers)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (E_CALENDAR (dedit->priv->calendar)->calitem),
			"show_week_numbers", show_week_numbers,
			NULL);
}


/* Whether we use 24 hour format in the time field & popup. */
gboolean
e_date_edit_get_use_24_hour_format	(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), TRUE);

	return dedit->priv->use_24_hour_format;
}


void
e_date_edit_set_use_24_hour_format	(EDateEdit	*dedit,
					 gboolean	 use_24_hour_format)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	if (dedit->priv->use_24_hour_format == use_24_hour_format)
		return;

	dedit->priv->use_24_hour_format = use_24_hour_format;

	rebuild_time_popup (dedit);

	e_date_edit_update_time_entry (dedit);
}


/* Whether we allow the date to be set to 'None'. e_date_edit_get_time() will
   return (time_t) -1 in this case. */
gboolean
e_date_edit_get_allow_no_date_set	(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), FALSE);

	return GTK_WIDGET_VISIBLE (dedit->priv->none_button);
}


void
e_date_edit_set_allow_no_date_set	(EDateEdit	*dedit,
					 gboolean	 allow_no_date_set)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	if (allow_no_date_set) {
		gtk_widget_show (dedit->priv->none_button);
	} else {
		gtk_widget_hide (dedit->priv->none_button);

		/* If the date is showing, we make sure it isn't 'None' (we
		   don't really mind if the time is empty), else if just the
		   time is showing we make sure it isn't 'None'. */
		if (dedit->priv->show_date) {
			if (dedit->priv->date_set_to_none)
				e_date_edit_set_time (dedit, 0);
		} else {
			if (dedit->priv->time_set_to_none)
				e_date_edit_set_time (dedit, 0);
		}
	}
}


/* The range of time to show in the time combo popup. */
void
e_date_edit_get_time_popup_range	(EDateEdit	*dedit,
					 gint		*lower_hour,
					 gint		*upper_hour)
{
	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	*lower_hour = dedit->priv->lower_hour;
	*upper_hour = dedit->priv->upper_hour;
}


void
e_date_edit_set_time_popup_range	(EDateEdit	*dedit,
					 gint		 lower_hour,
					 gint		 upper_hour)
{
	EDateEditPrivate *priv;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	if (priv->lower_hour == lower_hour
	    && priv->upper_hour == upper_hour)
		return;

	priv->lower_hour = lower_hour;
	priv->upper_hour = upper_hour;

	rebuild_time_popup (dedit);

	/* Setting the combo list items seems to mess up the time entry, so
	   we set it again. We have to reset it to its last valid time. */
	priv->time_is_valid = TRUE;
	e_date_edit_update_time_entry (dedit);
}


/* The arrow button beside the date field has been clicked, so we show the
   popup with the ECalendar in. */
static void
on_date_button_clicked		(GtkWidget	*widget,
				 EDateEdit	*dedit)
{
	e_date_edit_show_date_popup (dedit);
}


static void
e_date_edit_show_date_popup	(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	ECalendar *calendar;
	struct tm mtm;
	const gchar *date_text;
	GDate selected_day;
	gboolean clear_selection = FALSE;

	priv = dedit->priv;
	calendar = E_CALENDAR (priv->calendar);

	date_text = gtk_entry_get_text (GTK_ENTRY (priv->date_entry));
	if (field_set_to_none (date_text)
	    || !e_date_edit_parse_date (dedit, date_text, &mtm))
		clear_selection = TRUE;

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
	gtk_widget_show (priv->cal_popup);
	gtk_widget_grab_focus (priv->cal_popup);
	gtk_grab_add (priv->cal_popup);
	gdk_pointer_grab (priv->cal_popup->window, TRUE,
			  (GDK_BUTTON_PRESS_MASK
			   | GDK_BUTTON_RELEASE_MASK
			   | GDK_POINTER_MOTION_MASK),
			  NULL, NULL, GDK_CURRENT_TIME);
	gdk_keyboard_grab (priv->cal_popup->window, TRUE, GDK_CURRENT_TIME);
	gdk_window_focus (priv->cal_popup->window, GDK_CURRENT_TIME);
}


/* This positions the date popup below and to the left of the arrow button,
   just before it is shown. */
static void
position_date_popup		(EDateEdit	*dedit)
{
	gint x, y;
	gint win_x, win_y;
	gint bwidth, bheight;
	GtkRequisition cal_req, button_req;
	gint screen_width, screen_height;

	gtk_widget_size_request (dedit->priv->cal_popup, &cal_req);

	gtk_widget_size_request (dedit->priv->date_button, &button_req);
	bwidth = button_req.width;
	gtk_widget_size_request (gtk_widget_get_parent (dedit->priv->date_button), &button_req);
	bheight = button_req.height;

	gtk_widget_translate_coordinates (dedit->priv->date_button,
					  gtk_widget_get_toplevel (dedit->priv->date_button),
					  bwidth - cal_req.width, bheight,
					  &x, &y);

	gdk_window_get_origin (gtk_widget_get_toplevel (dedit->priv->date_button)->window,
			       &win_x, &win_y);

	x += win_x;
	y += win_y;

      	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

	x = CLAMP (x, 0, MAX (0, screen_width - cal_req.width));
	y = CLAMP (y, 0, MAX (0, screen_height - cal_req.height));

	gtk_widget_set_uposition (dedit->priv->cal_popup, x, y);
}


/* A date has been selected in the date popup, so we set the date field
   and hide the popup. */
static void
on_date_popup_date_selected	(ECalendarItem	*calitem,
				 EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	GDate start_date, end_date;

	priv = dedit->priv;

	hide_date_popup (dedit);

	if (!e_calendar_item_get_selection (calitem, &start_date, &end_date))
		return;

	e_date_edit_set_date (dedit, g_date_get_year (&start_date),
			      g_date_get_month (&start_date),
			      g_date_get_day (&start_date));
}


static void
on_date_popup_now_button_clicked	(GtkWidget	*button,
					 EDateEdit	*dedit)
{
	hide_date_popup (dedit);
	e_date_edit_set_time (dedit, 0);
}


static void
on_date_popup_today_button_clicked	(GtkWidget	*button,
					 EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	struct tm tmp_tm;
	time_t t;

	priv = dedit->priv;

	hide_date_popup (dedit);

	if (priv->time_callback) {
		tmp_tm = (*priv->time_callback) (dedit, priv->time_callback_data);
	} else {
		t = time (NULL);
		tmp_tm = *localtime (&t);
	}

	e_date_edit_set_date (dedit, tmp_tm.tm_year + 1900,
			      tmp_tm.tm_mon + 1, tmp_tm.tm_mday);
}


static void
on_date_popup_none_button_clicked	(GtkWidget	*button,
					 EDateEdit	*dedit)
{
	hide_date_popup (dedit);
	e_date_edit_set_time (dedit, -1);
}


/* A key has been pressed while the date popup is showing. If it is the Escape
   key we hide the popup. */
static gint
on_date_popup_key_press			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 EDateEdit	*dedit)
{
	if (event->keyval != GDK_Escape)
		return FALSE;

	g_signal_stop_emission_by_name (widget, "key_press_event");
	hide_date_popup (dedit);

	return TRUE;
}


/* A mouse button has been pressed while the date popup is showing.
   Any button press events used to select days etc. in the popup will have
   have been handled elsewhere, so here we just hide the popup.
   (This function is yanked from gtkcombo.c) */
static gint
on_date_popup_button_press	(GtkWidget	*widget,
				 GdkEventButton *event,
				 gpointer	 data)
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
on_date_popup_delete_event	(GtkWidget	*widget,
				 EDateEdit	*dedit)
{
	hide_date_popup (dedit);
	return TRUE;
}


/* Hides the date popup, removing any grabs. */
static void
hide_date_popup			(EDateEdit	*dedit)
{
	gtk_widget_hide (dedit->priv->cal_popup);
	gtk_grab_remove (dedit->priv->cal_popup);
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);
}


/* Clears the time popup and rebuilds it using the lower_hour, upper_hour
   and use_24_hour_format settings. */
static void
rebuild_time_popup			(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	GtkList *list;
	GtkWidget *listitem, *label;
	char buffer[40], *format;
	struct tm tmp_tm;
	gint hour, min;

	priv = dedit->priv;

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
				/* This is a strftime() format. %H = hour (0-23), %M = minute. */
				format = _("%H:%M");
			else
				/* This is a strftime() format. %I = hour (1-12), %M = minute, %p = am/pm string. */
				format = _("%I:%M %p");

			e_utf8_strftime (buffer, sizeof (buffer), format, &tmp_tm);

			/* For 12-hour am/pm format, we want space padding, not zero padding. This
			 * can be done with strftime's %l, but it's a potentially unportable extension. */
			if (!priv->use_24_hour_format && buffer [0] == '0')
				buffer [0] = ' ';

			/* We need to make sure labels are right-aligned, since we want digits to line up,
			 * and with a nonproportional font, the width of a space != width of a digit.
			 * Technically, only 12-hour format needs this, but we do it always, for consistency. */
			listitem = gtk_list_item_new ();
			label = gtk_label_new (buffer);
			gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
			gtk_container_add (GTK_CONTAINER (listitem), label);

			gtk_widget_show_all (listitem);
			gtk_container_add (GTK_CONTAINER (list), listitem);
		}
	}
}


static gboolean
e_date_edit_parse_date (EDateEdit *dedit,
			const gchar *date_text,
			struct tm *date_tm)
{
	if (e_time_parse_date (date_text, date_tm) != E_TIME_PARSE_OK)
		return FALSE;

	return TRUE;
}


static gboolean
e_date_edit_parse_time	(EDateEdit	*dedit,
			 const gchar	*time_text,
			 struct tm	*time_tm)
{
	if (field_set_to_none (time_text)) {
		time_tm->tm_hour = 0;
		time_tm->tm_min = 0;
		return TRUE;
	}

	if (e_time_parse_time (time_text, time_tm) != E_TIME_PARSE_OK)
		return FALSE;

	return TRUE;
}


/* Returns TRUE if the string is empty or is "None" in the current locale.
   It ignores whitespace. */
static gboolean
field_set_to_none (const char *text)
{
	const char *pos;
	const char *none_string;

	pos = text;
	while (isspace (*pos))
		pos++;

	none_string = _("None");

	if (*pos == '\0' || !strncmp (pos, none_string, strlen (none_string)))
		return TRUE;
	return FALSE;
}


static void
on_date_edit_time_selected	(GtkList	*list,
				 EDateEdit	*dedit)
{
	GtkWidget *toplevel;

	/* We only want to emit signals when an item is selected explicitly,
	   not when it is selected by the silly combo update thing. */
	if (!list->selection)
		return;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (list));
	if (!GTK_WIDGET_MAPPED (toplevel))
		return;

	e_date_edit_check_time_changed (dedit);
}


static gint
on_date_entry_key_press			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 EDateEdit	*dedit)
{
	if (event->state & GDK_MOD1_MASK
	    && (event->keyval == GDK_Up || event->keyval == GDK_Down
		|| event->keyval == GDK_Return)) {
		g_signal_stop_emission_by_name (widget,
						"key_press_event");
		e_date_edit_show_date_popup (dedit);
		return TRUE;
	}

	/* If the user hits the return key emit a "date_changed" signal if
	   needed. But let the signal carry on. */
	if (event->keyval == GDK_Return) {
		e_date_edit_check_date_changed (dedit);
		return FALSE;
	}

	return FALSE;
}


static gint
on_time_entry_key_press			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 EDateEdit	*dedit)
{
	/* I'd like to use Alt+Up/Down for popping up the list, like Win32,
	   but the combo steals any Up/Down keys, so we use Alt+Return. */
#if 0	   
	if (event->state & GDK_MOD1_MASK
	    && (event->keyval == GDK_Up || event->keyval == GDK_Down)) {
#else
	if (event->state & GDK_MOD1_MASK && event->keyval == GDK_Return) {
#endif
		g_signal_stop_emission_by_name (widget,
						"key_press_event");
		g_signal_emit_by_name (GTK_COMBO (dedit->priv->time_combo)->entry, "activate", 0);
		return TRUE;
	}

	/* Stop the return key from emitting the activate signal, and check
	   if we need to emit a "time_changed" signal. */
	if (event->keyval == GDK_Return) {
		g_signal_stop_emission_by_name (widget,
						"key_press_event");
		e_date_edit_check_time_changed (dedit);
		return TRUE;
	}

	return FALSE;
}


static gint
on_date_entry_focus_out			(GtkEntry	*entry,
					 GdkEventFocus  *event,
					 EDateEdit	*dedit)
{
	e_date_edit_check_date_changed (dedit);
	return FALSE;
}


static gint
on_time_entry_focus_out			(GtkEntry	*entry,
					 GdkEventFocus  *event,
					 EDateEdit	*dedit)
{
	e_date_edit_check_time_changed (dedit);
	return FALSE;
}

static void
add_relation (EDateEdit *dedit, GtkWidget *widget)
{
	AtkObject *a11yEdit, *a11yWidget;
	AtkRelationSet *set;
	AtkRelation *relation;
	GPtrArray *target;
	gpointer target_object;

	/* add a labelled_by relation for widget for accessibility */

	a11yEdit = gtk_widget_get_accessible (GTK_WIDGET (dedit));
	a11yWidget = gtk_widget_get_accessible (widget);

	set = atk_object_ref_relation_set (a11yWidget);
	if (set != NULL) {
		relation = atk_relation_set_get_relation_by_type (set,
				ATK_RELATION_LABELLED_BY);
		/* check whether has a labelled_by relation already */
		if (relation != NULL)
			return;
	}

	set = atk_object_ref_relation_set (a11yEdit);
	if (!set)
		return;

	relation = atk_relation_set_get_relation_by_type (set,
			ATK_RELATION_LABELLED_BY);
	if (relation != NULL) {
		target = atk_relation_get_target (relation);
		target_object = g_ptr_array_index (target, 0);
		if (ATK_IS_OBJECT (target_object)) {
			atk_object_add_relationship (a11yWidget,
					ATK_RELATION_LABELLED_BY,
					ATK_OBJECT (target_object));
		}
	}
}

/* This sets the text in the date entry according to the current settings. */
static void
e_date_edit_update_date_entry		(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	char buffer[40];
	struct tm tmp_tm = { 0 };

	priv = dedit->priv;

	if (priv->date_set_to_none || !priv->date_is_valid) {
		gtk_entry_set_text (GTK_ENTRY (priv->date_entry), _("None"));
	} else {
		tmp_tm.tm_year = priv->year;
		tmp_tm.tm_mon = priv->month;
		tmp_tm.tm_mday = priv->day;
		tmp_tm.tm_isdst = -1;

		/* This is a strftime() format for a short date. %m = month,
		   %d = day of month, %Y = year (all digits). */
		e_utf8_strftime (buffer, sizeof (buffer), _("%m/%d/%Y"), &tmp_tm);
		gtk_entry_set_text (GTK_ENTRY (priv->date_entry), buffer);
	}

	add_relation (dedit, priv->date_entry);
}


/* This sets the text in the time entry according to the current settings. */
static void
e_date_edit_update_time_entry		(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	char buffer[40], *format;
	struct tm tmp_tm = { 0 };

	priv = dedit->priv;

	if (priv->time_set_to_none || !priv->time_is_valid) {
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->time_combo)->entry), "");
	} else {
		/* Set these to reasonable values just in case. */
		tmp_tm.tm_year = 2000;
		tmp_tm.tm_mon = 0;
		tmp_tm.tm_mday = 1;

		tmp_tm.tm_hour = priv->hour;
		tmp_tm.tm_min = priv->minute;

		tmp_tm.tm_sec = 0;
		tmp_tm.tm_isdst = -1;

		if (priv->use_24_hour_format)
			/* This is a strftime() format. %H = hour (0-23), %M = minute. */
			format = _("%H:%M");
		else
			/* This is a strftime() format. %I = hour (1-12), %M = minute, %p = am/pm string. */
			format = _("%I:%M %p");

		e_utf8_strftime (buffer, sizeof (buffer), format, &tmp_tm);
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->time_combo)->entry),
				    buffer);
	}

	add_relation (dedit, priv->time_combo);
}


static void
e_date_edit_update_time_combo_state	(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	gboolean show = TRUE, show_now_button = TRUE;
	gboolean clear_entry = FALSE, sensitive = TRUE;
	const gchar *text;

	priv = dedit->priv;

	/* If the date entry is currently shown, and it is set to None,
	   clear the time entry and disable the time combo. */
	if (priv->show_date && priv->date_set_to_none) {
		clear_entry = TRUE;
		sensitive = FALSE;
	}

	if (!priv->show_time) {
		if (priv->make_time_insensitive) {
			clear_entry = TRUE;
			sensitive = FALSE;
		} else {
			show = FALSE;
		}

		show_now_button = FALSE;
	}

	if (clear_entry) {
		/* Only clear it if it isn't empty already. */
		text = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->time_combo)->entry));
		if (text[0])
			gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->time_combo)->entry), "");
	}

	gtk_widget_set_sensitive (priv->time_combo, sensitive);

	if (show)
		gtk_widget_show (priv->time_combo);
	else
		gtk_widget_hide (priv->time_combo);

	if (show_now_button)
		gtk_widget_show (priv->now_button);
	else
		gtk_widget_hide (priv->now_button);

	if (priv->show_date
	    && (priv->show_time || priv->make_time_insensitive))
		gtk_widget_show (priv->space);
	else
		gtk_widget_hide (priv->space);
}


/* Parses the date, and if it is different from the current settings it
   updates the settings and emits a "date_changed" signal. */
static void
e_date_edit_check_date_changed		(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	const gchar *date_text;
	struct tm tmp_tm;
	gboolean none = FALSE, valid = TRUE, date_changed = FALSE;

	priv = dedit->priv;

	tmp_tm.tm_year = 0;
	tmp_tm.tm_mon = 0;
	tmp_tm.tm_mday = 0;

	date_text = gtk_entry_get_text (GTK_ENTRY (priv->date_entry));
	if (field_set_to_none (date_text)) {
		none = TRUE;
	} else if (!e_date_edit_parse_date (dedit, date_text, &tmp_tm)) {
		valid = FALSE;
		tmp_tm.tm_year = 0;
		tmp_tm.tm_mon = 0;
		tmp_tm.tm_mday = 0;
	}


	date_changed = e_date_edit_set_date_internal (dedit, valid, none,
						      tmp_tm.tm_year,
						      tmp_tm.tm_mon,
						      tmp_tm.tm_mday);

	if (date_changed)
		g_signal_emit (dedit,
			       date_edit_signals [CHANGED], 0);
}


/* Parses the time, and if it is different from the current settings it
   updates the settings and emits a "time_changed" signal. */
static void
e_date_edit_check_time_changed		(EDateEdit	*dedit)
{
	EDateEditPrivate *priv;
	const gchar *time_text;
	struct tm tmp_tm;
	gboolean none = FALSE, valid = TRUE, time_changed;

	priv = dedit->priv;

	tmp_tm.tm_hour = 0;
	tmp_tm.tm_min = 0;

	time_text = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->time_combo)->entry));
	if (field_set_to_none (time_text))
		none = TRUE;
	else if (!e_date_edit_parse_time (dedit, time_text, &tmp_tm))
		valid = FALSE;

	time_changed = e_date_edit_set_time_internal (dedit, valid, none,
						      tmp_tm.tm_hour,
						      tmp_tm.tm_min);

	if (time_changed)
		g_signal_emit (dedit,
			       date_edit_signals [CHANGED], 0);
}


/**
 * e_date_edit_date_is_valid:
 * @dedit: an #EDateEdit widget.
 * @Returns: TRUE if the last date entered was valid.
 *
 * Returns TRUE if the last date entered was valid.
 *
 * Note that if this returns FALSE, you can still use e_date_edit_get_time()
 * or e_date_edit_get_date() to get the last time or date entered which was
 * valid.
 */
gboolean
e_date_edit_date_is_valid	(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), FALSE);

	if (!dedit->priv->date_is_valid)
		return FALSE;

	/* If the date is empty/None and that isn't permitted, return FALSE. */
	if (dedit->priv->date_set_to_none
	    && !e_date_edit_get_allow_no_date_set (dedit))
		return FALSE;

	return TRUE;
}


/**
 * e_date_edit_time_is_valid:
 * @dedit: an #EDateEdit widget.
 * @Returns: TRUE if the last time entered was valid.
 *
 * Returns TRUE if the last time entered was valid.
 *
 * Note that if this returns FALSE, you can still use e_date_edit_get_time()
 * or e_date_edit_get_time_of_day() to get the last time or time of the day
 * entered which was valid.
 */
gboolean
e_date_edit_time_is_valid	(EDateEdit	*dedit)
{
	g_return_val_if_fail (E_IS_DATE_EDIT (dedit), FALSE);

	if (!dedit->priv->time_is_valid)
		return FALSE;

	/* If the time is empty and that isn't permitted, return FALSE.
	   Note that we don't mind an empty time if the date field is shown
	   - in that case we just assume 0:00. */
	if (dedit->priv->time_set_to_none && !dedit->priv->show_date
	    && !e_date_edit_get_allow_no_date_set (dedit))
		return FALSE;

	return TRUE;
}


static gboolean
e_date_edit_set_date_internal	(EDateEdit	*dedit,
				 gboolean	 valid,
				 gboolean	 none,
				 gint		 year,
				 gint		 month,
				 gint		 day)
{
	EDateEditPrivate *priv;
	gboolean date_changed = FALSE;

	priv = dedit->priv;

	if (!valid) {
		/* Date is invalid. */
		if (priv->date_is_valid) {
			priv->date_is_valid = FALSE;
			date_changed = TRUE;
		}
	} else if (none) {
		/* Date has been set to 'None'. */
		if (!priv->date_is_valid
		    || !priv->date_set_to_none) {
			priv->date_is_valid = TRUE;
			priv->date_set_to_none = TRUE;
			date_changed = TRUE;
		}
	} else {
		/* Date has been set to a specific date. */
		if (!priv->date_is_valid
		    || priv->date_set_to_none
		    || priv->year != year
		    || priv->month != month
		    || priv->day != day) {
			priv->date_is_valid = TRUE;
			priv->date_set_to_none = FALSE;
			priv->year = year;
			priv->month = month;
			priv->day = day;
			date_changed = TRUE;
		}
	}

	return date_changed;
}


static gboolean
e_date_edit_set_time_internal	(EDateEdit	*dedit,
				 gboolean	 valid,
				 gboolean	 none,
				 gint		 hour,
				 gint		 minute)
{
	EDateEditPrivate *priv;
	gboolean time_changed = FALSE;

	priv = dedit->priv;

	if (!valid) {
		/* Time is invalid. */
		if (priv->time_is_valid) {
			priv->time_is_valid = FALSE;
			time_changed = TRUE;
		}
	} else if (none) {
		/* Time has been set to empty/'None'. */
		if (!priv->time_is_valid
		    || !priv->time_set_to_none) {
			priv->time_is_valid = TRUE;
			priv->time_set_to_none = TRUE;
			time_changed = TRUE;
		}
	} else {
		/* Time has been set to a specific time. */
		if (!priv->time_is_valid
		    || priv->time_set_to_none
		    || priv->hour != hour
		    || priv->minute != minute) {
			priv->time_is_valid = TRUE;
			priv->time_set_to_none = FALSE;
			priv->hour = hour;
			priv->minute = minute;
			time_changed = TRUE;
		}
	}

	return time_changed;
}


/* Sets a callback to use to get the current time. This is useful if the
   application needs to use its own timezone data rather than rely on the
   Unix timezone. */
void
e_date_edit_set_get_time_callback	(EDateEdit	*dedit,
					 EDateEditGetTimeCallback cb,
					 gpointer	 data,
					 GtkDestroyNotify destroy)
{
	EDateEditPrivate *priv;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	priv = dedit->priv;

	if (priv->time_callback_data && priv->time_callback_destroy)
		(*priv->time_callback_destroy) (priv->time_callback_data);

	priv->time_callback = cb;
	priv->time_callback_data = data;
	priv->time_callback_destroy = destroy;

}

