/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Authors : 
 *  Damon Chaplin <damon@gtk.org>
 *  Rodrigo Moya <rodrigo@novell.com>
 *
 * Copyright 1999, Ximian, Inc.
 * Copyright 2004, Novell, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-meeting-time-sel.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkfixed.h>
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
#include <libgnomecanvas/gnome-canvas-widget.h>

#include <gal/widgets/e-canvas.h>
#include <gal/widgets/e-canvas-utils.h>

#include <widgets/misc/e-dateedit.h>
#include <e-util/e-gui-utils.h>

#include "calendar-component.h"
#include "calendar-config.h"
#include "e-meeting-utils.h"
#include "e-meeting-list-view.h"
#include "e-meeting-time-sel-item.h"

/* An array of hour strings for 24 hour time, "0:00" .. "23:00". */
const gchar *EMeetingTimeSelectorHours[24] = {
	"0:00", "1:00", "2:00", "3:00", "4:00", "5:00", "6:00", "7:00", 
	"8:00", "9:00", "10:00", "11:00", "12:00", "13:00", "14:00", "15:00", 
	"16:00", "17:00", "18:00", "19:00", "20:00", "21:00", "22:00", "23:00"
};

/* An array of hour strings for 12 hour time, "12:00am" .. "11:00pm". */
const gchar *EMeetingTimeSelectorHours12[24] = {
	"12:00am", "1:00am", "2:00am", "3:00am", "4:00am", "5:00am", "6:00am", 
	"7:00am", "8:00am", "9:00am", "10:00am", "11:00am", "12:00pm", 
	"1:00pm", "2:00pm", "3:00pm", "4:00pm", "5:00pm", "6:00pm", "7:00pm",
	"8:00pm", "9:00pm", "10:00pm", "11:00pm"
};

/* The number of days shown in the entire canvas. */
#define E_MEETING_TIME_SELECTOR_DAYS_SHOWN		365
#define E_MEETING_TIME_SELECTOR_DAYS_START_BEFORE	 60
#define E_MEETING_TIME_SELECTOR_FB_DAYS_BEFORE            7
#define E_MEETING_TIME_SELECTOR_FB_DAYS_AFTER            28

/* This is the number of pixels between the mouse has to move before the
   scroll speed is incremented. */
#define E_MEETING_TIME_SELECTOR_SCROLL_INCREMENT_WIDTH	10

/* This is the maximum scrolling speed. */
#define E_MEETING_TIME_SELECTOR_MAX_SCROLL_SPEED	4

/* Signals */
enum {
	CHANGED,
	LAST_SIGNAL
};


static gint mts_signals [LAST_SIGNAL] = { 0 };

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
static void e_meeting_time_selector_draw_shadow (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_hadjustment_changed (GtkAdjustment *adjustment,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_vadjustment_changed (GtkAdjustment *adjustment,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_canvas_realized (GtkWidget *widget,
							EMeetingTimeSelector *mts);

static void e_meeting_time_selector_on_options_button_clicked (GtkWidget *button,
							       EMeetingTimeSelector *mts);
static void e_meeting_time_selector_options_menu_position_callback (GtkMenu *menu,
								    gint *x,
								    gint *y,
								    gboolean *push_in,
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
								     gboolean *push_in,
								     gpointer user_data);
static void e_meeting_time_selector_on_autopick_option_toggled (GtkWidget *button,
								EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_prev_button_clicked (GtkWidget *button,
							    EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_next_button_clicked (GtkWidget *button,
							    EMeetingTimeSelector *mts);
static void e_meeting_time_selector_autopick (EMeetingTimeSelector *mts,
					      gboolean forward);
static void e_meeting_time_selector_calculate_time_difference (EMeetingTime*start,
							       EMeetingTime*end,
							       gint *days,
							       gint *hours,
							       gint *minutes);
static void e_meeting_time_selector_find_nearest_interval (EMeetingTimeSelector *mts,
							   EMeetingTime*start_time,
							   EMeetingTime*end_time,
							   gint days, gint hours, gint mins);
static void e_meeting_time_selector_find_nearest_interval_backward (EMeetingTimeSelector *mts,
								    EMeetingTime *start_time,
								    EMeetingTime *end_time,
								    gint days, gint hours, gint mins);
static void e_meeting_time_selector_adjust_time (EMeetingTime*mtstime,
						 gint days, gint hours, gint minutes);
static EMeetingFreeBusyPeriod* e_meeting_time_selector_find_time_clash (EMeetingTimeSelector *mts,
									EMeetingAttendee *attendee,
									EMeetingTime *start_time,
									EMeetingTime *end_time);


static void e_meeting_time_selector_recalc_grid (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_recalc_date_format (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_save_position (EMeetingTimeSelector *mts,
						   EMeetingTime *mtstime);
static void e_meeting_time_selector_restore_position (EMeetingTimeSelector *mts,
						      EMeetingTime*mtstime);
static void e_meeting_time_selector_on_start_time_changed (GtkWidget *widget,
							   EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_end_time_changed (GtkWidget *widget,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_date_popup_menus (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_canvas_size_allocate (GtkWidget *widget,
							     GtkAllocation *allocation,
							     EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_main_canvas_scroll_region (EMeetingTimeSelector *mts);
static gboolean e_meeting_time_selector_timeout_handler (gpointer data);
static void e_meeting_time_selector_update_start_date_edit (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_end_date_edit (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_ensure_meeting_time_shown (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_update_dates_shown (EMeetingTimeSelector *mts);

static void row_inserted_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void row_changed_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void row_deleted_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data);

static void free_busy_template_changed_cb (GConfClient *client, guint cnxn_id,
					   GConfEntry *entry, gpointer user_data);

G_DEFINE_TYPE (EMeetingTimeSelector, e_meeting_time_selector, GTK_TYPE_TABLE);

static void
e_meeting_time_selector_class_init (EMeetingTimeSelectorClass * klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	mts_signals [CHANGED] = 
		gtk_signal_new ("changed", GTK_RUN_FIRST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (EMeetingTimeSelectorClass, 
						   changed),
				gtk_signal_default_marshaller,
				GTK_TYPE_NONE, 0);

	object_class->destroy = e_meeting_time_selector_destroy;

	widget_class->realize      = e_meeting_time_selector_realize;
	widget_class->unrealize    = e_meeting_time_selector_unrealize;
	widget_class->style_set    = e_meeting_time_selector_style_set;
	widget_class->expose_event = e_meeting_time_selector_expose_event;
}


static void
e_meeting_time_selector_init (EMeetingTimeSelector * mts)
{
	/* The shadow is drawn in the border so it must be >= 2 pixels. */
	gtk_container_set_border_width (GTK_CONTAINER (mts), 2);

	mts->accel_group = gtk_accel_group_new ();

	mts->working_hours_only = TRUE;
	mts->day_start_hour = 9;
	mts->day_start_minute = 0;
	mts->day_end_hour = 18;
	mts->day_end_minute = 0;
	mts->zoomed_out = TRUE;
	mts->dragging_position = E_MEETING_TIME_SELECTOR_POS_NONE;

	mts->list_view = NULL;

	mts->fb_uri_not = 
		calendar_config_add_notification_free_busy_template ((GConfClientNotifyFunc) free_busy_template_changed_cb,
								     mts);

	mts->fb_refresh_not = 0;
}


void
e_meeting_time_selector_construct (EMeetingTimeSelector * mts, EMeetingStore *ems)
{
	char *filename;
	GtkWidget *hbox, *vbox, *separator, *label, *table, *sw;
	GtkWidget *alignment, *child_hbox, *arrow, *menuitem;
	GSList *group;
	GdkVisual *visual;
	GdkColormap *colormap;
	guint accel_key;
	time_t meeting_start_time;
	struct tm *meeting_start_tm;
	guchar stipple_bits[] = {
		0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
	};
	AtkObject *a11y_label, *a11y_date_edit;

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

	mts->row_height = 17;
	mts->col_width = 55;
	mts->day_width = 55 * 24 + 1;

	mts->auto_scroll_timeout_id = 0;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_table_attach (GTK_TABLE (mts),
			  vbox, 0, 1, 0, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (vbox);

	mts->attendees_vbox_spacer = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), mts->attendees_vbox_spacer, FALSE, FALSE, 0);
	gtk_widget_show (mts->attendees_vbox_spacer);
	
	mts->attendees_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), mts->attendees_vbox, TRUE, TRUE, 0);
	gtk_widget_show (mts->attendees_vbox);


	/* build the etable */
	filename = g_build_filename (calendar_component_peek_config_directory (calendar_component_peek ()),
				     "config", "et-header-meeting-time-sel", NULL);
	mts->model = ems;

	if (mts->model)
		g_object_ref (mts->model);

	g_signal_connect (mts->model, "row_inserted", G_CALLBACK (row_inserted_cb), mts);
	g_signal_connect (mts->model, "row_changed", G_CALLBACK (row_changed_cb), mts);
	g_signal_connect (mts->model, "row_deleted", G_CALLBACK (row_deleted_cb), mts);

	mts->list_view = e_meeting_list_view_new (mts->model);
	e_meeting_list_view_column_set_visible (mts->list_view, "Role", FALSE);
	e_meeting_list_view_column_set_visible (mts->list_view, "RSVP", FALSE);
	gtk_widget_show (GTK_WIDGET (mts->list_view));

	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_widget_set_child_visible (GTK_SCROLLED_WINDOW (sw)->vscrollbar, FALSE);
	gtk_widget_show (sw);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (mts->list_view));

#if 0
	/* FIXME: do we need sorting here */
	g_signal_connect (real_table->sort_info, "sort_info_changed", G_CALLBACK (sort_info_changed_cb), mts);
#endif

	gtk_box_pack_start (GTK_BOX (mts->attendees_vbox), GTK_WIDGET (sw), TRUE, TRUE, 6);

	/* The free/busy information */
	mts->display_top = gnome_canvas_new ();
	gtk_widget_set_usize (mts->display_top, -1, mts->row_height * 3);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (mts->display_top),
					0, 0,
					mts->day_width * E_MEETING_TIME_SELECTOR_DAYS_SHOWN,
					mts->row_height * 3);
	/* Add some horizontal padding for the shadow around the display. */
	gtk_table_attach (GTK_TABLE (mts), mts->display_top,
			  1, 4, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show (mts->display_top);
	g_signal_connect (mts->display_top, "realize",
			  G_CALLBACK (e_meeting_time_selector_on_canvas_realized), mts);

	mts->display_main = gnome_canvas_new ();
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);
	/* Add some horizontal padding for the shadow around the display. */
	gtk_table_attach (GTK_TABLE (mts), mts->display_main,
			  1, 4, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (mts->display_main);
	g_signal_connect (mts->display_main, "realize",
			  G_CALLBACK (e_meeting_time_selector_on_canvas_realized), mts);
	g_signal_connect (mts->display_main, "size_allocate",
			  G_CALLBACK (e_meeting_time_selector_on_canvas_size_allocate), mts);

	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sw), GTK_LAYOUT (mts->display_main)->vadjustment);

	mts->hscrollbar = gtk_hscrollbar_new (GTK_LAYOUT (mts->display_main)->hadjustment);
	GTK_LAYOUT (mts->display_main)->hadjustment->step_increment = mts->col_width;
	gtk_table_attach (GTK_TABLE (mts), mts->hscrollbar,
			  1, 4, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show (mts->hscrollbar);

	mts->vscrollbar = gtk_vscrollbar_new (GTK_LAYOUT (mts->display_main)->vadjustment);
	GTK_LAYOUT (mts->display_main)->vadjustment->step_increment = mts->row_height;
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

	e_meeting_time_selector_add_key_color (mts, hbox, _("Tentative"), &mts->busy_colors[E_MEETING_FREE_BUSY_TENTATIVE]);
	e_meeting_time_selector_add_key_color (mts, hbox, _("Busy"), &mts->busy_colors[E_MEETING_FREE_BUSY_BUSY]);
	e_meeting_time_selector_add_key_color (mts, hbox, _("Out of Office"), &mts->busy_colors[E_MEETING_FREE_BUSY_OUT_OF_OFFICE]);
	e_meeting_time_selector_add_key_color (mts, hbox, _("No Information"),
					       NULL);

	separator = gtk_hseparator_new ();
	gtk_table_attach (GTK_TABLE (mts), separator,
			  0, 5, 4, 5, GTK_FILL, 0, 6, 6);
	gtk_widget_show (separator);

	/* Create the Invite Others & Options buttons on the left. */
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_table_attach (GTK_TABLE (mts), hbox,
			  0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show (hbox);

	mts->add_attendees_button = e_button_new_with_stock_icon (_("Con_tacts..."), "gtk-jump-to");
	gtk_box_pack_start (GTK_BOX (hbox), mts->add_attendees_button, TRUE, TRUE, 6);
	gtk_widget_show (mts->add_attendees_button);
	g_signal_connect (mts->add_attendees_button, "clicked",
			  G_CALLBACK (e_meeting_time_selector_on_invite_others_button_clicked), mts);

	mts->options_button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), mts->options_button, TRUE, TRUE, 6);
	gtk_widget_show (mts->options_button);

	g_signal_connect (mts->options_button, "clicked",
			  G_CALLBACK (e_meeting_time_selector_on_options_button_clicked), mts);

	child_hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (mts->options_button), child_hbox);
	gtk_widget_show (child_hbox);

	label = gtk_label_new_with_mnemonic (_("O_ptions"));
	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 6);
	gtk_widget_show (label);
	gtk_widget_add_accelerator (mts->options_button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (child_hbox), arrow, FALSE, FALSE, 6);
	gtk_widget_show (arrow);

	/* Create the Options menu. */
	mts->options_menu = gtk_menu_new ();
	gtk_menu_attach_to_widget (GTK_MENU (mts->options_menu), mts->options_button,
				   e_meeting_time_selector_options_menu_detacher);

	menuitem = gtk_check_menu_item_new_with_label ("");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menuitem)->child), _("Show _only working hours"));
	gtk_menu_append (GTK_MENU (mts->options_menu), menuitem);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					mts->working_hours_only);

	g_signal_connect (menuitem, "toggled",
			  G_CALLBACK (e_meeting_time_selector_on_working_hours_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_check_menu_item_new_with_label ("");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menuitem)->child), _("Show _zoomed out"));
	gtk_menu_append (GTK_MENU (mts->options_menu), menuitem);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					mts->zoomed_out);

	g_signal_connect (menuitem, "toggled",
			  G_CALLBACK (e_meeting_time_selector_on_zoomed_out_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_menu_item_new ();
	gtk_menu_append (GTK_MENU (mts->options_menu), menuitem);
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);

	menuitem = gtk_menu_item_new_with_label ("");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menuitem)->child), _("_Update free/busy"));
	gtk_menu_append (GTK_MENU (mts->options_menu), menuitem);

	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (e_meeting_time_selector_on_update_free_busy), mts);
	gtk_widget_show (menuitem);

	/* Create the 3 AutoPick buttons on the left. */
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_table_attach (GTK_TABLE (mts), hbox,
			  0, 1, 5, 6, GTK_FILL, 0, 0, 0);
	gtk_widget_show (hbox);

	mts->autopick_down_button = gtk_button_new_with_label ("");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (mts->autopick_down_button)->child),
					  _("_<<"));
	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (GTK_BIN (mts->autopick_down_button)->child));
	gtk_widget_add_accelerator (mts->autopick_down_button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK | GDK_SHIFT_MASK, 0);
	gtk_box_pack_start (GTK_BOX (hbox), mts->autopick_down_button, TRUE, TRUE, 6);
	gtk_widget_show (mts->autopick_down_button);
	g_signal_connect (mts->autopick_down_button, "clicked",
			  G_CALLBACK (e_meeting_time_selector_on_prev_button_clicked), mts);

	mts->autopick_button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), mts->autopick_button, TRUE, TRUE, 6);
	gtk_widget_show (mts->autopick_button);

	child_hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (mts->autopick_button), child_hbox);
	gtk_widget_show (child_hbox);

	label = gtk_label_new ("");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (label), _("_Autopick"));
	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 6);
	gtk_widget_show (label);
	gtk_widget_add_accelerator (mts->autopick_button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK, 0);
	g_signal_connect (mts->autopick_button, "clicked",
			  G_CALLBACK (e_meeting_time_selector_on_autopick_button_clicked), mts);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (child_hbox), arrow, FALSE, FALSE, 6);
	gtk_widget_show (arrow);

	mts->autopick_up_button = gtk_button_new_with_label ("");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (mts->autopick_up_button)->child),
					  _(">_>"));
	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (GTK_BIN (mts->autopick_up_button)->child));
	gtk_widget_add_accelerator (mts->autopick_up_button, "clicked", mts->accel_group,
				    accel_key, GDK_MOD1_MASK | GDK_SHIFT_MASK, 0);
	gtk_box_pack_start (GTK_BOX (hbox), mts->autopick_up_button, TRUE, TRUE, 6);
	gtk_widget_show (mts->autopick_up_button);
	g_signal_connect (mts->autopick_up_button, "clicked",
			  G_CALLBACK (e_meeting_time_selector_on_next_button_clicked), mts);

	/* Create the Autopick menu. */
	mts->autopick_menu = gtk_menu_new ();
	gtk_menu_attach_to_widget (GTK_MENU (mts->autopick_menu), mts->autopick_button,
				   e_meeting_time_selector_autopick_menu_detacher);

	menuitem = gtk_radio_menu_item_new_with_label (NULL, "");
	mts->autopick_all_item = menuitem;
	group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menuitem)->child), _("_All people and resources"));
	gtk_menu_append (GTK_MENU (mts->autopick_menu), menuitem);
	g_signal_connect (menuitem, "toggled",
			  G_CALLBACK (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_all_people_one_resource_item = menuitem;
	group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menuitem)->child), _("All _people and one resource"));
	gtk_menu_append (GTK_MENU (mts->autopick_menu), menuitem);
	g_signal_connect (menuitem, "toggled",
			  G_CALLBACK (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_required_people_item = menuitem;
	group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menuitem)->child), _("_Required people"));
	gtk_menu_append (GTK_MENU (mts->autopick_menu), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_required_people_one_resource_item = menuitem;
	group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menuitem)->child), _("Required people and _one resource"));
	gtk_menu_append (GTK_MENU (mts->autopick_menu), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	/* Create the date entry fields on the right. */
	alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_table_attach (GTK_TABLE (mts), alignment,
			  1, 4, 5, 6, GTK_FILL, 0, 0, 0);
	gtk_widget_show (alignment);

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_container_add (GTK_CONTAINER (alignment), table);
	gtk_widget_show (table);

	mts->start_date_edit = e_date_edit_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), mts->start_date_edit);
	a11y_label = gtk_widget_get_accessible (label);
	a11y_date_edit = gtk_widget_get_accessible (mts->start_date_edit);
	if (a11y_label != NULL && a11y_date_edit != NULL) {
		atk_object_add_relationship (a11y_date_edit,
					ATK_RELATION_LABELLED_BY,
					a11y_label);
	}
	e_date_edit_set_show_time (E_DATE_EDIT (mts->start_date_edit), TRUE);
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (mts->start_date_edit),
					    calendar_config_get_24_hour_format ());
	
	gtk_table_attach (GTK_TABLE (table), mts->start_date_edit,
			  1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (mts->start_date_edit);
	g_signal_connect (mts->start_date_edit, "changed",
			  G_CALLBACK (e_meeting_time_selector_on_start_time_changed), mts);

	label = gtk_label_new_with_mnemonic (_("_Start time:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), (mts->start_date_edit));

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1, 0, 1, GTK_FILL, 0, 4, 0);
	gtk_widget_show (label);

	mts->end_date_edit = e_date_edit_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), mts->end_date_edit);
	a11y_label = gtk_widget_get_accessible (label);
	a11y_date_edit = gtk_widget_get_accessible (mts->end_date_edit);
	if (a11y_label != NULL && a11y_date_edit != NULL) {
		atk_object_add_relationship (a11y_date_edit,
					ATK_RELATION_LABELLED_BY,
					a11y_label);
	}
	e_date_edit_set_show_time (E_DATE_EDIT (mts->end_date_edit), TRUE);
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (mts->end_date_edit),
					    calendar_config_get_24_hour_format ());

	gtk_table_attach (GTK_TABLE (table), mts->end_date_edit,
			  1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show (mts->end_date_edit);
	g_signal_connect (mts->end_date_edit, "changed",
			  G_CALLBACK (e_meeting_time_selector_on_end_time_changed), mts);

	label = gtk_label_new_with_mnemonic (_("_End time:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), (mts->end_date_edit));

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1, 1, 2, GTK_FILL, 0, 4, 0);
	gtk_widget_show (label);

	gtk_table_set_col_spacing (GTK_TABLE (mts), 0, 4);
	gtk_table_set_row_spacing (GTK_TABLE (mts), 4, 12);

	/* Allocate the colors. */
	visual = gtk_widget_get_visual (GTK_WIDGET (mts));
	colormap = gtk_widget_get_colormap (GTK_WIDGET (mts));
	e_meeting_time_selector_alloc_named_color (mts, "gray75", &mts->bg_color);
	e_meeting_time_selector_alloc_named_color (mts, "gray50", &mts->all_attendees_bg_color);
	gdk_color_black (colormap, &mts->grid_color);
	gdk_color_white (colormap, &mts->grid_shadow_color);
	e_meeting_time_selector_alloc_named_color (mts, "gray50", &mts->grid_unused_color);
	gdk_color_white (colormap, &mts->meeting_time_bg_color);
	gdk_color_white (colormap, &mts->stipple_bg_color);
	gdk_color_white (colormap, &mts->attendee_list_bg_color);

	e_meeting_time_selector_alloc_named_color (mts, "LightSkyBlue2", &mts->busy_colors[E_MEETING_FREE_BUSY_TENTATIVE]);
	e_meeting_time_selector_alloc_named_color (mts, "blue", &mts->busy_colors[E_MEETING_FREE_BUSY_BUSY]);
	e_meeting_time_selector_alloc_named_color (mts, "HotPink3", &mts->busy_colors[E_MEETING_FREE_BUSY_OUT_OF_OFFICE]);

	/* Create the stipple, for attendees with no data. */
	mts->stipple = gdk_bitmap_create_from_data (NULL, (gchar*)stipple_bits,
						    8, 8);

	/* Connect handlers to the adjustments  scroll the other items. */
	g_signal_connect (GTK_LAYOUT (mts->display_main)->hadjustment, "value_changed",
			  G_CALLBACK (e_meeting_time_selector_hadjustment_changed), mts);
	g_signal_connect (GTK_LAYOUT (mts->display_main)->vadjustment, "value_changed",
			  G_CALLBACK (e_meeting_time_selector_vadjustment_changed), mts);
	g_signal_connect (GTK_LAYOUT (mts->display_main)->vadjustment, "changed",
			  G_CALLBACK (e_meeting_time_selector_vadjustment_changed), mts);

	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
	e_meeting_time_selector_update_start_date_edit (mts);
	e_meeting_time_selector_update_end_date_edit (mts);	
	e_meeting_time_selector_update_date_popup_menus (mts);

	gtk_signal_emit (GTK_OBJECT (mts), mts_signals [CHANGED]);
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
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 6);
	gtk_widget_show (label);

	g_signal_connect (darea, "expose_event",
			  G_CALLBACK (e_meeting_time_selector_expose_key_color),
			  color);
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
	GdkColormap *colormap;
	
	g_return_if_fail (name != NULL);
	g_return_if_fail (c != NULL);

	gdk_color_parse (name, c);
	colormap = gtk_widget_get_colormap (GTK_WIDGET (mts));
	if (!gdk_colormap_alloc_color (colormap, c, TRUE, TRUE))
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
e_meeting_time_selector_new (EMeetingStore *ems)
{
	GtkWidget *mts;

	mts = GTK_WIDGET (g_object_new (e_meeting_time_selector_get_type (), NULL));

	e_meeting_time_selector_construct (E_MEETING_TIME_SELECTOR (mts), ems);
	
	return mts;
}


static void
e_meeting_time_selector_destroy (GtkObject *object)
{
	EMeetingTimeSelector *mts;

	mts = E_MEETING_TIME_SELECTOR (object);

	e_meeting_time_selector_remove_timeout (mts);
	
	if (mts->stipple) {
		gdk_bitmap_unref (mts->stipple);
		mts->stipple = NULL;
	}
	
	if (mts->model) {
		g_object_unref (mts->model);
		mts->model = NULL;
	}
	
	mts->display_top = NULL;
	mts->display_main = NULL;

	calendar_config_remove_notification (mts->fb_uri_not);

	if (mts->fb_refresh_not != 0) {
		g_source_remove (mts->fb_refresh_not);
	}
	
	if (GTK_OBJECT_CLASS (e_meeting_time_selector_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (e_meeting_time_selector_parent_class)->destroy)(object);
}


static void
e_meeting_time_selector_realize (GtkWidget *widget)
{
	EMeetingTimeSelector *mts;

	if (GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->realize)
		(*GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->realize)(widget);

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

	if (GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->unrealize)(widget);
}

static int
get_cell_height (GtkTreeView *tree)
{
	GtkTreeViewColumn *column;
	int height = -1;

	column = gtk_tree_view_get_column (tree, 0);
	gtk_tree_view_column_cell_get_size (column, NULL,
					    NULL, NULL,
					    NULL, &height);
	
	return height;
}

static void
e_meeting_time_selector_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
	EMeetingTimeSelector *mts;
	EMeetingTime saved_time;
	int hour, max_hour_width;
	int maxheight;      
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;
	GtkTreePath *path;
	GdkRectangle cell_area;

	if (GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->style_set)
		(*GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->style_set)(widget, previous_style);

	mts = E_MEETING_TIME_SELECTOR (widget);

	/* Set up Pango prerequisites */
	font_desc = gtk_widget_get_style (widget)->font_desc;
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (pango_context, font_desc,
						  pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);
	
	/* Calculate the widths of the hour strings in the style's font. */
	max_hour_width = 0;
	for (hour = 0; hour < 24; hour++) {
		if (calendar_config_get_24_hour_format ())
			pango_layout_set_text (layout, EMeetingTimeSelectorHours [hour], -1);
		else
			pango_layout_set_text (layout, EMeetingTimeSelectorHours12 [hour], -1);

		pango_layout_get_pixel_size (layout, &mts->hour_widths [hour], NULL);
		max_hour_width = MAX (max_hour_width, mts->hour_widths[hour]);
	}
              
	mts->row_height = get_cell_height (GTK_TREE_VIEW (mts->list_view));
	mts->col_width = max_hour_width + 6;

	e_meeting_time_selector_save_position (mts, &saved_time);
	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_restore_position (mts, &saved_time);
              
	gtk_widget_set_usize (mts->display_top, -1, mts->row_height * 3 + 4);
 
	/*
	 * FIXME: I can't find a way to get the treeview header heights
	 * other than the below but it isn't nice to realize that widget here
	 *

	gtk_widget_realize (mts->list_view);
	gdk_window_get_position (gtk_tree_view_get_bin_window (GTK_TREE_VIEW (mts->list_view)),
				 NULL, &maxheight);
	gtk_widget_set_usize (mts->attendees_vbox_spacer, 1, mts->row_height * 3 - maxheight);

	*/	

	gtk_widget_set_usize (mts->attendees_vbox_spacer, 1, mts->row_height * 2 - 6);

	GTK_LAYOUT (mts->display_main)->hadjustment->step_increment = mts->col_width;
	GTK_LAYOUT (mts->display_main)->vadjustment->step_increment = mts->row_height;

	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);
}

/* This draws a shadow around the top display and main display. */
static gint
e_meeting_time_selector_expose_event (GtkWidget *widget,
				      GdkEventExpose *event)
{
	EMeetingTimeSelector *mts;

	mts = E_MEETING_TIME_SELECTOR (widget);

	e_meeting_time_selector_draw_shadow (mts);

	if (GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->expose_event)
		(*GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->expose_event)(widget, event);

	return FALSE;
}

static void
e_meeting_time_selector_draw_shadow (EMeetingTimeSelector *mts)
{
	GtkWidget *widget;
	gint x, y, w, h;

	widget = GTK_WIDGET (mts);

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

	adj = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (mts->list_view));
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

	/* Set the times in the EDateEdit widgets. */
	e_meeting_time_selector_update_start_date_edit (mts);
	e_meeting_time_selector_update_end_date_edit (mts);

	gtk_signal_emit (GTK_OBJECT (mts), mts_signals [CHANGED]);

	return TRUE;
}

void
e_meeting_time_selector_set_all_day (EMeetingTimeSelector *mts,
				     gboolean all_day)
{
	EMeetingTime saved_time;

	mts->all_day = all_day;
	
	e_date_edit_set_show_time (E_DATE_EDIT (mts->start_date_edit),
				   !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (mts->end_date_edit),
				   !all_day);

	e_meeting_time_selector_save_position (mts, &saved_time);
	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_restore_position (mts, &saved_time);

	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
	e_meeting_time_selector_update_date_popup_menus (mts);
}


void
e_meeting_time_selector_set_working_hours_only (EMeetingTimeSelector *mts,
						gboolean working_hours_only)
{
	EMeetingTime saved_time;

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
	EMeetingTime saved_time;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));

	if (mts->day_start_hour == day_start_hour
	    && mts->day_start_minute == day_start_minute
	    && mts->day_end_hour == day_end_hour
	    && mts->day_end_minute == day_end_minute)
		return;

	mts->day_start_hour = day_start_hour;
	mts->day_start_minute = day_start_minute;

	/* Make sure we always show atleast an hour */
	if (day_start_hour * 60 + day_start_minute + 60 < day_end_hour * 60 + day_end_minute) {
		mts->day_end_hour = day_end_hour;
		mts->day_end_minute = day_end_minute;
	} else {
		mts->day_end_hour = day_start_hour + 1;
		mts->day_end_minute = day_start_minute;
	}
	
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
	EMeetingTime saved_time;

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

static void
e_meeting_time_selector_refresh_cb (gpointer data) 
{
	EMeetingTimeSelector *mts = data;

	if (mts->display_top != NULL)
		gtk_widget_queue_draw (mts->display_top);
	if (mts->display_main != NULL)
		gtk_widget_queue_draw (mts->display_main);

	gtk_object_unref (GTK_OBJECT (mts));
}

static void
e_meeting_time_selector_refresh_free_busy (EMeetingTimeSelector *mts, int row, gboolean all)
{
	EMeetingTime start, end;
	
	start = mts->meeting_start_time;
	g_date_subtract_days (&start.date, E_MEETING_TIME_SELECTOR_FB_DAYS_BEFORE);
	start.hour = 0;
	start.minute = 0;
	end = mts->meeting_end_time;
	g_date_add_days (&end.date, E_MEETING_TIME_SELECTOR_FB_DAYS_AFTER);
	end.hour = 0;
	end.minute = 0;	

	/* Ref ourselves in case we are called back after destruction,
	 * we can do this because we will get a call back even after
	 * an error */
	/* FIXME We should really have a mechanism to unqueue the
	 * notification */
	if (all) {
		int i;
		
		for (i = 0; i < e_meeting_store_count_actual_attendees (mts->model); i++)
			gtk_object_ref (GTK_OBJECT (mts));
	} else {
		gtk_object_ref (GTK_OBJECT (mts));
	}
	
	if (all)
		e_meeting_store_refresh_all_busy_periods (mts->model, &start, &end, 
							  e_meeting_time_selector_refresh_cb, mts);
	else
		e_meeting_store_refresh_busy_periods (mts->model, row, &start, &end, 
						      e_meeting_time_selector_refresh_cb, mts);
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
#if 0
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
#endif

void
e_meeting_time_selector_set_read_only (EMeetingTimeSelector *mts, gboolean read_only)
{
	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR (mts));

	gtk_widget_set_sensitive (GTK_WIDGET (mts->list_view), !read_only);
	gtk_widget_set_sensitive (mts->display_main, !read_only);
	gtk_widget_set_sensitive (mts->add_attendees_button, !read_only);
	gtk_widget_set_sensitive (mts->autopick_down_button, !read_only);
	gtk_widget_set_sensitive (mts->autopick_button, !read_only);
	gtk_widget_set_sensitive (mts->autopick_up_button, !read_only);
	gtk_widget_set_sensitive (mts->start_date_edit, !read_only);
	gtk_widget_set_sensitive (mts->end_date_edit, !read_only);
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

			/* These are just for debugging so don't need i18n. */
			g_date_strftime (buffer, sizeof (buffer),
					 "%A, %B %d, %Y", &period->start.date);
			g_print ("  Start: %s %i:%02i\n", buffer,
				 period->start.hour, period->start.minute);

			g_date_strftime (buffer, sizeof (buffer),
					 "%A, %B %d, %Y", &period->end.date);
			g_print ("  End  : %s %i:%02i\n", buffer,
				 period->end.hour, period->end.minute);
		}
#endif
	}

}


/* This formats a EMeetingTimein a string and returns it.
   Note that it uses a static buffer. */
gchar*
e_meeting_time_selector_dump_time (EMeetingTime*mtstime)
{
	static gchar buffer[128];

	gchar buffer2[128];

	/* This is just for debugging so doesn't need i18n. */
	g_date_strftime (buffer, sizeof (buffer), "%A, %B %d, %Y",
			 &mtstime->date);
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

	/* This is just for debugging so doesn't need i18n. */
	g_date_strftime (buffer, sizeof (buffer), "%A, %B %d, %Y", date);
	return buffer;
}

#endif /* E_MEETING_TIME_SELECTOR_DEBUG */

static void
e_meeting_time_selector_on_invite_others_button_clicked (GtkWidget *button,
							 EMeetingTimeSelector *mts)
{
	e_meeting_list_view_invite_others_dialog (mts->list_view);
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
							gboolean *push_in,
							gpointer user_data)
{
	EMeetingTimeSelector *mts;
	GtkRequisition menu_requisition;
	gint max_x, max_y;

	mts = E_MEETING_TIME_SELECTOR (user_data);

	/* Calculate our preferred position. */
	gdk_window_get_origin (mts->options_button->window, x, y);
	*x += mts->options_button->allocation.x;
	*y += mts->options_button->allocation.y + mts->options_button->allocation.height / 2 - 2;

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

	e_meeting_time_selector_refresh_free_busy (mts, 0, TRUE);
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
							 gboolean *push_in,
							 gpointer user_data)
{
	EMeetingTimeSelector *mts;
	GtkRequisition menu_requisition;
	gint max_x, max_y;

	mts = E_MEETING_TIME_SELECTOR (user_data);

	/* Calculate our preferred position. */
	gdk_window_get_origin (mts->autopick_button->window, x, y);
	*x += mts->autopick_button->allocation.x;
	*y += mts->autopick_button->allocation.y + mts->autopick_button->allocation.height / 2 - 2;

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
	EMeetingTime start_time, end_time, *resource_free;
	EMeetingAttendee *attendee;
	EMeetingFreeBusyPeriod *period;
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
		for (row = 0; row <  e_meeting_store_count_actual_attendees (mts->model); row++) {
			attendee = e_meeting_store_find_attendee_at_row (mts->model, row);

			/* Skip optional people if they don't matter. */
			if (skip_optional && e_meeting_attendee_get_atype (attendee) == E_MEETING_ATTENDEE_OPTIONAL_PERSON)
				continue;

			period = e_meeting_time_selector_find_time_clash (mts, attendee, &start_time, &end_time);

			if (need_one_resource && e_meeting_attendee_get_atype (attendee) == E_MEETING_ATTENDEE_RESOURCE) {
				if (period) {
					/* We want to remember the closest
					   prev/next time that one resource is
					   available, in case we don't find any
					   free resources. */
					if (forward) {
						if (!resource_free || e_meeting_time_compare_times (resource_free, &period->end) > 0)
							resource_free = &period->end;
					} else {
						if (!resource_free || e_meeting_time_compare_times (resource_free, &period->start) < 0)
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

			/* Set the times in the EDateEdit widgets. */
			e_meeting_time_selector_update_start_date_edit (mts);
			e_meeting_time_selector_update_end_date_edit (mts);

			gtk_signal_emit (GTK_OBJECT (mts), mts_signals [CHANGED]);

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
e_meeting_time_selector_calculate_time_difference (EMeetingTime*start,
						   EMeetingTime*end,
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
					       EMeetingTime*start_time,
					       EMeetingTime*end_time,
					       gint days, gint hours, gint mins)
{
	gint minutes_shown;
	gboolean set_to_start_of_working_day = FALSE;

	if (!mts->all_day) {
		if (mts->zoomed_out) {
			start_time->hour++;
			start_time->minute = 0;
		} else {
			start_time->minute += 30;
			start_time->minute -= start_time->minute % 30;
		}
	} else {
		g_date_add_days (&start_time->date, 1);
		start_time->hour = 0;
		start_time->minute = 0;	
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
							EMeetingTime*start_time,
							EMeetingTime*end_time,
							gint days, gint hours, gint mins)
{
	gint new_hour, minutes_shown;
	gboolean set_to_end_of_working_day = FALSE;

	if (!mts->all_day) {
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
	} else {
		g_date_subtract_days (&start_time->date, 1);
		start_time->hour = 0;
		start_time->minute = 0;	
	}

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
e_meeting_time_selector_adjust_time (EMeetingTime*mtstime,
				     gint days, gint hours, gint minutes)
{
	gint new_hours, new_minutes;

	/* We have to handle negative values for hous and minutes here, since
	   EMeetingTimeuses guint8s to store them. */
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
static EMeetingFreeBusyPeriod *
e_meeting_time_selector_find_time_clash (EMeetingTimeSelector *mts,
					 EMeetingAttendee *attendee,
					 EMeetingTime*start_time,
					 EMeetingTime*end_time)
{
	EMeetingFreeBusyPeriod *period;
	const GArray *busy_periods;
	gint period_num;

	busy_periods = e_meeting_attendee_get_busy_periods (attendee);
	period_num = e_meeting_attendee_find_first_busy_period (attendee, &start_time->date);

	if (period_num == -1)
		return NULL;

	/* Step forward through the busy periods until we find a clash or we
	   go past the end_time. */
	while (period_num < busy_periods->len) {
		period = &g_array_index (busy_periods,  EMeetingFreeBusyPeriod, period_num);

		/* If the period starts at or after the end time, there is no
		   clash and we are finished. The busy periods are sorted by
		   their start times, so all the rest will be later. */
		if (e_meeting_time_compare_times (&period->start, end_time) >= 0)
			return NULL;

		/* If the period ends after the start time, we have found a
		   clash. From the above test we already know the busy period
		   isn't completely after the meeting time. */
		if (e_meeting_time_compare_times (&period->end, start_time) > 0)
			return period;

		period_num++;
	}

	return NULL;
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
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
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
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
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
				       EMeetingTime*mtstime)
{
	gint scroll_x, scroll_y;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (mts->display_main),
					 &scroll_x, &scroll_y);
	e_meeting_time_selector_calculate_time (mts, scroll_x, mtstime);
}


/* This restores a saved position. */
static void
e_meeting_time_selector_restore_position (EMeetingTimeSelector *mts,
					  EMeetingTime*mtstime)
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
	/* An array of dates, one for each month in the year 2000. They must
	   all be Sundays. */
	static const int days[12] = { 23, 20, 19, 23, 21, 18,
				      23, 20, 17, 22, 19, 24 };
	GDate date;
	gint max_date_width, longest_weekday_width, longest_month_width, width;
	gint day, longest_weekday, month, longest_month;
	gchar buffer[128];
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoLayout *layout;

	/* Set up Pango prerequisites */
	font_desc = gtk_widget_get_style (GTK_WIDGET (mts))->font_desc;
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (mts));
	layout = pango_layout_new (pango_context);

	/* Calculate the maximum date width we can fit into the display. */
	max_date_width = mts->day_width - 2;

	/* Find the biggest full weekday name. We start on a particular
	   Monday and go through seven days. */
	g_date_clear (&date, 1);
	g_date_set_dmy (&date, 3, 1, 2000);	/* Monday 3rd Jan 2000. */
	longest_weekday_width = 0;
	longest_weekday = G_DATE_MONDAY;
	for (day = G_DATE_MONDAY; day <= G_DATE_SUNDAY; day++) {
		g_date_strftime (buffer, sizeof (buffer), "%A", &date);
		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);
		if (width > longest_weekday_width) {
			longest_weekday = day;
			longest_weekday_width = width;
		}
		g_date_add_days (&date, 1);
	}

	/* Now find the biggest month name. */
	longest_month_width = 0;
	longest_month = G_DATE_JANUARY;
	for (month = G_DATE_JANUARY; month <= G_DATE_DECEMBER; month++) {
		g_date_set_month (&date, month);
		g_date_strftime (buffer, sizeof (buffer), "%B", &date);
		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);
		if (width > longest_month_width) {
			longest_month = month;
			longest_month_width = width;
		}
	}

	/* See if we can use the full date. We want to use a date with a
	   month day > 20 and also the longest weekday. We use a
	   pre-calculated array of days for each month and add on the
	   weekday (which is 1 (Mon) to 7 (Sun). */
	g_date_set_dmy (&date, days[longest_month - 1] + longest_weekday,
			longest_month, 2000);
	/* This is a strftime() format string %A = full weekday name,
	   %B = full month name, %d = month day, %Y = full year. */
	g_date_strftime (buffer, sizeof (buffer), _("%A, %B %d, %Y"), &date);

#if 0
	g_print ("longest_month: %i longest_weekday: %i date: %s\n",
		 longest_month, longest_weekday, buffer);
#endif

	pango_layout_set_text (layout, buffer, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	if (width < max_date_width) {
		mts->date_format = E_MEETING_TIME_SELECTOR_DATE_FULL;
		return;
	}

	/* Now try it with abbreviated weekday names. */
	longest_weekday_width = 0;
	longest_weekday = G_DATE_MONDAY;
	g_date_set_dmy (&date, 3, 1, 2000);	/* Monday 3rd Jan 2000. */
	for (day = G_DATE_MONDAY; day <= G_DATE_SUNDAY; day++) {
		g_date_strftime (buffer, sizeof (buffer), "%a", &date);
		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);
		if (width > longest_weekday_width) {
			longest_weekday = day;
			longest_weekday_width = width;
		}
		g_date_add_days (&date, 1);
	}

	g_date_set_dmy (&date, days[longest_month - 1] + longest_weekday,
			longest_month, 2000);
	/* This is a strftime() format string %a = abbreviated weekday name,
	   %m = month number, %d = month day, %Y = full year. */
	g_date_strftime (buffer, sizeof (buffer), _("%a %m/%d/%Y"), &date);

#if 0
	g_print ("longest_month: %i longest_weekday: %i date: %s\n",
		 longest_month, longest_weekday, buffer);
#endif

	pango_layout_set_text (layout, buffer, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	if (width < max_date_width)
		mts->date_format = E_MEETING_TIME_SELECTOR_DATE_ABBREVIATED_DAY;
	else
		mts->date_format = E_MEETING_TIME_SELECTOR_DATE_SHORT;

	g_object_unref (layout);
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
	EMeetingTime mtstime;
	gint hour = 0, minute = 0;
	time_t newtime;

	/* Date */
	newtime = e_date_edit_get_time (E_DATE_EDIT (mts->start_date_edit));
	g_date_clear (&mtstime.date, 1);
	g_date_set_time (&mtstime.date, newtime);

	/* Time */
	e_date_edit_get_time_of_day (E_DATE_EDIT (mts->start_date_edit), &hour, &minute);
	mtstime.hour = hour;
	mtstime.minute = minute;

	/* If the time hasn't changed, just return. */
	if (e_meeting_time_compare_times (&mtstime, &mts->meeting_start_time) == 0)
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

	gtk_signal_emit (GTK_OBJECT (mts), mts_signals [CHANGED]);
}


/* This is called when the meeting end time GnomeDateEdit is changed,
   either via the "date_changed", "time_changed" or "activate" signals on one
   of the GtkEntry widgets. So don't use the widget parameter since it may be
   one of the child GtkEntry widgets. */
static void
e_meeting_time_selector_on_end_time_changed (GtkWidget *widget,
					     EMeetingTimeSelector *mts)
{
	EMeetingTime mtstime;
	gint hour = 0, minute = 0;
	time_t newtime;

	/* Date */
	newtime = e_date_edit_get_time (E_DATE_EDIT (mts->end_date_edit));
	g_date_clear (&mtstime.date, 1);
	g_date_set_time (&mtstime.date, newtime);
	if (mts->all_day)
		g_date_add_days (&mtstime.date, 1);

	/* Time */
	e_date_edit_get_time_of_day (E_DATE_EDIT (mts->end_date_edit), &hour, &minute);
	mtstime.hour = hour;
	mtstime.minute = minute;

	/* If the time hasn't changed, just return. */
	if (e_meeting_time_compare_times (&mtstime, &mts->meeting_end_time) == 0)
		return;

	/* Set the new end time. */
	mts->meeting_end_time = mtstime;

	/* If the start time is after the end time, set it to the same time. */
	if (e_meeting_time_compare_times (&mtstime, &mts->meeting_start_time) <= 0) {
		/* We set it first, before updating the widget, so the signal
		   handler will just return. */
		mts->meeting_start_time = mtstime;
		if (mts->all_day)
			g_date_subtract_days (&mts->meeting_start_time.date, 1);
		e_meeting_time_selector_update_start_date_edit (mts);
	}

	mts->meeting_positions_valid = FALSE;
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);

	gtk_signal_emit (GTK_OBJECT (mts), mts_signals [CHANGED]);
}


/* This updates the ranges shown in the GnomeDateEdit popup menus, according
   to working_hours_only etc. */
static void
e_meeting_time_selector_update_date_popup_menus (EMeetingTimeSelector *mts)
{
	EDateEdit *start_edit, *end_edit;
	gint low_hour, high_hour;

	start_edit = E_DATE_EDIT (mts->start_date_edit);
	end_edit = E_DATE_EDIT (mts->end_date_edit);

	if (mts->working_hours_only) {
		low_hour = mts->day_start_hour;
		high_hour = mts->day_end_hour;
	} else {
		low_hour = 0;
		high_hour = 23;
	}

	e_date_edit_set_time_popup_range (start_edit, low_hour, high_hour);
	e_date_edit_set_time_popup_range (end_edit, low_hour, high_hour);
}


static void
e_meeting_time_selector_on_canvas_size_allocate (GtkWidget *widget,
						 GtkAllocation *allocation,
						 EMeetingTimeSelector *mts)
{
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);

	e_meeting_time_selector_ensure_meeting_time_shown (mts);
}

/* This updates the canvas scroll regions according to the number of attendees.
   If the total height needed is less than the height of the canvas, we must
   use the height of the canvas, or it causes problems. */
static void
e_meeting_time_selector_update_main_canvas_scroll_region (EMeetingTimeSelector *mts)
{
	gint height, canvas_height;

	height = mts->row_height * (e_meeting_store_count_actual_attendees (mts->model) + 2);
	canvas_height = GTK_WIDGET (mts->display_main)->allocation.height;

	height = MAX (height,  canvas_height);

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
	EMeetingTime first_time, last_time, drag_time, *time_to_set;
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
	if (!mts->all_day) {
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
	} else {
		if (first_time.hour > 0 || first_time.minute > 0)
			g_date_add_days (&first_time.date, 1);
		first_time.hour = 0;
		first_time.minute = 0;
		last_time.hour = 0;
		last_time.minute = 0;
	}
	e_meeting_time_selector_fix_time_overflows (&first_time);
	e_meeting_time_selector_fix_time_overflows (&last_time);

	/* Calculate the time from x coordinate. */
	e_meeting_time_selector_calculate_time (mts, x, &drag_time);

	/* Calculate the nearest half-hour or hour, depending on whether
	   zoomed_out is set. */
	if (!mts->all_day) {
		if (mts->zoomed_out) {
			if (drag_time.minute > 30)
				drag_time.hour++;
			drag_time.minute = 0;
		} else {
			drag_time.minute += 15;
			drag_time.minute -= drag_time.minute % 30;
		}
	} else {
		if (drag_time.hour > 12)
			g_date_add_days (&drag_time.date, 1);
		drag_time.hour = 0;
		drag_time.minute = 0;		
	}
	e_meeting_time_selector_fix_time_overflows (&drag_time);

	/* Now make sure we are between first_time & last_time. */
	if (e_meeting_time_compare_times (&drag_time, &first_time) < 0)
		drag_time = first_time;
	if (e_meeting_time_compare_times (&drag_time, &last_time) > 0)
		drag_time = last_time;

	/* Set the meeting start or end time to drag_time. */
	if (mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
		time_to_set = &mts->meeting_start_time;
	else
		time_to_set = &mts->meeting_end_time;

	/* If the time is unchanged, just return. */
	if (e_meeting_time_compare_times (time_to_set, &drag_time) == 0)
		return;

	/* Don't let an empty occur for all day events */
	if (mts->all_day
	    && mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START
	    && e_meeting_time_compare_times (&mts->meeting_end_time, &drag_time) == 0)
		return;
	else if (mts->all_day
		 && mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_END
		 && e_meeting_time_compare_times (&mts->meeting_start_time, &drag_time) == 0)
		return;
	
	*time_to_set = drag_time;

	/* Check if the start time and end time need to be switched. */
	if (e_meeting_time_compare_times (&mts->meeting_start_time,
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

	if (set_both_times
	    || mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_END
	    || mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
		gtk_signal_emit (GTK_OBJECT (mts), mts_signals [CHANGED]);
}


/* This is the timeout function which handles auto-scrolling when the user is
   dragging one of the meeting time vertical bars outside the left or right
   edge of the canvas. */
static gboolean
e_meeting_time_selector_timeout_handler (gpointer data)
{
	EMeetingTimeSelector *mts;
	EMeetingTime drag_time, *time_to_set;
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
		if (!mts->all_day) {
			if (mts->zoomed_out) {
				drag_time.minute = 0;
			} else {
				drag_time.minute -= drag_time.minute % 30;
			}
		} else {
			drag_time.hour = 0;
			drag_time.minute = 0;
		}
	} else {
		e_meeting_time_selector_calculate_time (mts, scroll_x,
							&drag_time);
		if (!mts->all_day) {
			if (mts->zoomed_out) {
				if (drag_time.minute > 30)
					drag_time.hour++;
				drag_time.minute = 0;
			} else {
				drag_time.minute += 15;
				drag_time.minute -= drag_time.minute % 30;
			}
		} else {
			if (drag_time.hour > 0 || drag_time.minute > 0)
				g_date_add_days (&drag_time.date, 1);
			drag_time.hour = 0;
			drag_time.minute = 0;
		}
	}
	e_meeting_time_selector_fix_time_overflows (&drag_time);

	/* Set the meeting start or end time to drag_time. */
	if (mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
		time_to_set = &mts->meeting_start_time;
	else
		time_to_set = &mts->meeting_end_time;

	/* If the time is unchanged, just return. */
	if (e_meeting_time_compare_times (time_to_set, &drag_time) == 0) {
		GDK_THREADS_LEAVE ();
		goto scroll;
	}

	/* Don't let an empty occur for all day events */
	if (mts->all_day
	    && mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START
	    && e_meeting_time_compare_times (&mts->meeting_end_time, &drag_time) == 0)
		goto scroll;
	else if (mts->all_day
		 && mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_END
		 && e_meeting_time_compare_times (&mts->meeting_start_time, &drag_time) == 0)
		goto scroll;

	*time_to_set = drag_time;

	/* Check if the start time and end time need to be switched. */
	if (e_meeting_time_compare_times (&mts->meeting_start_time, &mts->meeting_end_time) > 0) {
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

	if (set_both_times
	    || mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_END
	    || mts->dragging_position == E_MEETING_TIME_SELECTOR_POS_START)
		gtk_signal_emit (GTK_OBJECT (mts), mts_signals [CHANGED]);

 scroll:
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
	e_date_edit_set_date_and_time_of_day (E_DATE_EDIT (mts->start_date_edit),
					      g_date_year (&mts->meeting_start_time.date),
					      g_date_month (&mts->meeting_start_time.date),
					      g_date_day (&mts->meeting_start_time.date),
					      mts->meeting_start_time.hour,
					      mts->meeting_start_time.minute);
}


/* This updates the GnomeDateEdit widget displaying the meeting end time. */
static void
e_meeting_time_selector_update_end_date_edit (EMeetingTimeSelector *mts)
{
	GDate date;
	
	date = mts->meeting_end_time.date;
	if (mts->all_day)
		g_date_subtract_days (&date, 1);

	e_date_edit_set_date_and_time_of_day (E_DATE_EDIT (mts->end_date_edit),
					      g_date_year (&date),
					      g_date_month (&date),
					      g_date_day (&date),
					      mts->meeting_end_time.hour,
					      mts->meeting_end_time.minute);
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
	g_date_subtract_days (&mts->first_date_shown,
			      E_MEETING_TIME_SELECTOR_DAYS_START_BEFORE);

	mts->last_date_shown = mts->first_date_shown;
	g_date_add_days (&mts->last_date_shown,
			 E_MEETING_TIME_SELECTOR_DAYS_SHOWN - 1);
}


/* This checks if the time's hour is over 24 or its minute is over 60 and if
   so it updates the day/hour appropriately. Note that hours and minutes are
   stored in guint8's so they can't overflow by much. */
void
e_meeting_time_selector_fix_time_overflows (EMeetingTime*mtstime)
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
					EMeetingTime*time)
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


/* This takes a EMeetingTime and calculates the x pixel coordinate
   within the entire canvas scroll region. It is used to draw the selected
   meeting time and all the busy periods. */
gint
e_meeting_time_selector_calculate_time_position (EMeetingTimeSelector *mts,
						 EMeetingTime *mtstime)
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

static void
row_inserted_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);
	int row = gtk_tree_path_get_indices (path) [0];
	/* Update the scroll region. */
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);

	/* Redraw */
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);

	/* Get the latest free/busy info */
	e_meeting_time_selector_refresh_free_busy (mts, row, FALSE);
}

static void
row_changed_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);
	int row = gtk_tree_path_get_indices (path) [0];

	/* Get the latest free/busy info */
	e_meeting_time_selector_refresh_free_busy (mts, row, FALSE);
}

static void
row_deleted_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);

	/* Update the scroll region. */
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);

	/* Redraw */
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
}


#define REFRESH_PAUSE 5000

static gboolean
free_busy_timeout_refresh (gpointer data)
{
	char *fb_uri;
	
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);

	fb_uri = calendar_config_get_free_busy_template ();
	e_meeting_store_set_fb_uri (mts->model, fb_uri);
	g_free (fb_uri);
	
	/* Update all free/busy info, so we use the new template uri */
	e_meeting_time_selector_refresh_free_busy (mts, 0, TRUE);

	mts->fb_refresh_not = 0;
	
	return FALSE;
}

static void
free_busy_template_changed_cb (GConfClient *client,
			       guint cnxn_id,
			       GConfEntry *entry,
			       gpointer data)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);

	/* Wait REFRESH_PAUSE before refreshing, using the latest uri value */
	if (mts->fb_refresh_not != 0) {
		g_source_remove (mts->fb_refresh_not);		
	}

	mts->fb_refresh_not = g_timeout_add (REFRESH_PAUSE, 
					     free_busy_timeout_refresh, 
					     data);
}
