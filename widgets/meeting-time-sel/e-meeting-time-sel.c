/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@gtk.org>
 *
 * Copyright 1999, Helix Code, Inc.
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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkvscrollbar.h>
#include <libgnomeui/gnome-dateedit.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-canvas-widget.h>

#include "../../e-util/e-canvas.h"
#include "e-meeting-time-sel.h"
#include "e-meeting-time-sel-item.h"
#include "e-meeting-time-sel-list-item.h"

/* An array of hour strings, "0:00" .. "23:00". */
const gchar *EMeetingTimeSelectorHours[24] = {
	"0:00", "1:00", "2:00", "3:00", "4:00", "5:00", "6:00", "7:00", 
	"8:00", "9:00", "10:00", "11:00", "12:00", "13:00", "14:00", "15:00", 
	"16:00", "17:00", "18:00", "19:00", "20:00", "21:00", "22:00", "23:00"
};

/* The number of days shown in the entire canvas. */
#define E_MEETING_TIME_SELECTOR_DAYS_SHOWN		365

/* This is the number of pixels between the mouse has to move before the
   scroll speed is incremented. */
#define E_MEETING_TIME_SELECTOR_SCROLL_INCREMENT_WIDTH	10

/* This is the maximum scrolling speed. */
#define E_MEETING_TIME_SELECTOR_MAX_SCROLL_SPEED	4


static void e_meeting_time_selector_class_init (EMeetingTimeSelectorClass * klass);
static void e_meeting_time_selector_init (EMeetingTimeSelector * mts);
static void e_meeting_time_selector_destroy (GtkObject *object);
static void e_meeting_time_selector_alloc_named_color (EMeetingTimeSelector * mts,
						       const char *name, GdkColor *c);
static void e_meeting_time_selector_add_key_color (EMeetingTimeSelector * mts,
						   GtkWidget *hbox,
						   gchar *label_text,
						   GdkColor *color);
static gint e_meeting_time_selector_expose_key_color (GtkWidget *darea,
						      GdkEventExpose *event,
						      GdkColor *color);
static gint e_meeting_time_selector_expose_title_bar (GtkWidget *darea,
						      GdkEventExpose *event,
						      gpointer data);
static void e_meeting_time_selector_options_menu_detacher (GtkWidget *widget,
							   GtkMenu   *menu);
static void e_meeting_time_selector_autopick_menu_detacher (GtkWidget *widget,
							    GtkMenu   *menu);
static void e_meeting_time_selector_realize (GtkWidget *widget);
static void e_meeting_time_selector_unrealize (GtkWidget *widget);
static void e_meeting_time_selector_style_set (GtkWidget *widget,
					       GtkStyle  *previous_style);
static gint e_meeting_time_selector_expose_event (GtkWidget *widget,
						  GdkEventExpose *event);
static void e_meeting_time_selector_draw (GtkWidget    *widget,
					  GdkRectangle *area);
static void e_meeting_time_selector_draw_shadow (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_hadjustment_changed (GtkAdjustment *adjustment,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_vadjustment_changed (GtkAdjustment *adjustment,
							 EMeetingTimeSelector *mts);

static void e_meeting_time_selector_on_canvas_realized (GtkWidget *widget,
							EMeetingTimeSelector *mts);

static gint e_meeting_time_selector_compare_period_starts (const void *arg1,
							 const void *arg2);
#if 0
static gint e_meeting_time_selector_compare_periods (const void *arg1,
						     const void *arg2);
#endif
static gint e_meeting_time_selector_compare_times (EMeetingTimeSelectorTime *time1,
						   EMeetingTimeSelectorTime *time2);
static void e_meeting_time_selector_on_options_button_clicked (GtkWidget *button,
							       EMeetingTimeSelector *mts);
static void e_meeting_time_selector_options_menu_position_callback (GtkMenu *menu,
								    gint *x,
								    gint *y,
								    gpointer user_data);
static void e_meeting_time_selector_on_zoomed_out_toggled (GtkWidget *button,
							   EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_working_hours_toggled (GtkWidget *button,
							      EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_invite_others_button_clicked (GtkWidget *button,
								     EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_update_free_busy (GtkWidget *button,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_autopick_button_clicked (GtkWidget *button,
								EMeetingTimeSelector *mts);
static void e_meeting_time_selector_autopick_menu_position_callback (GtkMenu *menu,
								     gint *x,
								     gint *y,
								     gpointer user_data);
static void e_meeting_time_selector_on_autopick_option_toggled (GtkWidget *button,
								EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_prev_button_clicked (GtkWidget *button,
							    EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_next_button_clicked (GtkWidget *button,
							    EMeetingTimeSelector *mts);
static void e_meeting_time_selector_autopick (EMeetingTimeSelector *mts,
					      gboolean forward);
static void e_meeting_time_selector_calculate_time_difference (EMeetingTimeSelectorTime *start,
							       EMeetingTimeSelectorTime *end,
							       gint *days,
							       gint *hours,
							       gint *minutes);
static void e_meeting_time_selector_find_nearest_interval (EMeetingTimeSelector *mts,
							   EMeetingTimeSelectorTime *start_time,
							   EMeetingTimeSelectorTime *end_time,
							   gint days, gint hours, gint mins);
static void e_meeting_time_selector_find_nearest_interval_backward (EMeetingTimeSelector *mts,
								    EMeetingTimeSelectorTime *start_time,
								    EMeetingTimeSelectorTime *end_time,
								    gint days, gint hours, gint mins);
static void e_meeting_time_selector_adjust_time (EMeetingTimeSelectorTime *mtstime,
						 gint days, gint hours, gint minutes);
static EMeetingTimeSelectorPeriod* e_meeting_time_selector_find_time_clash (EMeetingTimeSelector *mts,
									    EMeetingTimeSelectorAttendee *attendee,
									    EMeetingTimeSelectorTime *start_time,
									    EMeetingTimeSelectorTime *end_time);


static void e_meeting_time_selector_recalc_grid (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_recalc_date_format (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_save_position (EMeetingTimeSelector *mts,
						   EMeetingTimeSelectorTime *mtstime);
static void e_meeting_time_selector_restore_position (EMeetingTimeSelector *mts,
						      EMeetingTimeSelectorTime *mtstime);
static void e_meeting_time_selector_on_start_time_changed (GtkWidget *widget,
							   EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_end_time_changed (GtkWidget *widget,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_date_popup_menus (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_attendees_list_size_allocate (GtkWidget *widget,
								     GtkAllocation *allocation,
								     EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_canvas_size_allocate (GtkWidget *widget,
							     GtkAllocation *allocation,
							     EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_main_canvas_scroll_region (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_attendees_list_scroll_region (EMeetingTimeSelector *mts);
static gboolean e_meeting_time_selector_timeout_handler (gpointer data);
static void e_meeting_time_selector_update_start_date_edit (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_end_date_edit (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_ensure_meeting_time_shown (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_dates_shown (EMeetingTimeSelector *mts);

static void e_meeting_time_selector_update_attendees_list_positions (EMeetingTimeSelector *mts);
static gboolean e_meeting_time_selector_on_text_item_event (GnomeCanvasItem *item,
							    GdkEvent *event,
							    EMeetingTimeSelector *mts);
static gint e_meeting_time_selector_find_row_from_text_item (EMeetingTimeSelector *mts,
							     GnomeCanvasItem *item);

static GtkTableClass *parent_class;


GtkType
e_meeting_time_selector_get_type (void)
{
	static guint e_meeting_time_selector_type = 0;

	if (!e_meeting_time_selector_type) {
		GtkTypeInfo e_meeting_time_selector_info =
		{
			"EMeetingTimeSelector",
			sizeof (EMeetingTimeSelector),
			sizeof (EMeetingTimeSelectorClass),
			(GtkClassInitFunc) e_meeting_time_selector_class_init,
			(GtkObjectInitFunc) e_meeting_time_selector_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		e_meeting_time_selector_type = gtk_type_unique (GTK_TYPE_TABLE,
								&e_meeting_time_selector_info);
	}
	return e_meeting_time_selector_type;
}


static void
e_meeting_time_selector_class_init (EMeetingTimeSelectorClass * klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = gtk_type_class (gtk_table_get_type());

	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	object_class->destroy = e_meeting_time_selector_destroy;

	widget_class->realize      = e_meeting_time_selector_realize;
	widget_class->unrealize    = e_meeting_time_selector_unrealize;
	widget_class->style_set    = e_meeting_time_selector_style_set;
	widget_class->expose_event = e_meeting_time_selector_expose_event;
	widget_class->draw	   = e_meeting_time_selector_draw;
}


static void
e_meeting_time_selector_init (EMeetingTimeSelector * mts)
{
	GtkWidget *hbox, *separator, *button, *label, *table;
	GtkWidget *alignment, *child_hbox, *arrow, *menuitem;
	GSList *group;
	GdkVisual *visual;
	GdkColormap *colormap;
	guint accel_key;
	GtkAccelGroup *menu_accel_group;
	time_t meeting_start_time;
	struct tm *meeting_start_tm;
	guchar stipple_bits[] = {
		0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
	};

	/* The shadow is drawn in the border so it must be >= 2 pixels. */
	gtk_container_set_border_width (GTK_CONTAINER (mts), 2);

	mts->accel_group = gtk_accel_group_new ();

	mts->attendees = g_array_new (FALSE, FALSE,
				      sizeof (EMeetingTimeSelectorAttendee));

	mts->working_hours_only = TRUE;
	mts->day_start_hour = 9;
	mts->day_start_minute = 0;
	mts->day_end_hour = 18;
	mts->day_end_minute = 0;
	mts->zoomed_out = FALSE;
	mts->dragging_position = E_MEETING_TIME_SELECTOR_POS_NONE;

	/* The default meeting time is the nearest half-hour interval in the
	   future, in working hours. */
	meeting_start_time = time (NULL);
	g_date_clear (&mts->meeting_start_time.date, 1);
	g_date_set_time (&mts->meeting_start_time.date, meeting_start_time);
	meeting_start_tm = localtime (&meeting_start_time);
	mts->meeting_start_time.hour = meeting_start_tm->tm_hour;
	mts->meeting_start_time.minute = meeting_start_tm->tm_min;

	e_meeting_time_selector_find_nearest_interval (mts, &mts->meeting_start_time,
						       &mts->meeting_end_time,
						       0, 0, 30);

	e_meeting_time_selector_update_dates_shown (mts);

	mts->meeting_positions_valid = FALSE;

	mts->row_height = 30;
	mts->col_width = 50;
	mts->day_width = 50 * 24 + 1;

	mts->auto_scroll_timeout_id = 0;


	mts->attendees_title_bar_vbox = gtk_vbox_new (FALSE, 2);
	/* Add some horizontal padding for the shadow around the display. */
	gtk_table_attach (GTK_TABLE (mts),
			  mts->attendees_title_bar_vbox,
			  0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 2, 0);
	gtk_widget_show (mts->attendees_title_bar_vbox);

	mts->attendees_title_bar = gtk_drawing_area_new ();
	gtk_box_pack_end (GTK_BOX (mts->attendees_title_bar_vbox),
			  mts->attendees_title_bar, FALSE, FALSE, 0);
	gtk_widget_show (mts->attendees_title_bar);
	gtk_signal_connect (GTK_OBJECT (mts->attendees_title_bar),
			    "expose_event",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_expose_title_bar), mts);

	mts->attendees_list = e_canvas_new ();
	/* Add some horizontal padding for the shadow around the display. */
	gtk_table_attach (GTK_TABLE (mts), mts->attendees_list,
			  0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 0);
	gtk_widget_show (mts->attendees_list);
	gtk_signal_connect (GTK_OBJECT (mts->attendees_list), "realize",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_canvas_realized), mts);
	gtk_signal_connect (GTK_OBJECT (mts->attendees_list), "size_allocate",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_attendees_list_size_allocate), mts);

	/* Create the item in the list canvas. */
	gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (mts->attendees_list)->root),
			       e_meeting_time_selector_list_item_get_type (),
			       "EMeetingTimeSelectorListItem::meeting_time_selector", mts,
			       NULL);

	mts->display_top = gnome_canvas_new ();
	gnome_canvas_set_scroll_region (GNOME_CANVAS (mts->display_top),
					0, 0,
					mts->day_width * E_MEETING_TIME_SELECTOR_DAYS_SHOWN,
					mts->row_height * 3);
	/* Add some horizontal padding for the shadow around the display. */
	gtk_table_attach (GTK_TABLE (mts), mts->display_top,
			  1, 4, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 0);
	gtk_widget_show (mts->display_top);
	gtk_signal_connect (GTK_OBJECT (mts->display_top), "realize",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_canvas_realized), mts);

	mts->display_main = gnome_canvas_new ();
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);
	/* Add some horizontal padding for the shadow around the display. */
	gtk_table_attach (GTK_TABLE (mts), mts->display_main,
			  1, 4, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 0);
	gtk_widget_show (mts->display_main);
	gtk_signal_connect (GTK_OBJECT (mts->display_main), "realize",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_canvas_realized), mts);
	gtk_signal_connect (GTK_OBJECT (mts->display_main), "size_allocate",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_canvas_size_allocate), mts);

	mts->hscrollbar = gtk_hscrollbar_new (GTK_LAYOUT (mts->display_main)->hadjustment);
	gtk_table_attach (GTK_TABLE (mts), mts->hscrollbar,
			  1, 4, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show (mts->hscrollbar);

	mts->vscrollbar = gtk_vscrollbar_new (GTK_LAYOUT (mts->display_main)->vadjustment);
	gtk_table_attach (GTK_TABLE (mts), mts->vscrollbar,
			  4, 5, 1, 2, 0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (mts->vscrollbar);

	/* Create the item in the top canvas. */
	gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (mts->display_top)->root),
			       e_meeting_time_selector_item_get_type (),
			       "EMeetingTimeSelectorItem::meeting_time_selector", mts,
			       NULL);

	/* Create the item in the main canvas. */
	gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (mts->display_main)->root),
			       e_meeting_time_selector_item_get_type (),
			       "EMeetingTimeSelectorItem::meeting_time_selector", mts,
			       NULL);

	/* Create the hbox containing the color key. */
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_table_attach (GTK_TABLE (mts), hbox,
			  1, 4, 3, 4, GTK_FILL, 0, 0, 8);
	gtk_widget_show (hbox);

	e_meeting_time_selector_add_key_color (mts, hbox, _("Tentative"), &mts->busy_colors[E_MEETING_TIME_SELECTOR_BUSY_TENTATIVE]);
	e_meeting_time_selector_add_key_color (mts, hbox, _("Busy"), &mts->busy_colors[E_MEETING_TIME_SELECTOR_BUSY_BUSY]);
	e_meeting_time_selector_add_key_color (mts, hbox, _("Out of Office"), &mts->busy_colors[E_MEETING_TIME_SELECTOR_BUSY_OUT_OF_OFFICE]);
	e_meeting_time_selector_add_key_color (mts, hbox, _("No Information"),
					       NULL);

	separator = gtk_hseparator_new ();
	gtk_table_attach (GTK_TABLE (mts), separator,
			  0, 5, 4, 5, GTK_FILL, 0, 0, 0);
	gtk_widget_show (separator);

	/* Create the Invite Others & Options buttons on the left. */
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_table_attach (GTK_TABLE (mts), hbox,
			  0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show (hbox);

	button = gtk_button_new_with_label ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (button)->child),
					   _("_Invite Others..."));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show (button);
	gtk_widget_add_accelerator (button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_invite_others_button_clicked), mts);

	mts->options_button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), mts->options_button, TRUE, TRUE, 0);
	gtk_widget_show (mts->options_button);

	gtk_signal_connect (GTK_OBJECT (mts->options_button), "clicked",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_options_button_clicked), mts);

	child_hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (mts->options_button), child_hbox);
	gtk_widget_show (child_hbox);

	label = gtk_label_new ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (label), _("_Options"));
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gtk_widget_add_accelerator (mts->options_button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (child_hbox), arrow, FALSE, FALSE, 2);
	gtk_widget_show (arrow);

	/* Create the Options menu. */
	mts->options_menu = gtk_menu_new ();
	gtk_menu_attach_to_widget (GTK_MENU (mts->options_menu), mts->options_button,
				   e_meeting_time_selector_options_menu_detacher);
	menu_accel_group = gtk_menu_ensure_uline_accel_group (GTK_MENU (mts->options_menu));

	menuitem = gtk_check_menu_item_new_with_label ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menuitem)->child), _("Show _Only Working Hours"));
	gtk_menu_append (GTK_MENU (mts->options_menu), menuitem);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					mts->working_hours_only);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, 0, 0);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (menuitem), "toggled",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_working_hours_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_check_menu_item_new_with_label ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menuitem)->child), _("Show _Zoomed Out"));
	gtk_menu_append (GTK_MENU (mts->options_menu), menuitem);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					mts->zoomed_out);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, 0, 0);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (menuitem), "toggled",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_zoomed_out_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_menu_item_new ();
	gtk_menu_append (GTK_MENU (mts->options_menu), menuitem);
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);

	menuitem = gtk_menu_item_new_with_label ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menuitem)->child), _("_Update Free/Busy"));
	gtk_menu_append (GTK_MENU (mts->options_menu), menuitem);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, 0, 0);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_update_free_busy), mts);
	gtk_widget_show (menuitem);

	/* Create the 3 AutoPick buttons on the left. */
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_table_attach (GTK_TABLE (mts), hbox,
			  0, 1, 5, 6, GTK_FILL, 0, 0, 0);
	gtk_widget_show (hbox);

	button = gtk_button_new_with_label ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (button)->child),
					   _("_<<"));
	gtk_widget_add_accelerator (button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK | GDK_SHIFT_MASK, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_prev_button_clicked), mts);

	mts->autopick_button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), mts->autopick_button, TRUE, TRUE, 0);
	gtk_widget_show (mts->autopick_button);

	child_hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (mts->autopick_button), child_hbox);
	gtk_widget_show (child_hbox);

	label = gtk_label_new ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (label), _("_Autopick"));
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gtk_widget_add_accelerator (mts->autopick_button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (mts->autopick_button), "clicked",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_autopick_button_clicked), mts);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (child_hbox), arrow, FALSE, FALSE, 2);
	gtk_widget_show (arrow);

	button = gtk_button_new_with_label ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (button)->child),
					   _(">_>"));
	gtk_widget_add_accelerator (button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK | GDK_SHIFT_MASK, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_next_button_clicked), mts);

	/* Create the Autopick menu. */
	mts->autopick_menu = gtk_menu_new ();
	gtk_menu_attach_to_widget (GTK_MENU (mts->autopick_menu), mts->autopick_button,
				   e_meeting_time_selector_autopick_menu_detacher);
	menu_accel_group = gtk_menu_ensure_uline_accel_group (GTK_MENU (mts->autopick_menu));

	menuitem = gtk_radio_menu_item_new_with_label (NULL, "");
	mts->autopick_all_item = menuitem;
	group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menuitem)->child), _("_All People and Resources"));
	gtk_menu_append (GTK_MENU (mts->autopick_menu), menuitem);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, 0, 0);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (menuitem), "toggled",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_all_people_one_resource_item = menuitem;
	group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menuitem)->child), _("All _People and One Resource"));
	gtk_menu_append (GTK_MENU (mts->autopick_menu), menuitem);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, 0, 0);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (menuitem), "toggled",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_required_people_item = menuitem;
	group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menuitem)->child), _("_Required People"));
	gtk_menu_append (GTK_MENU (mts->autopick_menu), menuitem);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, 0, 0);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_required_people_one_resource_item = menuitem;
	group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menuitem)->child), _("Required People and _One Resource"));
	gtk_menu_append (GTK_MENU (mts->autopick_menu), menuitem);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, 0, 0);
	gtk_widget_add_accelerator (menuitem, "activate", menu_accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	/* Create the date entry fields on the right. */
	alignment = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_table_attach (GTK_TABLE (mts), alignment,
			  1, 4, 5, 6, GTK_FILL, 0, 0, 0);
	gtk_widget_show (alignment);

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_container_add (GTK_CONTAINER (alignment), table);
	gtk_widget_show (table);

	label = gtk_label_new ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (label),
					   _("Meeting _start time:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1, 0, 1, GTK_FILL, 0, 4, 0);
	gtk_widget_show (label);

	mts->start_date_edit = gnome_date_edit_new (0, TRUE, TRUE);
	/* I don't like the 'Calendar' label. */
	gtk_widget_hide (GNOME_DATE_EDIT (mts->start_date_edit)->cal_label);
	gtk_table_attach (GTK_TABLE (table), mts->start_date_edit,
			  1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (mts->start_date_edit);
	gtk_signal_connect (GTK_OBJECT (mts->start_date_edit), "date_changed",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_start_time_changed), mts);
	gtk_signal_connect (GTK_OBJECT (mts->start_date_edit), "time_changed",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_start_time_changed), mts);
	gtk_signal_connect (GTK_OBJECT (GNOME_DATE_EDIT (mts->start_date_edit)->date_entry), "activate", GTK_SIGNAL_FUNC (e_meeting_time_selector_on_start_time_changed), mts);
	gtk_signal_connect (GTK_OBJECT (GNOME_DATE_EDIT (mts->start_date_edit)->time_entry), "activate", GTK_SIGNAL_FUNC (e_meeting_time_selector_on_start_time_changed), mts);
	gtk_widget_add_accelerator (GNOME_DATE_EDIT (mts->start_date_edit)->date_entry,
				    "grab_focus", mts->accel_group,
				    accel_key, GDK_MOD1_MASK, 0);

	label = gtk_label_new ("");
	accel_key = gtk_label_parse_uline (GTK_LABEL (label),
					   _("Meeting _end time:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1, 1, 2, GTK_FILL, 0, 4, 0);
	gtk_widget_show (label);

	mts->end_date_edit = gnome_date_edit_new (0, TRUE, TRUE);
	gtk_widget_hide (GNOME_DATE_EDIT (mts->end_date_edit)->cal_label);
	gtk_table_attach (GTK_TABLE (table), mts->end_date_edit,
			  1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show (mts->end_date_edit);
	gtk_signal_connect (GTK_OBJECT (mts->end_date_edit), "date_changed",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_end_time_changed), mts);
	gtk_signal_connect (GTK_OBJECT (mts->end_date_edit), "time_changed",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_end_time_changed), mts);
	gtk_signal_connect (GTK_OBJECT (GNOME_DATE_EDIT (mts->end_date_edit)->date_entry), "activate", GTK_SIGNAL_FUNC (e_meeting_time_selector_on_end_time_changed), mts);
	gtk_signal_connect (GTK_OBJECT (GNOME_DATE_EDIT (mts->end_date_edit)->time_entry), "activate", GTK_SIGNAL_FUNC (e_meeting_time_selector_on_end_time_changed), mts);
	gtk_widget_add_accelerator (GNOME_DATE_EDIT (mts->end_date_edit)->date_entry,
				    "grab_focus", mts->accel_group,
				    accel_key, GDK_MOD1_MASK, 0);

	gtk_table_set_col_spacing (GTK_TABLE (mts), 0, 4);
	gtk_table_set_row_spacing (GTK_TABLE (mts), 4, 12);

	/* Allocate the colors. */
	visual = gtk_widget_get_visual (GTK_WIDGET (mts));
	colormap = gtk_widget_get_colormap (GTK_WIDGET (mts));
	mts->color_context = gdk_color_context_new (visual, colormap);
	e_meeting_time_selector_alloc_named_color (mts, "gray75", &mts->bg_color);
	e_meeting_time_selector_alloc_named_color (mts, "gray50", &mts->all_attendees_bg_color);
	gdk_color_black (colormap, &mts->grid_color);
	gdk_color_white (colormap, &mts->grid_shadow_color);
	e_meeting_time_selector_alloc_named_color (mts, "gray50", &mts->grid_unused_color);
	gdk_color_white (colormap, &mts->meeting_time_bg_color);
	gdk_color_white (colormap, &mts->stipple_bg_color);
	gdk_color_white (colormap, &mts->attendee_list_bg_color);

	e_meeting_time_selector_alloc_named_color (mts, "LightSkyBlue2", &mts->busy_colors[E_MEETING_TIME_SELECTOR_BUSY_TENTATIVE]);
	e_meeting_time_selector_alloc_named_color (mts, "blue", &mts->busy_colors[E_MEETING_TIME_SELECTOR_BUSY_BUSY]);
	e_meeting_time_selector_alloc_named_color (mts, "HotPink3", &mts->busy_colors[E_MEETING_TIME_SELECTOR_BUSY_OUT_OF_OFFICE]);

	/* Create the stipple, for attendees with no data. */
	mts->stipple = gdk_bitmap_create_from_data (NULL, (gchar*)stipple_bits,
						    8, 8);

	/* Connect handlers to the adjustments in the main canvas, so we can
	   scroll the other 2 canvases. */
	gtk_signal_connect (GTK_OBJECT (GTK_LAYOUT (mts->display_main)->hadjustment), "value_changed", GTK_SIGNAL_FUNC (e_meeting_time_selector_hadjustment_changed), mts);
	gtk_signal_connect (GTK_OBJECT (GTK_LAYOUT (mts->display_main)->vadjustment), "value_changed", GTK_SIGNAL_FUNC (e_meeting_time_selector_vadjustment_changed), mts);
	gtk_signal_connect (GTK_OBJECT (GTK_LAYOUT (mts->display_main)->vadjustment), "changed", GTK_SIGNAL_FUNC (e_meeting_time_selector_vadjustment_changed), mts);

	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
	e_meeting_time_selector_update_start_date_edit (mts);
	e_meeting_time_selector_update_end_date_edit (mts);
	e_meeting_time_selector_update_date_popup_menus (mts);
}


/* This adds a color to the color key beneath the main display. If color is
   NULL, it displays the No Info stipple instead. */
static void
e_meeting_time_selector_add_key_color (EMeetingTimeSelector * mts,
				       GtkWidget *hbox,
				       gchar *label_text, GdkColor *color)
{
	GtkWidget *child_hbox, *darea, *label;

	child_hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), child_hbox, TRUE, TRUE, 0);
	gtk_widget_show (child_hbox);

	darea = gtk_drawing_area_new ();
	gtk_box_pack_start (GTK_BOX (child_hbox), darea, FALSE, FALSE, 0);
	gtk_object_set_user_data (GTK_OBJECT (darea), mts);
	gtk_widget_set_usize (darea, 14, 14);
	gtk_widget_show (darea);

	label = gtk_label_new (label_text);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	gtk_signal_connect (GTK_OBJECT (darea), "expose_event",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_expose_key_color),
			    color);
}


static gint
e_meeting_time_selector_expose_title_bar (GtkWidget *widget,
					  GdkEventExpose *event,
					  gpointer data)
{
	EMeetingTimeSelector * mts;
	GdkFont *font;

	mts = E_MEETING_TIME_SELECTOR (data);

	gtk_draw_shadow (widget->style, widget->window, GTK_STATE_NORMAL,
			 GTK_SHADOW_OUT, 0, 0,
			 E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH + 1,
			 widget->allocation.height);
	gtk_draw_shadow (widget->style, widget->window, GTK_STATE_NORMAL,
			 GTK_SHADOW_OUT,
			 E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH + 1, 0,
			 widget->allocation.width - E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH - 1,
			 widget->allocation.height);

	font = widget->style->font;
	gdk_draw_string (widget->window, font,
			 widget->style->fg_gc[GTK_STATE_NORMAL],
			 E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH + 4,
			 4 + font->ascent, _("All Attendees"));

	return FALSE;
}


static gint
e_meeting_time_selector_expose_key_color (GtkWidget *darea,
					  GdkEventExpose *event,
					  GdkColor *color)
{
	EMeetingTimeSelector * mts;
	GdkGC *gc;
	gint width, height;

	mts = gtk_object_get_user_data (GTK_OBJECT (darea));
	gc = mts->color_key_gc;
	width = darea->allocation.width;
	height = darea->allocation.height;

	gtk_draw_shadow (darea->style, darea->window, GTK_STATE_NORMAL,
			 GTK_SHADOW_IN, 0, 0, width, height);

	if (color) {
		gdk_gc_set_foreground (gc, color);
		gdk_draw_rectangle (darea->window, gc, TRUE, 1, 1,
				    width - 2, height - 2);
	} else {
		gdk_gc_set_foreground (gc, &mts->grid_color);
		gdk_gc_set_background (gc, &mts->stipple_bg_color);
		gdk_gc_set_stipple (gc, mts->stipple);
		gdk_gc_set_fill (gc, GDK_OPAQUE_STIPPLED);
		gdk_draw_rectangle (darea->window, gc, TRUE, 1, 1,
				    width - 2, height - 2);
		gdk_gc_set_fill (gc, GDK_SOLID);
	}

	return TRUE;
}


static void
e_meeting_time_selector_alloc_named_color (EMeetingTimeSelector * mts,
					   const char *name, GdkColor *c)
{
	int failed;
	
	g_return_if_fail (name != NULL);
	g_return_if_fail (c != NULL);

	gdk_color_parse (name, c);
	c->pixel = 0;
	c->pixel = gdk_color_context_get_pixel (mts->color_context,
						c->red, c->green, c->blue,
						&failed);
	if (failed)
		g_warning ("Failed to allocate color: %s\n", name);
}


static void
e_meeting_time_selector_options_menu_detacher (GtkWidget *widget,
					       GtkMenu   *menu)
{
  EMeetingTimeSelector *mts;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (widget));

  mts = E_MEETING_TIME_SELECTOR (widget);
  g_return_if_fail (mts->options_menu == (GtkWidget*) menu);

  mts->options_menu = NULL;
}


static void
e_meeting_time_selector_autopick_menu_detacher (GtkWidget *widget,
						GtkMenu   *menu)
{
  EMeetingTimeSelector *mts;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (widget));

  mts = E_MEETING_TIME_SELECTOR (widget);
  g_return_if_fail (mts->autopick_menu == (GtkWidget*) menu);

  mts->autopick_menu = NULL;
}


GtkWidget *
e_meeting_time_selector_new (void)
{
	GtkWidget *mts;

	mts = GTK_WIDGET (gtk_type_new (e_meeting_time_selector_get_type ()));

	return mts;
}


static void
e_meeting_time_selector_destroy (GtkObject *object)
{
	EMeetingTimeSelector *mts;
	EMeetingTimeSelectorAttendee *attendee;
	gint row;

	mts = E_MEETING_TIME_SELECTOR (object);

	e_meeting_time_selector_remove_timeout (mts);

	gdk_color_context_free (mts->color_context);
	gdk_bitmap_unref (mts->stipple);

	for (row = 0; row < mts->attendees->len; row++) {
		attendee = &g_array_index (mts->attendees,
					   EMeetingTimeSelectorAttendee, row);
		g_free (attendee->name);
		g_array_free (attendee->busy_periods, TRUE);
	}

	g_array_free (mts->attendees, TRUE);
		
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy)(object);
}


static void
e_meeting_time_selector_realize (GtkWidget *widget)
{
	EMeetingTimeSelector *mts;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(*GTK_WIDGET_CLASS (parent_class)->realize)(widget);

	mts = E_MEETING_TIME_SELECTOR (widget);

	mts->color_key_gc = gdk_gc_new (widget->window);
}


static void
e_meeting_time_selector_unrealize (GtkWidget *widget)
{
	EMeetingTimeSelector *mts;

	mts = E_MEETING_TIME_SELECTOR (widget);

	gdk_gc_unref (mts->color_key_gc);
	mts->color_key_gc = NULL;

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize)(widget);
}


static void
e_meeting_time_selector_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
	EMeetingTimeSelector *mts;
	EMeetingTimeSelectorTime saved_time;
	GdkFont *font;
	gint hour, max_hour_width;

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(*GTK_WIDGET_CLASS (parent_class)->style_set)(widget, previous_style);

	mts = E_MEETING_TIME_SELECTOR (widget);
	font = widget->style->font;

	/* Calculate the widths of the hour strings in the style's font. */
	max_hour_width = 0;
	for (hour = 0; hour < 24; hour++) {
		mts->hour_widths[hour] = gdk_string_width (font, EMeetingTimeSelectorHours[hour]);
		max_hour_width = MAX (max_hour_width, mts->hour_widths[hour]);
	}

	mts->row_height = font->ascent + font->descent
		+ E_MEETING_TIME_SELECTOR_TEXT_Y_PAD * 2 + 1;
	mts->col_width = max_hour_width + 4;

	e_meeting_time_selector_save_position (mts, &saved_time);
	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_restore_position (mts, &saved_time);

	gtk_widget_set_usize (mts->display_top, -1, mts->row_height * 3);
	gtk_widget_set_usize (mts->attendees_title_bar, -1, mts->row_height);

	GTK_LAYOUT (mts->display_main)->hadjustment->step_increment = mts->col_width;
	GTK_LAYOUT (mts->display_main)->vadjustment->step_increment = mts->row_height;
}


/* This draws a shadow around the top display and main display. */
static gint
e_meeting_time_selector_expose_event (GtkWidget *widget,
				      GdkEventExpose *event)
{
	EMeetingTimeSelector *mts;

	mts = E_MEETING_TIME_SELECTOR (widget);

	e_meeting_time_selector_draw_shadow (mts);

	if (GTK_WIDGET_CLASS (parent_class)->expose_event)
		(*GTK_WIDGET_CLASS (parent_class)->expose_event)(widget, event);

	return FALSE;
}


static void
e_meeting_time_selector_draw (GtkWidget    *widget,
			      GdkRectangle *area)
{
	EMeetingTimeSelector *mts;

	mts = E_MEETING_TIME_SELECTOR (widget);

	e_meeting_time_selector_draw_shadow (mts);

	if (GTK_WIDGET_CLASS (parent_class)->draw)
		(*GTK_WIDGET_CLASS (parent_class)->draw)(widget, area);
}


static void
e_meeting_time_selector_draw_shadow (EMeetingTimeSelector *mts)
{
	GtkWidget *widget;
	gint x, y, w, h;

	widget = GTK_WIDGET (mts);

	/* Draw the shadow around the attendees title bar and list. */
	x = mts->attendees_title_bar->allocation.x - 2;
	y = mts->attendees_title_bar->allocation.y - 2;
	w = mts->attendees_title_bar->allocation.width + 4;
	h = mts->attendees_title_bar->allocation.height + mts->attendees_list->allocation.height + 4;

	gtk_draw_shadow (widget->style, widget->window, GTK_STATE_NORMAL,
			 GTK_SHADOW_IN, x, y, w, h);

	/* Draw the shadow around the graphical displays. */
	x = mts->display_top->allocation.x - 2;
	y = mts->display_top->allocation.y - 2;
	w = mts->display_top->allocation.width + 4;
	h = mts->display_top->allocation.height + mts->display_main->allocation.height + 4;

	gtk_draw_shadow (widget->style, widget->window, GTK_STATE_NORMAL,
			 GTK_SHADOW_IN, x, y, w, h);
}


/* When the main canvas scrolls, we scroll the other canvases. */
static void
e_meeting_time_selector_hadjustment_changed (GtkAdjustment *adjustment,
					     EMeetingTimeSelector *mts)
{
	GtkAdjustment *adj;

	adj = GTK_LAYOUT (mts->display_top)->hadjustment;
	if (adj->value != adjustment->value) {
		adj->value = adjustment->value;
		gtk_adjustment_value_changed (adj);
	}
}


static void
e_meeting_time_selector_vadjustment_changed (GtkAdjustment *adjustment,
					     EMeetingTimeSelector *mts)
{
	GtkAdjustment *adj;

	adj = GTK_LAYOUT (mts->attendees_list)->vadjustment;
	if (adj->value != adjustment->value) {
		adj->value = adjustment->value;
		gtk_adjustment_value_changed (adj);
	}
}


void
e_meeting_time_selector_get_meeting_time (EMeetingTimeSelector *mts,
					  gint *start_year,
					  gint *start_month,
					  gint *start_day,
					  gint *start_hour,
					  gint *start_minute,
					  gint *end_year,
					  gint *end_month,
					  gint *end_day,
					  gint *end_hour,
					  gint *end_minute)
{
	*start_year = g_date_year (&mts->meeting_start_time.date);
	*start_month = g_date_month (&mts->meeting_start_time.date);
	*start_day = g_date_day (&mts->meeting_start_time.date);
	*start_hour = mts->meeting_start_time.hour;
	*start_minute = mts->meeting_start_time.minute;

	*end_year = g_date_year (&mts->meeting_end_time.date);
	*end_month = g_date_month (&mts->meeting_end_time.date);
	*end_day = g_date_day (&mts->meeting_end_time.date);
	*end_hour = mts->meeting_end_time.hour;
	*end_minute = mts->meeting_end_time.minute;
}


gboolean
e_meeting_time_selector_set_meeting_time (EMeetingTimeSelector *mts,
					  gint start_year,
					  gint start_month,
					  gint start_day,
					  gint start_hour,
					  gint start_minute,
					  gint end_year,
					  gint end_month,
					  gint end_day,
					  gint end_hour,
					  gint end_minute)
{
	g_return_val_if_fail (IS_E_MEETING_TIME_SELECTOR (mts), FALSE);

	/* Check the dates are valid. */
	if (!g_date_valid_dmy (start_day, start_month, start_year)
	    || !g_date_valid_dmy (end_day, end_month, end_year)
	    || start_hour < 0 || start_hour > 23
	    || end_hour < 0 || end_hour > 23
	    || start_minute < 0 || start_minute > 59
	    || end_minute < 0 || end_minute > 59)
		return FALSE;

	g_date_set_dmy (&mts->meeting_start_time.date, start_day, start_month,
			start_year);
	mts->meeting_start_time.hour = start_hour;
	mts->meeting_start_time.minute = start_minute;
	g_date_set_dmy (&mts->meeting_end_time.date, end_day, end_month,
			end_year);
	mts->meeting_end_time.hour = end_hour;
	mts->meeting_end_time.minute = end_minute;

	mts->meeting_positions_valid = FALSE;

	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);

	/* Set the times in the GnomeDateEdit widgets. */
	e_meeting_time_selector_update_start_date_edit (mts);
	e_meeting_time_selector_update_end_date_edit (mts);

	return TRUE;
}


void
e_meeting_time_selector_set_working_hours_only (EMeetingTimeSelector *mts,
						gboolean working_hours_only)
{
	EMeetingTimeSelectorTime saved_time;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));

	if (mts->working_hours_only == working_hours_only)
		return;

	mts->working_hours_only = working_hours_only;

	e_meeting_time_selector_save_position (mts, &saved_time);
	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_restore_position (mts, &saved_time);

	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
	e_meeting_time_selector_update_date_popup_menus (mts);
}


void
e_meeting_time_selector_set_working_hours (EMeetingTimeSelector *mts,
					   gint day_start_hour,
					   gint day_start_minute,
					   gint day_end_hour,
					   gint day_end_minute)
{
	EMeetingTimeSelectorTime saved_time;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));

	if (mts->day_start_hour == day_start_hour
	    && mts->day_start_minute == day_start_minute
	    && mts->day_end_hour == day_end_hour
	    && mts->day_end_minute == day_end_minute)
		return;

	mts->day_start_hour = day_start_hour;
	mts->day_start_minute = day_start_minute;
	mts->day_end_hour = day_end_hour;
	mts->day_end_minute = day_end_minute;

	e_meeting_time_selector_save_position (mts, &saved_time);
	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_restore_position (mts, &saved_time);

	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
	e_meeting_time_selector_update_date_popup_menus (mts);
}


void
e_meeting_time_selector_set_zoomed_out (EMeetingTimeSelector *mts,
					gboolean zoomed_out)
{
	EMeetingTimeSelectorTime saved_time;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));

	if (mts->zoomed_out == zoomed_out)
		return;

	mts->zoomed_out = zoomed_out;

	e_meeting_time_selector_save_position (mts, &saved_time);
	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_restore_position (mts, &saved_time);

	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
}


EMeetingTimeSelectorAutopickOption
e_meeting_time_selector_get_autopick_option (EMeetingTimeSelector *mts)
{
	if (GTK_CHECK_MENU_ITEM (mts->autopick_all_item)->active)
		return E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_RESOURCES;
	if (GTK_CHECK_MENU_ITEM (mts->autopick_all_people_one_resource_item)->active)
		return E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_ONE_RESOURCE;
	if (GTK_CHECK_MENU_ITEM (mts->autopick_required_people_item)->active)
		return E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE;
	return E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE_AND_ONE_RESOURCE;
}


void
e_meeting_time_selector_set_autopick_option (EMeetingTimeSelector *mts,
					     EMeetingTimeSelectorAutopickOption autopick_option)
{
	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));

	switch (autopick_option) {
	case E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_RESOURCES:
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mts->autopick_all_item), TRUE);
		break;
	case E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_ONE_RESOURCE:
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mts->autopick_all_people_one_resource_item), TRUE);
		break;
	case E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE:
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mts->autopick_required_people_item), TRUE);
		break;
	case E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE_AND_ONE_RESOURCE:
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mts->autopick_required_people_one_resource_item), TRUE);
		break;
	}
}


/* Adds an attendee to the list, returning the row. The data is meant for
   something like an address book id, though if the user edits the name this
   will become invalid. We'll probably have to handle address book lookup
   ourself. */
gint
e_meeting_time_selector_attendee_add (EMeetingTimeSelector *mts,
				      gchar *attendee_name,
				      gpointer data)
{
	EMeetingTimeSelectorAttendee attendee;
	gint list_width, item_width;
	GdkFont *font;

	g_return_val_if_fail (IS_E_MEETING_TIME_SELECTOR (mts), -1);
	g_return_val_if_fail (attendee_name != NULL, -1);

	attendee.name = g_strdup (attendee_name);
	attendee.type = E_MEETING_TIME_SELECTOR_REQUIRED_PERSON;
	attendee.has_calendar_info = FALSE;
	attendee.send_meeting_to = TRUE;
	g_date_clear (&attendee.busy_periods_start.date, 1);
	attendee.busy_periods_start.hour = 0;
	attendee.busy_periods_start.minute = 0;
	g_date_clear (&attendee.busy_periods_end.date, 1);
	attendee.busy_periods_end.hour = 0;
	attendee.busy_periods_end.minute = 0;
	attendee.busy_periods = g_array_new (FALSE, FALSE,
					     sizeof (EMeetingTimeSelectorPeriod));
	attendee.busy_periods_sorted = TRUE;
	attendee.longest_period_in_days = 0;
	attendee.data = data;

	/* Add to the list on the left. */
	list_width = GTK_WIDGET (mts->attendees_list)->allocation.width;
	item_width = MAX (1, list_width - E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH - E_MEETING_TIME_SELECTOR_TEXT_X_PAD * 2);
	font = GTK_WIDGET (mts)->style->font;
	attendee.text_item = gnome_canvas_item_new
		(GNOME_CANVAS_GROUP (GNOME_CANVAS (mts->attendees_list)->root),
		 e_text_get_type (),
		 "font_gdk", font,
		 "anchor", GTK_ANCHOR_NW,
		 "clip", TRUE,
		 "max_lines", 1,
		 "editable", TRUE,
		 "text", attendee_name ? attendee_name : "",
		 "x", (gdouble) E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH
		 + E_MEETING_TIME_SELECTOR_TEXT_X_PAD,
		 "y", (gdouble) (mts->attendees->len * mts->row_height + 1
				 + E_MEETING_TIME_SELECTOR_TEXT_Y_PAD),
		 "clip_width", (gdouble) item_width,
		 "clip_height", (gdouble) font->ascent + font->descent,
		 NULL);
#if 0
	gnome_canvas_item_hide (attendee.text_item);
#endif

	gtk_signal_connect (GTK_OBJECT (attendee.text_item), "event",
			    GTK_SIGNAL_FUNC (e_meeting_time_selector_on_text_item_event),
			    mts);
	
	g_array_append_val (mts->attendees, attendee);

	/* Update the scroll region. */
	e_meeting_time_selector_update_attendees_list_scroll_region (mts);
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);

	/* Redraw the canvases. */
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);


	return mts->attendees->len - 1;
}


gint
e_meeting_time_selector_attendee_find_by_name (EMeetingTimeSelector *mts,
					       gchar *attendee_name,
					       gint start_row)
{
	EMeetingTimeSelectorAttendee *attendee;
	gint row;

	g_return_val_if_fail (IS_E_MEETING_TIME_SELECTOR (mts), -1);
	g_return_val_if_fail (start_row >= 0, -1);
	g_return_val_if_fail (start_row < mts->attendees->len, -1);

	for (row = start_row; row < mts->attendees->len; row++) {
		attendee = &g_array_index (mts->attendees,
					   EMeetingTimeSelectorAttendee, row);
		if (!strcmp (attendee->name, attendee_name))
			return row;
	}

	return -1;
}


gint
e_meeting_time_selector_attendee_find_by_data (EMeetingTimeSelector *mts,
					       gpointer data,
					       gint start_row)
{
	EMeetingTimeSelectorAttendee *attendee;
	gint row;

	g_return_val_if_fail (IS_E_MEETING_TIME_SELECTOR (mts), -1);
	g_return_val_if_fail (start_row >= 0, -1);
	g_return_val_if_fail (start_row < mts->attendees->len, -1);

	for (row = start_row; row < mts->attendees->len; row++) {
		attendee = &g_array_index (mts->attendees,
					   EMeetingTimeSelectorAttendee, row);
		if (attendee->data == data)
			return row;
	}

	return -1;
}


void
e_meeting_time_selector_attendee_remove (EMeetingTimeSelector *mts,
					 gint row)
{
	EMeetingTimeSelectorAttendee *attendee;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < mts->attendees->len);

	attendee = &g_array_index (mts->attendees,
				   EMeetingTimeSelectorAttendee, row);

	g_free (attendee->name);
	g_array_free (attendee->busy_periods, TRUE);

	/* Destroy the GtkEntry in the list. */
	gtk_object_destroy (GTK_OBJECT (attendee->text_item));

	g_array_remove_index (mts->attendees, row);

	/* Update the positions of all the other GtkEntry widgets. */
	e_meeting_time_selector_update_attendees_list_positions (mts);

	e_meeting_time_selector_update_attendees_list_scroll_region (mts);
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
}


void
e_meeting_time_selector_attendee_set_type (EMeetingTimeSelector *mts,
					   gint row,
					   EMeetingTimeSelectorAttendeeType type)
{
	EMeetingTimeSelectorAttendee *attendee;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < mts->attendees->len);

	attendee = &g_array_index (mts->attendees,
				   EMeetingTimeSelectorAttendee, row);
	attendee->type = type;
}


void
e_meeting_time_selector_attendee_set_has_calendar_info (EMeetingTimeSelector *mts,
							gint row,
							gboolean has_calendar_info)
{
	EMeetingTimeSelectorAttendee *attendee;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < mts->attendees->len);

	attendee = &g_array_index (mts->attendees,
				   EMeetingTimeSelectorAttendee, row);
	attendee->has_calendar_info = has_calendar_info;
}


void
e_meeting_time_selector_attendee_set_send_meeting_to (EMeetingTimeSelector *mts,
						      gint row,
						      gboolean send_meeting_to)
{
	EMeetingTimeSelectorAttendee *attendee;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < mts->attendees->len);

	attendee = &g_array_index (mts->attendees,
				   EMeetingTimeSelectorAttendee, row);
	attendee->send_meeting_to = send_meeting_to;
}


gboolean
e_meeting_time_selector_attendee_set_busy_range	(EMeetingTimeSelector *mts,
						 gint row,
						 gint start_year,
						 gint start_month,
						 gint start_day,
						 gint start_hour,
						 gint start_minute,
						 gint end_year,
						 gint end_month,
						 gint end_day,
						 gint end_hour,
						 gint end_minute)
{
	EMeetingTimeSelectorAttendee *attendee;

	g_return_val_if_fail (IS_E_MEETING_TIME_SELECTOR (mts), FALSE);
	g_return_val_if_fail (row >= 0, FALSE);
	g_return_val_if_fail (row < mts->attendees->len, FALSE);

	/* Check the dates are valid. */
	if (!g_date_valid_dmy (start_day, start_month, start_year))
		return FALSE;
	if (!g_date_valid_dmy (end_day, end_month, end_year))
		return FALSE;
	if (start_hour < 0 || start_hour > 23)
		return FALSE;
	if (end_hour < 0 || end_hour > 23)
		return FALSE;
	if (start_minute < 0 || start_minute > 59)
		return FALSE;
	if (end_minute < 0 || end_minute > 59)
		return FALSE;

	attendee = &g_array_index (mts->attendees,
				   EMeetingTimeSelectorAttendee, row);

	g_date_clear (&attendee->busy_periods_start.date, 1);
	g_date_clear (&attendee->busy_periods_end.date, 1);
	g_date_set_dmy (&attendee->busy_periods_start.date,
			start_day, start_month, start_year);
	g_date_set_dmy (&attendee->busy_periods_end.date,
			end_day, end_month, end_year);
	attendee->busy_periods_start.hour = start_hour;
	attendee->busy_periods_start.minute = start_minute;
	attendee->busy_periods_end.hour = end_hour;
	attendee->busy_periods_end.minute = end_minute;

	return TRUE;
}


/* Clears all busy times for the given attendee. */
void
e_meeting_time_selector_attendee_clear_busy_periods (EMeetingTimeSelector *mts,
						     gint row)
{
	EMeetingTimeSelectorAttendee *attendee;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < mts->attendees->len);

	attendee = &g_array_index (mts->attendees,
				   EMeetingTimeSelectorAttendee, row);

	g_array_set_size (attendee->busy_periods, 0);
	attendee->busy_periods_sorted = TRUE;
	attendee->longest_period_in_days = 0;
}


/* Adds one busy time for the given attendee. It returns FALSE if the date
   or time is invalid. Months and days count from 1. */
gboolean
e_meeting_time_selector_attendee_add_busy_period (EMeetingTimeSelector *mts,
						  gint row,
						  gint start_year,
						  gint start_month,
						  gint start_day,
						  gint start_hour,
						  gint start_minute,
						  gint end_year,
						  gint end_month,
						  gint end_day,
						  gint end_hour,
						  gint end_minute,
						  EMeetingTimeSelectorBusyType busy_type)
{
	EMeetingTimeSelectorAttendee *attendee;
	EMeetingTimeSelectorPeriod period;
	gint period_in_days;

	g_return_val_if_fail (IS_E_MEETING_TIME_SELECTOR (mts), FALSE);
	g_return_val_if_fail (row >= 0, FALSE);
	g_return_val_if_fail (row < mts->attendees->len, FALSE);
	g_return_val_if_fail (busy_type >= 0, FALSE);
	g_return_val_if_fail (busy_type < E_MEETING_TIME_SELECTOR_BUSY_LAST, FALSE);

	/* Check the dates are valid. */
	if (!g_date_valid_dmy (start_day, start_month, start_year))
		return FALSE;
	if (!g_date_valid_dmy (end_day, end_month, end_year))
		return FALSE;
	if (start_hour < 0 || start_hour > 23)
		return FALSE;
	if (end_hour < 0 || end_hour > 23)
		return FALSE;
	if (start_minute < 0 || start_minute > 59)
		return FALSE;
	if (end_minute < 0 || end_minute > 59)
		return FALSE;

	g_date_clear (&period.start.date, 1);
	g_date_clear (&period.end.date, 1);
	g_date_set_dmy (&period.start.date, start_day, start_month, start_year);
	g_date_set_dmy (&period.end.date, end_day, end_month, end_year);
	period.start.hour = start_hour;
	period.start.minute = start_minute;
	period.end.hour = end_hour;
	period.end.minute = end_minute;
	period.busy_type = busy_type;

	/* Check that the start time is before or equal to the end time. */
	if (e_meeting_time_selector_compare_times (&period.start, &period.end) > 0)
		return FALSE;

	attendee = &g_array_index (mts->attendees,
				   EMeetingTimeSelectorAttendee, row);
	g_array_append_val (attendee->busy_periods, period);
	attendee->has_calendar_info = TRUE;
	attendee->busy_periods_sorted = FALSE;

	period_in_days = g_date_julian (&period.end.date) - g_date_julian (&period.start.date) + 1;
	attendee->longest_period_in_days = MAX (attendee->longest_period_in_days, period_in_days);

	return TRUE;
}


void
e_meeting_time_selector_attendee_ensure_periods_sorted (EMeetingTimeSelector *mts,
							EMeetingTimeSelectorAttendee *attendee)
{
	if (attendee->busy_periods_sorted)
		return;

	qsort (attendee->busy_periods->data, attendee->busy_periods->len,
	       sizeof (EMeetingTimeSelectorPeriod),
	       e_meeting_time_selector_compare_period_starts);
	attendee->busy_periods_sorted = TRUE;
}


/* This compares two time periods, using their end times. */
static gint
e_meeting_time_selector_compare_period_starts (const void *arg1,
					       const void *arg2)
{
	EMeetingTimeSelectorPeriod *period1, *period2;

	period1 = (EMeetingTimeSelectorPeriod *) arg1;
	period2 = (EMeetingTimeSelectorPeriod *) arg2;

	return e_meeting_time_selector_compare_times (&period1->start,
						    &period2->start);
}


/* This compares two time periods, using start and end times, mainly to see if
   they overlap at all. If they overlap it returns 0. Or -1 if arg1 < arg2.
   Or 1 if arg1 > arg2. */
/* Currently unused. */
#if 0
static gint
e_meeting_time_selector_compare_periods (const void *arg1,
					 const void *arg2)
{
	EMeetingTimeSelectorPeriod *period1, *period2;

	period1 = (EMeetingTimeSelectorPeriod *) arg1;
	period2 = (EMeetingTimeSelectorPeriod *) arg2;

	/* If period 2 starts after period 1 ends, return 1. */
	if (e_meeting_time_selector_compare_times (&period2->start, &period1->end) >= 0)
		return 1;

	/* If period 1 starts after period 2 ends, return -1. */
	if (e_meeting_time_selector_compare_times (&period1->start, &period2->end) >= 0)
		return -1;

	/* They must overlap so return 0. */
	return 0;
}
#endif


static gint
e_meeting_time_selector_compare_times (EMeetingTimeSelectorTime *time1,
				       EMeetingTimeSelectorTime *time2)
{
	gint day_comparison;

	day_comparison = g_date_compare (&time1->date,
					 &time2->date);
	if (day_comparison != 0)
		return day_comparison;

	if (time1->hour < time2->hour)
		return -1;
	if (time1->hour > time2->hour)
		return 1;

	if (time1->minute < time2->minute)
		return -1;
	if (time1->minute > time2->minute)
		return 1;

	/* The start times are exactly the same. */
	return 0;
}


/*
 * DEBUGGING ROUTINES - functions to output various bits of data.
 */

#ifdef E_MEETING_TIME_SELECTOR_DEBUG

/* Debugging function to dump information on all attendees. */
void
e_meeting_time_selector_dump (EMeetingTimeSelector *mts)
{
	EMeetingTimeSelectorAttendee *attendee;
	EMeetingTimeSelectorPeriod *period;
	gint row, period_num;
	gchar buffer[128];

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));

	g_print ("\n\nAttendee Information:\n");

	for (row = 0; row < mts->attendees->len; row++) {
		attendee = &g_array_index (mts->attendees,
					   EMeetingTimeSelectorAttendee, row);
		g_print ("Attendee: %s\n", attendee->name);
		g_print ("  Longest Busy Period: %i days\n",
			 attendee->longest_period_in_days);

		e_meeting_time_selector_attendee_ensure_periods_sorted (mts, attendee);
#if 1
		for (period_num = 0;
		     period_num < attendee->busy_periods->len;
		     period_num++) {
			period = &g_array_index (attendee->busy_periods,
						 EMeetingTimeSelectorPeriod,
						 period_num);

			g_date_strftime (buffer, 128, "%A, %B %d, %Y",
					 &period->start.date);
			g_print ("  Start: %s %i:%02i\n", buffer,
				 period->start.hour, period->start.minute);

			g_date_strftime (buffer, 128, "%A, %B %d, %Y",
					 &period->end.date);
			g_print ("  End  : %s %i:%02i\n", buffer,
				 period->end.hour, period->end.minute);
		}
#endif
	}

}


/* This formats a EMeetingTimeSelectorTime in a string and returns it.
   Note that it uses a static buffer. */
gchar*
e_meeting_time_selector_dump_time (EMeetingTimeSelectorTime *mtstime)
{
	static gchar buffer[128];

	gchar buffer2[128];

	g_date_strftime (buffer, 128, "%A, %B %d, %Y", &mtstime->date);
	sprintf (buffer2, " at %i:%02i", (gint) mtstime->hour,
		 (gint) mtstime->minute);
	strcat (buffer, buffer2);

	return buffer;
}


/* This formats a GDate in a string and returns it.
   Note that it uses a static buffer. */
gchar*
e_meeting_time_selector_dump_date (GDate *date)
{
	static gchar buffer[128];

	g_date_strftime (buffer, 128, "%A, %B %d, %Y", date);
	return buffer;
}

#endif /* E_MEETING_TIME_SELECTOR_DEBUG */


static void
e_meeting_time_selector_on_invite_others_button_clicked (GtkWidget *button,
							 EMeetingTimeSelector *mts)
{


}


static void
e_meeting_time_selector_on_options_button_clicked (GtkWidget *button,
						   EMeetingTimeSelector *mts)
{
	gtk_menu_popup (GTK_MENU (mts->options_menu), NULL, NULL,
			e_meeting_time_selector_options_menu_position_callback,
			mts, 1, GDK_CURRENT_TIME);
}


static void
e_meeting_time_selector_options_menu_position_callback (GtkMenu *menu,
							gint *x,
							gint *y,
							gpointer user_data)
{
	EMeetingTimeSelector *mts;
	GtkRequisition menu_requisition;
	gint max_x, max_y;

	mts = E_MEETING_TIME_SELECTOR (user_data);

	/* Calculate our preferred position. */
	gdk_window_get_origin (mts->options_button->window, x, y);
	*y += mts->options_button->allocation.height;

	/* Now make sure we are on the screen. */
	gtk_widget_size_request (mts->options_menu, &menu_requisition);
	max_x = MAX (0, gdk_screen_width () - menu_requisition.width);
	max_y = MAX (0, gdk_screen_height () - menu_requisition.height);
	*x = CLAMP (*x, 0, max_x);
	*y = CLAMP (*y, 0, max_y);
}


static void
e_meeting_time_selector_on_update_free_busy (GtkWidget *button,
					     EMeetingTimeSelector *mts)
{

	/* Make sure the menu pops down, which doesn't happen by default if
	   keyboard accelerators are used. */
	if (GTK_WIDGET_VISIBLE (mts->options_menu))
		gtk_menu_popdown (GTK_MENU (mts->options_menu));
}


static void
e_meeting_time_selector_on_autopick_button_clicked (GtkWidget *button,
						    EMeetingTimeSelector *mts)
{
	gtk_menu_popup (GTK_MENU (mts->autopick_menu), NULL, NULL,
			e_meeting_time_selector_autopick_menu_position_callback,
			mts, 1, GDK_CURRENT_TIME);
}


static void
e_meeting_time_selector_autopick_menu_position_callback (GtkMenu *menu,
							 gint *x,
							 gint *y,
							 gpointer user_data)
{
	EMeetingTimeSelector *mts;
	GtkRequisition menu_requisition;
	gint max_x, max_y;

	mts = E_MEETING_TIME_SELECTOR (user_data);

	/* Calculate our preferred position. */
	gdk_window_get_origin (mts->autopick_button->window, x, y);
	*y += mts->autopick_button->allocation.height;

	/* Now make sure we are on the screen. */
	gtk_widget_size_request (mts->autopick_menu, &menu_requisition);
	max_x = MAX (0, gdk_screen_width () - menu_requisition.width);
	max_y = MAX (0, gdk_screen_height () - menu_requisition.height);
	*x = CLAMP (*x, 0, max_x);
	*y = CLAMP (*y, 0, max_y);
}


static void
e_meeting_time_selector_on_autopick_option_toggled (GtkWidget *button,
						    EMeetingTimeSelector *mts)
{
	/* Make sure the menu pops down, which doesn't happen by default if
	   keyboard accelerators are used. */
	if (GTK_WIDGET_VISIBLE (mts->autopick_menu))
		gtk_menu_popdown (GTK_MENU (mts->autopick_menu));
}


static void
e_meeting_time_selector_on_prev_button_clicked (GtkWidget *button,
						EMeetingTimeSelector *mts)
{
	e_meeting_time_selector_autopick (mts, FALSE);
}


static void
e_meeting_time_selector_on_next_button_clicked (GtkWidget *button,
						EMeetingTimeSelector *mts)
{
	e_meeting_time_selector_autopick (mts, TRUE);
}


/* This tries to find the previous or next meeting time for which all
   attendees will be available. */
static void
e_meeting_time_selector_autopick (EMeetingTimeSelector *mts,
				  gboolean forward)
{
	EMeetingTimeSelectorTime start_time, end_time, *resource_free;
	EMeetingTimeSelectorAttendee *attendee;
	EMeetingTimeSelectorPeriod *period;
	EMeetingTimeSelectorAutopickOption autopick_option;
	gint duration_days, duration_hours, duration_minutes, row;
	gboolean meeting_time_ok, skip_optional = FALSE;
	gboolean need_one_resource = FALSE, found_resource;

	/* Get the current meeting duration in days + hours + minutes. */
	e_meeting_time_selector_calculate_time_difference (&mts->meeting_start_time, &mts->meeting_end_time, &duration_days, &duration_hours, &duration_minutes);

	/* Find the first appropriate start time. */
	start_time = mts->meeting_start_time;
	if (forward)
		e_meeting_time_selector_find_nearest_interval (mts, &start_time, &end_time, duration_days, duration_hours, duration_minutes);
	else
		e_meeting_time_selector_find_nearest_interval_backward (mts, &start_time, &end_time, duration_days, duration_hours, duration_minutes);

	/* Determine if we can skip optional people and if we only need one
	   resource based on the autopick option. */
	autopick_option = e_meeting_time_selector_get_autopick_option (mts);
	if (autopick_option == E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE
	    || autopick_option == E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE_AND_ONE_RESOURCE)
		skip_optional = TRUE;
	if (autopick_option == E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_ONE_RESOURCE
	    || autopick_option == E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE_AND_ONE_RESOURCE)
		need_one_resource = TRUE;

	/* Keep moving forward or backward until we find a possible meeting
	   time. */
	for (;;) {
		meeting_time_ok = TRUE;
		found_resource = FALSE;
		resource_free = NULL;

		/* Step through each attendee, checking if the meeting time
		   intersects one of the attendees busy periods. */
		for (row = 0; row < mts->attendees->len; row++) {
			attendee = &g_array_index (mts->attendees,
						   EMeetingTimeSelectorAttendee,
						   row);

			/* Skip optional people if they don't matter. */
			if (skip_optional && attendee->type == E_MEETING_TIME_SELECTOR_OPTIONAL_PERSON)
				continue;

			period = e_meeting_time_selector_find_time_clash (mts, attendee, &start_time, &end_time);

			if (need_one_resource && attendee->type == E_MEETING_TIME_SELECTOR_RESOURCE) {
				if (period) {
					/* We want to remember the closest
					   prev/next time that one resource is
					   available, in case we don't find any
					   free resources. */
					if (forward) {
						if (!resource_free || e_meeting_time_selector_compare_times (resource_free, &period->end) > 0)
							resource_free = &period->end;
					} else {
						if (!resource_free || e_meeting_time_selector_compare_times (resource_free, &period->start) < 0)
							resource_free = &period->start;
					}

				} else {
					found_resource = TRUE;
				}
			} else if (period) {
				/* Skip the period which clashed. */
				if (forward) {
					start_time = period->end;
				} else {
					start_time = period->start;
					e_meeting_time_selector_adjust_time (&start_time, -duration_days, -duration_hours, -duration_minutes);
				}
				meeting_time_ok = FALSE;
				break;
			}
		}

		/* Check that we found one resource if necessary. If not, skip
		   to the closest time that a resource is free. Note that if
		   there are no resources, resource_free will never get set,
		   so we assume the meeting time is OK. */
		if (meeting_time_ok && need_one_resource && !found_resource
		    && resource_free) {
			if (forward) {
				start_time = *resource_free;
			} else {
				start_time = *resource_free;
				e_meeting_time_selector_adjust_time (&start_time, -duration_days, -duration_hours, -duration_minutes);
				}
			meeting_time_ok = FALSE;
		}

		if (meeting_time_ok) {
			mts->meeting_start_time = start_time;
			mts->meeting_end_time = end_time;
			mts->meeting_positions_valid = FALSE;
			gtk_widget_queue_draw (mts->display_top);
			gtk_widget_queue_draw (mts->display_main);

			/* Make sure the time is shown. */
			e_meeting_time_selector_ensure_meeting_time_shown (mts);

			/* Set the times in the GnomeDateEdit widgets. */
			e_meeting_time_selector_update_start_date_edit (mts);
			e_meeting_time_selector_update_end_date_edit (mts);
			return;
		}

		/* Move forward to the next possible interval. */
		if (forward)
			e_meeting_time_selector_find_nearest_interval (mts, &start_time, &end_time, duration_days, duration_hours, duration_minutes);
		else
			e_meeting_time_selector_find_nearest_interval_backward (mts, &start_time, &end_time, duration_days, duration_hours, duration_minutes);
	}
}


static void
e_meeting_time_selector_calculate_time_difference (EMeetingTimeSelectorTime *start,
						   EMeetingTimeSelectorTime *end,
						   gint *days,
						   gint *hours,
						   gint *minutes)
{
	*days = g_date_julian (&end->date) - g_date_julian (&start->date);
	*hours = end->hour - start->hour;
	*minutes = end->minute - start->minute;
	if (*minutes < 0) {
		*minutes += 60;
		*hours = *hours - 1;
	}
	if (*hours < 0) {
		*hours += 24;
		*days = *days - 1;
	}
}


/* This moves the given time forward to the next suitable start of a meeting.
   If zoomed_out is set, this means every hour. If not every half-hour. */
static void
e_meeting_time_selector_find_nearest_interval (EMeetingTimeSelector *mts,
					       EMeetingTimeSelectorTime *start_time,
					       EMeetingTimeSelectorTime *end_time,
					       gint days, gint hours, gint mins)
{
	gint minutes_shown;
	gboolean set_to_start_of_working_day = FALSE;

	if (mts->zoomed_out) {
		start_time->hour++;
		start_time->minute = 0;
	} else {
		start_time->minute += 30;
		start_time->minute -= start_time->minute % 30;
	}
	e_meeting_time_selector_fix_time_overflows (start_time);

	*end_time = *start_time;
	e_meeting_time_selector_adjust_time (end_time, days, hours, mins);

	/* Check if the interval is less than a day as seen in the display.
	   If it isn't we don't worry about the working day. */
	if (!mts->working_hours_only || days > 0)
		return;
	minutes_shown = (mts->day_end_hour - mts->day_start_hour) * 60;
	minutes_shown += mts->day_end_minute - mts->day_start_minute;
	if (hours * 60 + mins > minutes_shown)
		return;

	/* If the meeting time finishes past the end of the working day, move
	   onto the start of the next working day. If the meeting time starts
	   before the working day, move it on as well. */
	if (start_time->hour > mts->day_end_hour
	    || (start_time->hour == mts->day_end_hour
		&& start_time->minute > mts->day_end_minute)
	    || end_time->hour > mts->day_end_hour
	    || (end_time->hour == mts->day_end_hour
		&& end_time->minute > mts->day_end_minute)) {
		g_date_add_days (&start_time->date, 1);
		set_to_start_of_working_day = TRUE;
	} else if (start_time->hour < mts->day_start_hour
		   || (start_time->hour == mts->day_start_hour
		       && start_time->minute < mts->day_start_minute)) {
		set_to_start_of_working_day = TRUE;
	}

	if (set_to_start_of_working_day) {
		start_time->hour = mts->day_start_hour;
		start_time->minute = mts->day_start_minute;

		if (mts->zoomed_out) {
			if (start_time->minute > 0) {
				start_time->hour++;
				start_time->minute = 0;
			}
		} else {
			start_time->minute += 29;
			start_time->minute -= start_time->minute % 30;
		}
		e_meeting_time_selector_fix_time_overflows (start_time);

		*end_time = *start_time;
		e_meeting_time_selector_adjust_time (end_time, days, hours, mins);
	}
}


/* This moves the given time backward to the next suitable start of a meeting.
   If zoomed_out is set, this means every hour. If not every half-hour. */
static void
e_meeting_time_selector_find_nearest_interval_backward (EMeetingTimeSelector *mts,
							EMeetingTimeSelectorTime *start_time,
							EMeetingTimeSelectorTime *end_time,
							gint days, gint hours, gint mins)
{
	gint new_hour, minutes_shown;
	gboolean set_to_end_of_working_day = FALSE;

	new_hour = start_time->hour;
	if (mts->zoomed_out) {
		if (start_time->minute == 0)
			new_hour--;
		start_time->minute = 0;
	} else {
		if (start_time->minute == 0) {
			start_time->minute = 30;
			new_hour--;
		} else if (start_time->minute <= 30)
			start_time->minute = 0;
		else
			start_time->minute = 30;
	}
	if (new_hour < 0) {
		new_hour += 24;
		g_date_subtract_days (&start_time->date, 1);
	}
	start_time->hour = new_hour;

	*end_time = *start_time;
	e_meeting_time_selector_adjust_time (end_time, days, hours, mins);

	/* Check if the interval is less than a day as seen in the display.
	   If it isn't we don't worry about the working day. */
	if (!mts->working_hours_only || days > 0)
		return;
	minutes_shown = (mts->day_end_hour - mts->day_start_hour) * 60;
	minutes_shown += mts->day_end_minute - mts->day_start_minute;
	if (hours * 60 + mins > minutes_shown)
		return;

	/* If the meeting time finishes past the end of the working day, move
	   back to the end of the working day. If the meeting time starts
	   before the working day, move it back to the end of the previous
	   working day. */
	if (start_time->hour > mts->day_end_hour
	    || (start_time->hour == mts->day_end_hour
		&& start_time->minute > mts->day_end_minute)
	    || end_time->hour > mts->day_end_hour
	    || (end_time->hour == mts->day_end_hour
		&& end_time->minute > mts->day_end_minute)) {
		set_to_end_of_working_day = TRUE;
	} else if (start_time->hour < mts->day_start_hour
		   || (start_time->hour == mts->day_start_hour
		       && start_time->minute < mts->day_start_minute)) {
		g_date_subtract_days (&end_time->date, 1);
		set_to_end_of_working_day = TRUE;
	}

	if (set_to_end_of_working_day) {
		end_time->hour = mts->day_end_hour;
		end_time->minute = mts->day_end_minute;
		*start_time = *end_time;
		e_meeting_time_selector_adjust_time (start_time, -days, -hours, -mins);

		if (mts->zoomed_out) {
			start_time->minute = 0;
		} else {
			start_time->minute -= start_time->minute % 30;
		}

		*end_time = *start_time;
		e_meeting_time_selector_adjust_time (end_time, days, hours, mins);
	}
}


/* This adds on the given days, hours & minutes to a EMeetingTimeSelectorTime.
   It is used to calculate the end of a period given a start & duration.
   Days, hours & minutes can be negative, to move backwards, but they should
   be within normal ranges, e.g. hours should be between -23 and 23. */
static void
e_meeting_time_selector_adjust_time (EMeetingTimeSelectorTime *mtstime,
				     gint days, gint hours, gint minutes)
{
	gint new_hours, new_minutes;

	/* We have to handle negative values for hous and minutes here, since
	   EMeetingTimeSelectorTime uses guint8s to store them. */
	new_minutes = mtstime->minute + minutes;
	if (new_minutes < 0) {
		new_minutes += 60;
		hours -= 1;
	}

	new_hours = mtstime->hour + hours;
	if (new_hours < 0) {
		new_hours += 24;
		days -= 1;
	}

	g_date_add_days (&mtstime->date, days);
	mtstime->hour = new_hours;
	mtstime->minute = new_minutes;

	e_meeting_time_selector_fix_time_overflows (mtstime);
}


/* This looks for any busy period of the given attendee which clashes with
   the start and end time. It uses a binary search. */
static EMeetingTimeSelectorPeriod*
e_meeting_time_selector_find_time_clash (EMeetingTimeSelector *mts,
					 EMeetingTimeSelectorAttendee *attendee,
					 EMeetingTimeSelectorTime *start_time,
					 EMeetingTimeSelectorTime *end_time)
{
	EMeetingTimeSelectorPeriod *period;
	gint period_num;

	period_num = e_meeting_time_selector_find_first_busy_period (mts, attendee, &start_time->date);

	if (period_num == -1)
		return NULL;

	/* Step forward through the busy periods until we find a clash or we
	   go past the end_time. */
	while (period_num < attendee->busy_periods->len) {
		period = &g_array_index (attendee->busy_periods,
					 EMeetingTimeSelectorPeriod,
					 period_num);

		/* If the period starts at or after the end time, there is no
		   clash and we are finished. The busy periods are sorted by
		   their start times, so all the rest will be later. */
		if (e_meeting_time_selector_compare_times (&period->start,
							 end_time) >= 0)
			return NULL;

		/* If the period ends after the start time, we have found a
		   clash. From the above test we already know the busy period
		   isn't completely after the meeting time. */
		if (e_meeting_time_selector_compare_times (&period->end,
							 start_time) > 0) {
			return period;
		}

		period_num++;
	}

	return NULL;
}


/* This subtracts the attendees longest_period_in_days from the given date,
   and does a binary search of the attendee's busy periods array to find the
   first one which could possible end on the given day or later.
   If none are found it returns -1. */
gint
e_meeting_time_selector_find_first_busy_period (EMeetingTimeSelector *mts,
						EMeetingTimeSelectorAttendee *attendee,
						GDate *date)
{
	EMeetingTimeSelectorPeriod *period;
	gint lower, upper, middle = 0, cmp = 0;
	GDate tmp_date;

	/* Make sure the busy periods have been sorted. */
	e_meeting_time_selector_attendee_ensure_periods_sorted (mts, attendee);

	/* Calculate the first day which could have a busy period which
	   continues onto our given date. */
	tmp_date = *date;
	g_date_subtract_days (&tmp_date, attendee->longest_period_in_days);

	/* We want the first busy period which starts on tmp_date. */
	lower = 0;
	upper = attendee->busy_periods->len;

	if (upper == 0)
		return -1;

	while (lower < upper) {
		middle = (lower + upper) >> 1;
	  
		period = &g_array_index (attendee->busy_periods,
					 EMeetingTimeSelectorPeriod, middle);

		cmp = g_date_compare (&tmp_date, &period->start.date);
	  
		if (cmp == 0)
			break;
		else if (cmp < 0)
			upper = middle;
		else
			lower = middle + 1;
	}

	/* There may be several busy periods on the same day so we step
	   backwards to the first one. */
	if (cmp == 0) {
		while (middle > 0) {
			period = &g_array_index (attendee->busy_periods,
						 EMeetingTimeSelectorPeriod, middle - 1);
			if (g_date_compare (&tmp_date, &period->start.date) != 0)
				break;
			middle--;
		}
	} else if (cmp > 0) {
		/* This means we couldn't find a period on the given day, and
		   the last one we looked at was before it, so if there are
		   any more periods after this one we return it. */
		middle++;
		if (attendee->busy_periods->len <= middle)
			return -1;
	}

	return middle;
}


static void
e_meeting_time_selector_on_zoomed_out_toggled (GtkWidget *menuitem,
					       EMeetingTimeSelector *mts)
{
	/* Make sure the menu pops down, which doesn't happen by default if
	   keyboard accelerators are used. */
	if (GTK_WIDGET_VISIBLE (mts->options_menu))
		gtk_menu_popdown (GTK_MENU (mts->options_menu));

	e_meeting_time_selector_set_zoomed_out (mts, GTK_CHECK_MENU_ITEM (menuitem)->active);
}


static void
e_meeting_time_selector_on_working_hours_toggled (GtkWidget *menuitem,
						  EMeetingTimeSelector *mts)
{
	/* Make sure the menu pops down, which doesn't happen by default if
	   keyboard accelerators are used. */
	if (GTK_WIDGET_VISIBLE (mts->options_menu))
		gtk_menu_popdown (GTK_MENU (mts->options_menu));

	e_meeting_time_selector_set_working_hours_only (mts, GTK_CHECK_MENU_ITEM (menuitem)->active);
}


/* This recalculates day_width, first_hour_shown and last_hour_shown. */
static void
e_meeting_time_selector_recalc_grid (EMeetingTimeSelector *mts)
{
	if (mts->working_hours_only) {
		mts->first_hour_shown = mts->day_start_hour;
		mts->last_hour_shown = mts->day_end_hour;
		if (mts->day_end_minute != 0)
			mts->last_hour_shown += 1;
	} else {
		mts->first_hour_shown = 0;
		mts->last_hour_shown = 24;
	}

	/* In the brief view we use the nearest hours divisible by 3. */
	if (mts->zoomed_out) {
		mts->first_hour_shown -= mts->first_hour_shown % 3;
		mts->last_hour_shown += 2;
		mts->last_hour_shown -= mts->last_hour_shown % 3;
	}

	mts->day_width = mts->col_width	* (mts->last_hour_shown - mts->first_hour_shown);
	if (mts->zoomed_out)
		mts->day_width /= 3;

	/* Add one pixel for the extra vertical grid line. */
	mts->day_width++;

	gnome_canvas_set_scroll_region (GNOME_CANVAS (mts->display_top),
					0, 0,
					mts->day_width * E_MEETING_TIME_SELECTOR_DAYS_SHOWN,
					mts->row_height * 3);
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);

	e_meeting_time_selector_recalc_date_format (mts);
	mts->meeting_positions_valid = FALSE;
}


/* This saves the first visible time in the given EMeetingTimeSelectorTime. */
static void
e_meeting_time_selector_save_position (EMeetingTimeSelector *mts,
				       EMeetingTimeSelectorTime *mtstime)
{
	gint scroll_x, scroll_y;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (mts->display_main),
					 &scroll_x, &scroll_y);
	e_meeting_time_selector_calculate_time (mts, scroll_x, mtstime);
}


/* This restores a saved position. */
static void
e_meeting_time_selector_restore_position (EMeetingTimeSelector *mts,
					  EMeetingTimeSelectorTime *mtstime)
{
	gint scroll_x, scroll_y, new_scroll_x;

	new_scroll_x = e_meeting_time_selector_calculate_time_position (mts,
								      mtstime);
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (mts->display_main),
					 &scroll_x, &scroll_y);
	gnome_canvas_scroll_to (GNOME_CANVAS (mts->display_main),
				new_scroll_x, scroll_y);
}


/* This returns the x pixel coords of the meeting time in the entire scroll
   region. It recalculates them if they have been marked as invalid.
   If it returns FALSE then no meeting time is set or the meeting time is
   not visible in the current scroll area. */
gboolean
e_meeting_time_selector_get_meeting_time_positions (EMeetingTimeSelector *mts,
						    gint *start_x,
						    gint *end_x)
{
	if (mts->meeting_positions_valid) {
		if (mts->meeting_positions_in_scroll_area) {
			*start_x = mts->meeting_start_x;
			*end_x = mts->meeting_end_x;
			return TRUE;
		} else {
			return FALSE;
		}
	}

	mts->meeting_positions_valid = TRUE;

	/* Check if the days aren't in our current range. */
	if (g_date_compare (&mts->meeting_start_time.date, &mts->last_date_shown) > 0
	    || g_date_compare (&mts->meeting_end_time.date, &mts->first_date_shown) < 0) {
		mts->meeting_positions_in_scroll_area = FALSE;
		return FALSE;
	}

	mts->meeting_positions_in_scroll_area = TRUE;
	*start_x = mts->meeting_start_x = e_meeting_time_selector_calculate_time_position (mts, &mts->meeting_start_time);
	*end_x = mts->meeting_end_x = e_meeting_time_selector_calculate_time_position (mts, &mts->meeting_end_time);

	return TRUE;
}


/* This recalculates the date format to used, by computing the width of the
   longest date strings in the widget's font and seeing if they fit. */
static void
e_meeting_time_selector_recalc_date_format (EMeetingTimeSelector *mts)
{
	GDate date;
	gint max_date_width, base_width, max_weekday_width, max_month_width;
	gint weekday, month;
	gchar buffer[128];
	GdkFont *font;

	font = GTK_WIDGET (mts)->style->font;

	/* Check if we will be able to display the full date strings. */
	mts->date_format = E_MEETING_TIME_SELECTOR_DATE_SHORT;
	g_date_clear (&date, 1);
	g_date_set_dmy (&date, 20, 1, 2000);

	/* Calculate the maximum date width we can fit into the display. */
	max_date_width = mts->day_width - 2;

	/* First compute the width of the date string without the day or
	   month names. */
	g_date_strftime (buffer, 128, ",  %d, %Y", &date);
	base_width = gdk_string_width (font, buffer);

	/* If that doesn't fit just return. We have to use the short format.
	   If that doesn't fit it will just be clipped. */
	if (gdk_string_width (font, buffer) > max_date_width)
		return;

	/* Now find the biggest weekday name. We start on any day and just
	   go through seven days. */
	max_weekday_width = 0;
	for (weekday = 1; weekday <= 7; weekday++) {
		g_date_strftime (buffer, 128, "%A", &date);
		max_weekday_width = MAX (max_weekday_width, 
					 gdk_string_width (font, buffer));
		g_date_add_days (&date, 1);
	}

	/* Now find the biggest month name. */
	max_month_width = 0;
	for (month = 1; month <= 12; month++) {
		g_date_set_month (&date, month);
		g_date_strftime (buffer, 128, "%B", &date);
		max_month_width = MAX (max_month_width, 
				       gdk_string_width (font, buffer));
	}

	/* See if we can use the full date. */
	if (base_width + max_month_width + max_weekday_width <= max_date_width) {
		mts->date_format = E_MEETING_TIME_SELECTOR_DATE_FULL;
		return;
	}

	/* Now try it with abbreviated weekday names. */
	g_date_strftime (buffer, 128, " %x", &date);
	base_width = gdk_string_width (font, buffer);

	max_weekday_width = 0;
	for (weekday = 1; weekday <= 7; weekday++) {
		g_date_strftime (buffer, 128, "%a", &date);
		max_weekday_width = MAX (max_weekday_width, 
					 gdk_string_width (font, buffer));
		g_date_add_days (&date, 1);
	}

	if (base_width + max_weekday_width <= max_date_width)
		mts->date_format = E_MEETING_TIME_SELECTOR_DATE_ABBREVIATED_DAY;
}


/* Turn off the background of the canvas windows. This reduces flicker
   considerably when scrolling. (Why isn't it in GnomeCanvas?). */
static void
e_meeting_time_selector_on_canvas_realized (GtkWidget *widget,
					    EMeetingTimeSelector *mts)
{
	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window,
				    NULL, FALSE);
}


/* This is called when the meeting start time GnomeDateEdit is changed,
   either via the "date_changed". "time_changed" or "activate" signals on one
   of the GtkEntry widgets. So don't use the widget parameter since it may be
   one of the child GtkEntry widgets. */
static void
e_meeting_time_selector_on_start_time_changed (GtkWidget *widget,
					       EMeetingTimeSelector *mts)
{
	gint duration_days, duration_hours, duration_minutes;
	EMeetingTimeSelectorTime mtstime;
	time_t newtime;
	struct tm *newtime_tm;

	newtime = gnome_date_edit_get_date (GNOME_DATE_EDIT (mts->start_date_edit));
	newtime_tm = localtime (&newtime);
	g_date_clear (&mtstime.date, 1);
	g_date_set_time (&mtstime.date, newtime);
	mtstime.hour = newtime_tm->tm_hour;
	mtstime.minute = newtime_tm->tm_min;

	/* If the time hasn't changed, just return. */
	if (e_meeting_time_selector_compare_times (&mtstime, &mts->meeting_start_time) == 0)
		return;

	/* Calculate the current meeting duration. */
	e_meeting_time_selector_calculate_time_difference (&mts->meeting_start_time, &mts->meeting_end_time, &duration_days, &duration_hours, &duration_minutes);

	/* Set the new start time. */
	mts->meeting_start_time = mtstime;

	/* Update the end time so the meeting duration stays the same. */
	mts->meeting_end_time = mts->meeting_start_time;
	e_meeting_time_selector_adjust_time (&mts->meeting_end_time, duration_days, duration_hours, duration_minutes);
	e_meeting_time_selector_update_end_date_edit (mts);

	mts->meeting_positions_valid = FALSE;
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
}


/* This is called when the meeting end time GnomeDateEdit is changed,
   either via the "date_changed", "time_changed" or "activate" signals on one
   of the GtkEntry widgets. So don't use the widget parameter since it may be
   one of the child GtkEntry widgets. */
static void
e_meeting_time_selector_on_end_time_changed (GtkWidget *widget,
					     EMeetingTimeSelector *mts)
{
	EMeetingTimeSelectorTime mtstime;
	time_t newtime;
	struct tm *newtime_tm;

	newtime = gnome_date_edit_get_date (GNOME_DATE_EDIT (mts->end_date_edit));
	newtime_tm = localtime (&newtime);
	g_date_clear (&mtstime.date, 1);
	g_date_set_time (&mtstime.date, newtime);
	mtstime.hour = newtime_tm->tm_hour;
	mtstime.minute = newtime_tm->tm_min;

	/* If the time hasn't changed, just return. */
	if (e_meeting_time_selector_compare_times (&mtstime, &mts->meeting_end_time) == 0)
		return;

	/* Set the new end time. */
	mts->meeting_end_time = mtstime;

	/* If the start time is after the end time, set it to the same time. */
	if (e_meeting_time_selector_compare_times (&mtstime, &mts->meeting_start_time) < 0) {
		/* We set it first, before updating the widget, so the signal
		   handler will just return. */
		mts->meeting_start_time = mtstime;
		e_meeting_time_selector_update_start_date_edit (mts);
	}

	mts->meeting_positions_valid = FALSE;
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
}


/* This updates the ranges shown in the GnomeDateEdit popup menus, according
   to working_hours_only etc. */
static void
e_meeting_time_selector_update_date_popup_menus (EMeetingTimeSelector *mts)
{
	GnomeDateEdit *start_edit, *end_edit;
	gint low_hour, high_hour;

	start_edit = GNOME_DATE_EDIT (mts->start_date_edit);
	end_edit = GNOME_DATE_EDIT (mts->end_date_edit);

	if (mts->working_hours_only) {
		low_hour = mts->day_start_hour;
		high_hour = mts->day_end_hour;
	} else {
		low_hour = 0;
		high_hour = 23;
	}

	gnome_date_edit_set_popup_range (start_edit, low_hour, high_hour);
	gnome_date_edit_set_popup_range (end_edit, low_hour, high_hour);
}


static void
e_meeting_time_selector_on_canvas_size_allocate (GtkWidget *widget,
						 GtkAllocation *allocation,
						 EMeetingTimeSelector *mts)
{
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);

	e_meeting_time_selector_ensure_meeting_time_shown (mts);
}


static void
e_meeting_time_selector_on_attendees_list_size_allocate (GtkWidget *widget,
							 GtkAllocation *allocation,
							 EMeetingTimeSelector *mts)
{
	e_meeting_time_selector_update_attendees_list_scroll_region (mts);
	e_meeting_time_selector_update_attendees_list_positions (mts);
}


/* This updates the list canvas scroll region according to the number of
   attendees. If the total height needed is less than the height of the canvas,
   we must use the height of the canvas, or it causes problems. */
static void
e_meeting_time_selector_update_attendees_list_scroll_region (EMeetingTimeSelector *mts)
{
	gint height, canvas_width, canvas_height;

	height = mts->row_height * mts->attendees->len;
	canvas_width = GTK_WIDGET (mts->attendees_list)->allocation.width;
	canvas_height = GTK_WIDGET (mts->attendees_list)->allocation.height;

	height = MAX (height,  canvas_height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (mts->attendees_list),
					0, 0,
					canvas_width,
					height);
}


/* This updates the canvas scroll regions according to the number of attendees.
   If the total height needed is less than the height of the canvas, we must
   use the height of the canvas, or it causes problems. */
static void
e_meeting_time_selector_update_main_canvas_scroll_region (EMeetingTimeSelector *mts)
{
	gint height, canvas_height, list_width;

	height = mts->row_height * (mts->attendees->len + 1);
	canvas_height = GTK_WIDGET (mts->display_main)->allocation.height;
	list_width = GTK_WIDGET (mts->attendees_list)->allocation.width;

	height = MAX (height,  canvas_height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (mts->attendees_list),
					0, 0,
					list_width,
					height);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (mts->display_main),
					0, 0,
					mts->day_width * E_MEETING_TIME_SELECTOR_DAYS_SHOWN,
					height);
}


/* This changes the meeting time based on the given x coordinate and whether
   we are dragging the start or end bar. It returns the new position, which
   will be swapped if the start bar is dragged past the end bar or vice versa.
   It make sure the meeting time is never dragged outside the visible canvas
   area. */
void
e_meeting_time_selector_drag_meeting_time (EMeetingTimeSelector *mts,
					   gint x)
{
	EMeetingTimeSelectorTime first_time, last_time, drag_time, *time_to_set;
	gint scroll_x, scroll_y, canvas_width;
	gboolean set_both_times = FALSE;

	/* Get the x coords of visible part of the canvas. */
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (mts->display_main),
					 &scroll_x, &scroll_y);
	canvas_width = mts->display_main->allocation.width;

	/* Save the x coordinate for the timeout handler. */
	mts->last_drag_x = (x < scroll_x) ? x - scroll_x
		: x - scroll_x - canvas_width + 1;

	/* Check if the mouse is off the edge of the canvas. */
	if (x < scroll_x || x > scroll_x + canvas_width) {
		/* If we haven't added a timeout function, add one. */
		if (mts->auto_scroll_timeout_id == 0) {
			mts->auto_scroll_timeout_id = g_timeout_add (60, e_meeting_time_selector_timeout_handler, mts);
			mts->scroll_count = 0;

			/* Call the handler to start scrolling now. */
			e_meeting_time_selector_timeout_handler (mts);
			return;
		}
	} else {
		e_meeting_time_selector_remove_timeout (mts);
	}

	/* Calculate the minimum & maximum times we can use, based on the
	   scroll offsets and whether zoomed_out is set. */
	e_meeting_time_selector_calculate_time (mts, scroll_x, &first_time);
	e_meeting_time_selector_calculate_time (mts, scroll_x + canvas_width - 1,
						&last_time);
	if (mts->zoomed_out) {
		if (first_time.minute > 30)
			first_time.hour++;
		first_time.minute = 0;
		last_time.minute = 0;
	} else {
		first_time.minute += 15;
		first_time.minute -= first_time.minute % 30;
		last_time.minute -= last_time.minute % 30;
	}
	e_meeting_time_selector_fix_time_overflows (&first_time);
	e_meeting_time_selector_fix_time_overflows (&last_time);

	/* Calculate the time from x coordinate. */
	e_meeting_time_selector_calculate_time (mts, x, &drag_time);

	/* Calculate the nearest half-hour or hour, depending on whether
	   zoomed_out is set. */
	if (mts->zoomed_out) {
		if (drag_time.minute > 30)
			drag_time.hour++;
		drag_time.minute = 0;
	} else {
		drag_time.minute += 15;
		drag_time.minute -= drag_time.minute % 30;
	}
	e_meeting_time_selector_fix_time_overflows (&drag_time);

	/* Now make sure we are between first_time & last_time. */
	if (e_meeting_time_selector_compare_times (&drag_time, &first_time) < 0)
		drag_time = first_time;
	if (e_meeting_time_selector_compare_times (&drag_time, &last_time) > 0)
		drag_time = last_time;

	/* Set the meeting start or end time to drag_time. */
	if (mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
		time_to_set = &mts->meeting_start_time;
	else
		time_to_set = &mts->meeting_end_time;

	/* If the time is unchanged, just return. */
	if (e_meeting_time_selector_compare_times (time_to_set, &drag_time) == 0)
		return;

	*time_to_set = drag_time;

	/* Check if the start time and end time need to be switched. */
	if (e_meeting_time_selector_compare_times (&mts->meeting_start_time,
						 &mts->meeting_end_time) > 0) {
		drag_time = mts->meeting_start_time;
		mts->meeting_start_time = mts->meeting_end_time;
		mts->meeting_end_time = drag_time;

		if (mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
			mts->dragging_position = E_MEETING_TIME_SELECTOR_POS_END;
		else
			mts->dragging_position = E_MEETING_TIME_SELECTOR_POS_START;

		set_both_times = TRUE;
	}

	/* Mark the calculated positions as invalid. */
	mts->meeting_positions_valid = FALSE;

	/* Redraw the canvases. */
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);

	/* Set the times in the GnomeDateEdit widgets. */
	if (set_both_times
	    || mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
		e_meeting_time_selector_update_start_date_edit (mts);

	if (set_both_times
	    || mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_END)
		e_meeting_time_selector_update_end_date_edit (mts);
}


/* This is the timeout function which handles auto-scrolling when the user is
   dragging one of the meeting time vertical bars outside the left or right
   edge of the canvas. */
static gboolean
e_meeting_time_selector_timeout_handler (gpointer data)
{
	EMeetingTimeSelector *mts;
	EMeetingTimeSelectorTime drag_time, *time_to_set;
	gint scroll_x, max_scroll_x, scroll_y, canvas_width;
	gint scroll_speed, scroll_offset;
	gboolean set_both_times = FALSE;

	mts = E_MEETING_TIME_SELECTOR (data);

	GDK_THREADS_ENTER ();

	/* Return if we don't need to scroll yet. */
	if (mts->scroll_count-- > 0) {
		GDK_THREADS_LEAVE ();
		return TRUE;
	}

	/* Get the x coords of visible part of the canvas. */
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (mts->display_main),
					 &scroll_x, &scroll_y);
	canvas_width = mts->display_main->allocation.width;

	/* Calculate the scroll delay, between 0 and MAX_SCROLL_SPEED. */
	scroll_speed = abs (mts->last_drag_x / E_MEETING_TIME_SELECTOR_SCROLL_INCREMENT_WIDTH);
	scroll_speed = MIN (scroll_speed,
			    E_MEETING_TIME_SELECTOR_MAX_SCROLL_SPEED);

	/* Reset the scroll count. */
	mts->scroll_count = E_MEETING_TIME_SELECTOR_MAX_SCROLL_SPEED - scroll_speed;

	/* Calculate how much we need to scroll. */
	if (mts->last_drag_x >= 0)
		scroll_offset = mts->col_width;
	else
		scroll_offset = -mts->col_width;

	scroll_x += scroll_offset;
	max_scroll_x = (mts->day_width * E_MEETING_TIME_SELECTOR_DAYS_SHOWN)
		- canvas_width;
	scroll_x = CLAMP (scroll_x, 0, max_scroll_x);

	/* Calculate the minimum or maximum visible time in the canvas, which
	   we will now set the dragged time to. */
	if (scroll_offset > 0) {
		e_meeting_time_selector_calculate_time (mts,
						      scroll_x + canvas_width - 1,
						      &drag_time);
		if (mts->zoomed_out) {
			drag_time.minute = 0;
		} else {
			drag_time.minute -= drag_time.minute % 30;
		}
	} else {
		e_meeting_time_selector_calculate_time (mts, scroll_x,
							&drag_time);
		if (mts->zoomed_out) {
			if (drag_time.minute > 30)
				drag_time.hour++;
			drag_time.minute = 0;
		} else {
			drag_time.minute += 15;
			drag_time.minute -= drag_time.minute % 30;
		}
	}
	e_meeting_time_selector_fix_time_overflows (&drag_time);

	/* Set the meeting start or end time to drag_time. */
	if (mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
		time_to_set = &mts->meeting_start_time;
	else
		time_to_set = &mts->meeting_end_time;

	/* If the time is unchanged, just return. */
	if (e_meeting_time_selector_compare_times (time_to_set, &drag_time) == 0) {
		GDK_THREADS_LEAVE ();
		return TRUE;
	}

	*time_to_set = drag_time;

	/* Check if the start time and end time need to be switched. */
	if (e_meeting_time_selector_compare_times (&mts->meeting_start_time, &mts->meeting_end_time) > 0) {
		drag_time = mts->meeting_start_time;
		mts->meeting_start_time = mts->meeting_end_time;
		mts->meeting_end_time = drag_time;

		if (mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
			mts->dragging_position = E_MEETING_TIME_SELECTOR_POS_END;
		else
			mts->dragging_position = E_MEETING_TIME_SELECTOR_POS_START;

		set_both_times = TRUE;
	}

	/* Mark the calculated positions as invalid. */
	mts->meeting_positions_valid = FALSE;

	/* Set the times in the GnomeDateEdit widgets. */
	if (set_both_times
	    || mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
		e_meeting_time_selector_update_start_date_edit (mts);

	if (set_both_times
	    || mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_END)
		e_meeting_time_selector_update_end_date_edit (mts);

	/* Redraw the canvases. We freeze and thaw the layouts so that they
	   get redrawn completely. Otherwise the pixels get scrolled left or
	   right which is not good for us (since our vertical bars have been
	   moved) and causes flicker. */
	gtk_layout_freeze (GTK_LAYOUT (mts->display_main));
	gtk_layout_freeze (GTK_LAYOUT (mts->display_top));
	gnome_canvas_scroll_to (GNOME_CANVAS (mts->display_main),
				scroll_x, scroll_y);
	gnome_canvas_scroll_to (GNOME_CANVAS (mts->display_top),
				scroll_x, scroll_y);
	gtk_layout_thaw (GTK_LAYOUT (mts->display_main));
	gtk_layout_thaw (GTK_LAYOUT (mts->display_top));

	GDK_THREADS_LEAVE ();
	return TRUE;
}


/* This removes our auto-scroll timeout function, if we have one installed. */
void
e_meeting_time_selector_remove_timeout (EMeetingTimeSelector *mts)
{
	if (mts->auto_scroll_timeout_id) {
		g_source_remove (mts->auto_scroll_timeout_id);
		mts->auto_scroll_timeout_id = 0;
	}
}


/* This updates the GnomeDateEdit widget displaying the meeting start time. */
static void
e_meeting_time_selector_update_start_date_edit (EMeetingTimeSelector *mts)
{
	struct tm start_tm;
	time_t start_time_t;

	g_date_to_struct_tm (&mts->meeting_start_time.date, &start_tm);
	start_tm.tm_hour = mts->meeting_start_time.hour;
	start_tm.tm_min = mts->meeting_start_time.minute;
	start_time_t = mktime (&start_tm);
	gnome_date_edit_set_time (GNOME_DATE_EDIT (mts->start_date_edit),
				  start_time_t);
}


/* This updates the GnomeDateEdit widget displaying the meeting end time. */
static void
e_meeting_time_selector_update_end_date_edit (EMeetingTimeSelector *mts)
{
	struct tm end_tm;
	time_t end_time_t;

	g_date_to_struct_tm (&mts->meeting_end_time.date, &end_tm);
	end_tm.tm_hour = mts->meeting_end_time.hour;
	end_tm.tm_min = mts->meeting_end_time.minute;
	end_time_t = mktime (&end_tm);
	gnome_date_edit_set_time (GNOME_DATE_EDIT (mts->end_date_edit),
				  end_time_t);
}


/* This ensures that the meeting time is shown on screen, by scrolling the
   canvas and possibly by changing the range of dates shown in the canvas. */
static void
e_meeting_time_selector_ensure_meeting_time_shown (EMeetingTimeSelector *mts)
{
	gint start_x, end_x, scroll_x, scroll_y, canvas_width;
	gint new_scroll_x;
	gboolean fits_in_canvas;

	/* Check if we need to change the range of dates shown. */
	if (g_date_compare (&mts->meeting_start_time.date,
			    &mts->first_date_shown) < 0
	    || g_date_compare (&mts->meeting_end_time.date,
			       &mts->last_date_shown) > 0) {
		e_meeting_time_selector_update_dates_shown (mts);
		gtk_widget_queue_draw (mts->display_top);
		gtk_widget_queue_draw (mts->display_main);
	}

	/* If all of the meeting time is visible, just return. */
	e_meeting_time_selector_get_meeting_time_positions (mts, &start_x,
							    &end_x);
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (mts->display_main),
					 &scroll_x, &scroll_y);
	canvas_width = mts->display_main->allocation.width;
	if (start_x > scroll_x && end_x <= scroll_x + canvas_width)
		return;

	fits_in_canvas = end_x - start_x < canvas_width ? TRUE : FALSE;

	/* If the meeting is not entirely visible, either center it if it is
	   smaller than the canvas, or show the start of it if it is big. */
	if (fits_in_canvas) {
		new_scroll_x = (start_x + end_x - canvas_width) / 2;
	} else {
		new_scroll_x = start_x;
	}
	gnome_canvas_scroll_to (GNOME_CANVAS (mts->display_main),
				new_scroll_x, scroll_y);
}


/* This updates the range of dates shown in the canvas, to make sure that the
   currently selected meeting time is in the range. */
static void
e_meeting_time_selector_update_dates_shown (EMeetingTimeSelector *mts)
{
	mts->first_date_shown = mts->meeting_start_time.date;
	g_date_subtract_days (&mts->first_date_shown, 60);

	mts->last_date_shown = mts->first_date_shown;
	g_date_add_days (&mts->last_date_shown, E_MEETING_TIME_SELECTOR_DAYS_SHOWN - 1);
}


/* This checks if the time's hour is over 24 or its minute is over 60 and if
   so it updates the day/hour appropriately. Note that hours and minutes are
   stored in guint8's so they can't overflow by much. */
void
e_meeting_time_selector_fix_time_overflows (EMeetingTimeSelectorTime *mtstime)
{
	gint hours_to_add, days_to_add;

	hours_to_add = mtstime->minute / 60;
	if (hours_to_add > 0) {
		mtstime->minute -= hours_to_add * 60;
		mtstime->hour += hours_to_add;
	}

	days_to_add = mtstime->hour / 24;
	if (days_to_add > 0) {
		mtstime->hour -= days_to_add * 24;
		g_date_add_days (&mtstime->date, days_to_add);
	}
}


static void
e_meeting_time_selector_update_attendees_list_positions (EMeetingTimeSelector *mts)
{
	EMeetingTimeSelectorAttendee *attendee;
	gint list_width, item_width;
	gint row;
	GdkFont *font;

	list_width = GTK_WIDGET (mts->attendees_list)->allocation.width;
	item_width = MAX (1, list_width - E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH - E_MEETING_TIME_SELECTOR_TEXT_X_PAD * 2);
	font = GTK_WIDGET (mts)->style->font;
	for (row = 0; row < mts->attendees->len; row++) {
		attendee = &g_array_index (mts->attendees,
					   EMeetingTimeSelectorAttendee, row);

		gnome_canvas_item_set
			(attendee->text_item,
			 "font_gdk", font,
			 "y", (gdouble) (row * mts->row_height + 1
					 + E_MEETING_TIME_SELECTOR_TEXT_Y_PAD),
			 "clip_width", (gdouble) item_width,
			 "clip_height", (gdouble) (font->ascent
						   + font->descent),
			 NULL);

		gnome_canvas_item_show (attendee->text_item);
	}

}


/*
 * CONVERSION ROUTINES - functions to convert between different coordinate
 *			 spaces and dates.
 */

/* This takes an x pixel coordinate within the entire canvas scroll region and
   returns the date in which it falls. If day_position is not NULL it also
   returns the x coordinate within the date, relative to the visible part of
   the canvas. It is used when painting the days in the item_draw function.
   Note that it must handle negative x coordinates in case we are dragging off
   the edge of the canvas. */
void
e_meeting_time_selector_calculate_day_and_position (EMeetingTimeSelector *mts,
						    gint x,
						    GDate *date,
						    gint *day_position)
{
	gint days_from_first_shown;

	*date = mts->first_date_shown;

	if (x >= 0) {
		days_from_first_shown = x / mts->day_width;
		g_date_add_days (date, days_from_first_shown);
		if (day_position)
			*day_position = - x % mts->day_width;
	} else {
		days_from_first_shown = -x / mts->day_width + 1;
		g_date_subtract_days (date, days_from_first_shown);
		if (day_position)
			*day_position = -mts->day_width - x % mts->day_width;
	}
}


/* This takes an x pixel coordinate within a day, and converts it to hours
   and minutes, depending on working_hours_only and zoomed_out. */
void
e_meeting_time_selector_convert_day_position_to_hours_and_mins (EMeetingTimeSelector *mts, gint day_position, guint8 *hours, guint8 *minutes)
{
	if (mts->zoomed_out)
		day_position *= 3;

	/* Calculate the hours & minutes from the first displayed. */
	*hours = day_position / mts->col_width;
	*minutes = (day_position % mts->col_width) * 60 / mts->col_width;

	/* Now add on the first hour shown. */
	*hours += mts->first_hour_shown;
}


/* This takes an x pixel coordinate within the entire canvas scroll region and
   returns the time in which it falls. Note that it won't be extremely
   accurate since hours may only be a few pixels wide in the display.
   With zoomed_out set each pixel may represent 5 minutes or more, depending
   on how small the font is. */
void
e_meeting_time_selector_calculate_time (EMeetingTimeSelector *mts,
					gint x,
					EMeetingTimeSelectorTime *time)
{
	gint day_position;

	/* First get the day and the x position within the day. */
	e_meeting_time_selector_calculate_day_and_position (mts, x, &time->date,
							    NULL);

	/* Now convert the day_position into an hour and minute. */
	if (x >= 0)
		day_position = x % mts->day_width;
	else
		day_position = mts->day_width + x % mts->day_width;

	e_meeting_time_selector_convert_day_position_to_hours_and_mins (mts, day_position, &time->hour, &time->minute);
}


/* This takes a EMeetingTimeSelectorTime and calculates the x pixel coordinate
   within the entire canvas scroll region. It is used to draw the selected
   meeting time and all the busy periods. */
gint
e_meeting_time_selector_calculate_time_position (EMeetingTimeSelector *mts,
						 EMeetingTimeSelectorTime *mtstime)
{
	gint x, date_offset, day_offset;

	/* Calculate the number of days since the first date shown in the
	   entire canvas scroll region. */
	date_offset = g_date_julian (&mtstime->date) - g_date_julian (&mts->first_date_shown);

	/* Calculate the x pixel coordinate of the start of the day. */
	x = date_offset * mts->day_width;

	/* Add on the hours and minutes, depending on whether zoomed_out and
	   working_hours_only are set. */
	day_offset = (mtstime->hour - mts->first_hour_shown) * 60
		+ mtstime->minute;
	/* The day width includes an extra vertical grid line so subtract 1. */
	day_offset *= (mts->day_width - 1);
	day_offset /= (mts->last_hour_shown - mts->first_hour_shown) * 60;

	/* Clamp the day_offset in case the time isn't actually visible. */
	x += CLAMP (day_offset, 0, mts->day_width);

	return x;
}


static gboolean
e_meeting_time_selector_on_text_item_event (GnomeCanvasItem *item,
					    GdkEvent *event,
					    EMeetingTimeSelector *mts)
{
	EMeetingTimeSelectorAttendee *attendee;
	gint row, min;
	ETextEventProcessor *event_processor = NULL;
	ETextEventProcessorCommand command;
	GtkAdjustment *adj;
	gchar *text;
	gboolean empty = FALSE;

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event && event->key.keyval == GDK_Return) {
			row = e_meeting_time_selector_find_row_from_text_item (mts, item);
			g_return_val_if_fail (row != -1, FALSE);

			if (row == mts->attendees->len - 1)
				row = e_meeting_time_selector_attendee_add (mts, "", NULL);
			else
				row++;

			/* Make sure the item is visible. */
			adj = GTK_LAYOUT (mts->display_main)->vadjustment;
			min = ((row + 1) * mts->row_height) - adj->page_size;
			if (adj->value < min) {
				adj->value = min;
				gtk_adjustment_value_changed (adj);
			}

			attendee = &g_array_index (mts->attendees, EMeetingTimeSelectorAttendee, row);
			e_canvas_item_grab_focus (attendee->text_item);

			/* Try to move the cursor to the end of the text. */
			gtk_object_get (GTK_OBJECT (attendee->text_item),
					"event_processor", &event_processor,
					NULL);
			if (event_processor) {
				command.action = E_TEP_MOVE;
				command.position = E_TEP_END_OF_BUFFER;
				gtk_signal_emit_by_name (GTK_OBJECT (event_processor),
							 "command", &command);
			}

			/* Stop the signal last or we will also stop any
			   other events getting to the EText item. */
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");
			return TRUE;
		}
		break;
	case GDK_FOCUS_CHANGE:
		if (!event->focus_change.in) {
			gtk_object_get (GTK_OBJECT (item),
					"text", &text,
					NULL);
			if (!text || !text[0])
				empty = TRUE;
			g_free (text);

			if (empty) {
				row = e_meeting_time_selector_find_row_from_text_item (mts, item);
				g_return_val_if_fail (row != -1, FALSE);
				e_meeting_time_selector_attendee_remove (mts,
									 row);
			}
		}
		break;
	default:
		break;
	}

	return FALSE;
}


static gint
e_meeting_time_selector_find_row_from_text_item (EMeetingTimeSelector *mts,
						 GnomeCanvasItem *item)
{
	EMeetingTimeSelectorAttendee *attendee;
	gint row;

	for (row = 0; row < mts->attendees->len; row++) {
		attendee = &g_array_index (mts->attendees,
					   EMeetingTimeSelectorAttendee, row);
		if (attendee->text_item == item)
			return row;
	}

	return -1;
}

