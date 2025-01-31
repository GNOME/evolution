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
 *		Damon Chaplin <damon@gtk.org>
 *		Rodrigo Moya <rodrigo@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-meeting-time-sel.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <libebackend/libebackend.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-util.h>

#include "e-meeting-utils.h"
#include "e-meeting-list-view.h"
#include "e-meeting-time-sel-item.h"

struct _EMeetingTimeSelectorPrivate {
	gboolean use_24_hour_format;
	gulong notify_free_busy_template_id;
	gulong notify_timezone_id;
};

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
#define E_MEETING_TIME_SELECTOR_DAYS_SHOWN		35
#define E_MEETING_TIME_SELECTOR_DAYS_START_BEFORE	7
#define E_MEETING_TIME_SELECTOR_FB_DAYS_BEFORE          7
#define E_MEETING_TIME_SELECTOR_FB_DAYS_AFTER           28

/* This is the number of pixels between the mouse has to move before the
 * scroll speed is incremented. */
#define E_MEETING_TIME_SELECTOR_SCROLL_INCREMENT_WIDTH	10

/* This is the maximum scrolling speed. */
#define E_MEETING_TIME_SELECTOR_MAX_SCROLL_SPEED	4

enum {
	PROP_0,
	PROP_USE_24_HOUR_FORMAT
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

static void e_meeting_time_selector_alloc_named_color (EMeetingTimeSelector * mts,
						       const gchar *name, GdkRGBA *c);
static void e_meeting_time_selector_add_key_color (EMeetingTimeSelector * mts,
						   GtkWidget *hbox,
						   gchar *label_text,
						   GdkRGBA *color);
static gint e_meeting_time_selector_draw_key_color (GtkWidget *darea,
						      cairo_t *cr,
						      GdkRGBA *color);
static void e_meeting_time_selector_options_menu_detacher (GtkWidget *widget,
							   GtkMenu   *menu);
static void e_meeting_time_selector_autopick_menu_detacher (GtkWidget *widget,
							    GtkMenu   *menu);
static void e_meeting_time_selector_realize (GtkWidget *widget);
static void e_meeting_time_selector_unrealize (GtkWidget *widget);
static void e_meeting_time_selector_style_updated (GtkWidget *widget);
static gint e_meeting_time_selector_draw (GtkWidget *widget, cairo_t *cr);
static void e_meeting_time_selector_draw_shadow (EMeetingTimeSelector *mts, cairo_t *cr);
static void e_meeting_time_selector_hadjustment_changed (GtkAdjustment *adjustment,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_vadjustment_changed (GtkAdjustment *adjustment,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_canvas_realized (GtkWidget *widget,
							EMeetingTimeSelector *mts);

static void e_meeting_time_selector_on_options_button_clicked (GtkWidget *button,
							       EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_zoomed_out_toggled (GtkCheckMenuItem *button,
							   EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_working_hours_toggled (GtkCheckMenuItem *menuitem,
							      EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_invite_others_button_clicked (GtkWidget *button,
								     EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_update_free_busy (GtkWidget *button,
							 EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_autopick_button_clicked (GtkWidget *button,
								EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_autopick_option_toggled (GtkWidget *button,
								EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_prev_button_clicked (GtkWidget *button,
							    EMeetingTimeSelector *mts);
static void e_meeting_time_selector_on_next_button_clicked (GtkWidget *button,
							    EMeetingTimeSelector *mts);
static void e_meeting_time_selector_autopick (EMeetingTimeSelector *mts,
					      gboolean forward);
static void e_meeting_time_selector_calculate_time_difference (EMeetingTime *start,
							       EMeetingTime *end,
							       gint *days,
							       gint *hours,
							       gint *minutes);
static void e_meeting_time_selector_find_nearest_interval (EMeetingTimeSelector *mts,
							   EMeetingTime *start_time,
							   EMeetingTime *end_time,
							   gint days, gint hours, gint mins);
static void e_meeting_time_selector_find_nearest_interval_backward (EMeetingTimeSelector *mts,
								    EMeetingTime *start_time,
								    EMeetingTime *end_time,
								    gint days, gint hours, gint mins);
static void e_meeting_time_selector_adjust_time (EMeetingTime *mtstime,
						 gint days, gint hours, gint minutes);
static EMeetingFreeBusyPeriod * e_meeting_time_selector_find_time_clash (EMeetingTimeSelector *mts,
									EMeetingAttendee *attendee,
									EMeetingTime *start_time,
									EMeetingTime *end_time);

static void e_meeting_time_selector_recalc_grid (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_recalc_date_format (EMeetingTimeSelector *mts);
static void e_meeting_time_selector_save_position (EMeetingTimeSelector *mts,
						   EMeetingTime *mtstime);
static void e_meeting_time_selector_restore_position (EMeetingTimeSelector *mts,
						      EMeetingTime *mtstime);
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
static gboolean e_meeting_time_selector_on_canvas_scroll_event (GtkWidget *widget, GdkEventScroll *event, EMeetingTimeSelector *mts);
static gboolean e_meeting_time_selector_on_canvas_query_tooltip (GtkWidget *widget,
                                                                 gint x,
                                                                 gint y,
                                                                 gboolean keyboard_mode,
                                                                 GtkTooltip *tooltip,
                                                                 gpointer user_data);

static void row_inserted_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void row_changed_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void row_deleted_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data);

static void free_busy_schedule_refresh_cb (EMeetingTimeSelector *mts);

G_DEFINE_TYPE_WITH_CODE (EMeetingTimeSelector, e_meeting_time_selector, GTK_TYPE_GRID,
	G_ADD_PRIVATE (EMeetingTimeSelector)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
meeting_time_selector_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_USE_24_HOUR_FORMAT:
			e_meeting_time_selector_set_use_24_hour_format (
				E_MEETING_TIME_SELECTOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
meeting_time_selector_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_USE_24_HOUR_FORMAT:
			g_value_set_boolean (
				value,
				e_meeting_time_selector_get_use_24_hour_format (
				E_MEETING_TIME_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
meeting_time_selector_dispose (GObject *object)
{
	EMeetingTimeSelector *mts;

	mts = E_MEETING_TIME_SELECTOR (object);

	e_meeting_time_selector_remove_timeout (mts);

	if (mts->model) {
		g_signal_handlers_disconnect_matched (
			mts->model, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, mts);
		e_signal_disconnect_notify_handler (mts->model, &mts->priv->notify_free_busy_template_id);
		e_signal_disconnect_notify_handler (mts->model, &mts->priv->notify_timezone_id);

		g_object_unref (mts->model);
		mts->model = NULL;
	}

	mts->display_top = NULL;
	mts->display_main = NULL;

	if (mts->fb_refresh_not != 0) {
		g_source_remove (mts->fb_refresh_not);
		mts->fb_refresh_not = 0;
	}

	if (mts->style_change_idle_id != 0) {
		g_source_remove (mts->style_change_idle_id);
		mts->style_change_idle_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_meeting_time_selector_parent_class)->dispose (object);
}

static void
e_meeting_time_selector_class_init (EMeetingTimeSelectorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = meeting_time_selector_set_property;
	object_class->get_property = meeting_time_selector_get_property;
	object_class->dispose = meeting_time_selector_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = e_meeting_time_selector_realize;
	widget_class->unrealize = e_meeting_time_selector_unrealize;
	widget_class->style_updated = e_meeting_time_selector_style_updated;
	widget_class->draw = e_meeting_time_selector_draw;

	g_object_class_install_property (
		object_class,
		PROP_USE_24_HOUR_FORMAT,
		g_param_spec_boolean (
			"use-24-hour-format",
			"Use 24-Hour Format",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMeetingTimeSelectorClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_meeting_time_selector_init (EMeetingTimeSelector *mts)
{
	GDateWeekday weekday;

	mts->priv = e_meeting_time_selector_get_instance_private (mts);

	/* The shadow is drawn in the border so it must be >= 2 pixels. */
	gtk_container_set_border_width (GTK_CONTAINER (mts), 2);

	mts->accel_group = gtk_accel_group_new ();

	mts->working_hours_only = TRUE;

	for (weekday = G_DATE_BAD_WEEKDAY; weekday <= G_DATE_SUNDAY; weekday++) {
		mts->day_start_hour[weekday] = 9;
		mts->day_start_minute[weekday] = 0;
		mts->day_end_hour[weekday] = 18;
		mts->day_end_minute[weekday] = 0;
	}

	mts->zoomed_out = FALSE;
	mts->dragging_position = E_MEETING_TIME_SELECTOR_POS_NONE;

	mts->list_view = NULL;

	mts->fb_refresh_not = 0;
	mts->style_change_idle_id = 0;

	e_extensible_load_extensions (E_EXTENSIBLE (mts));
}

void
e_meeting_time_selector_construct (EMeetingTimeSelector *mts,
                                   EMeetingStore *ems)
{
	GtkWidget *hbox, *vbox, *separator, *label, *grid, *sw;
	GtkWidget *child_hbox, *arrow, *menuitem;
	GtkWidget *child;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	GSList *group;
	guint accel_key;
	time_t meeting_start_time;
	struct tm *meeting_start_tm;
	AtkObject *a11y_label, *a11y_date_edit;

	/* The default meeting time is the nearest half-hour interval in the
	 * future, in working hours. */
	meeting_start_time = time (NULL);
	g_date_clear (&mts->meeting_start_time.date, 1);
	g_date_set_time_t (&mts->meeting_start_time.date, meeting_start_time);
	meeting_start_tm = localtime (&meeting_start_time);
	mts->meeting_start_time.hour = meeting_start_tm->tm_hour;
	mts->meeting_start_time.minute = meeting_start_tm->tm_min;

	e_meeting_time_selector_find_nearest_interval (
		mts, &mts->meeting_start_time,
		&mts->meeting_end_time,
		0, 0, 30);

	e_meeting_time_selector_update_dates_shown (mts);

	mts->meeting_positions_valid = FALSE;

	mts->row_height = 17;
	mts->col_width = 55;
	mts->day_width = 55 * 24 + 1;

	mts->auto_scroll_timeout_id = 0;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_end (vbox, 4);
	gtk_grid_attach (
		GTK_GRID (mts),
		vbox, 0, 0, 1, 2);
	gtk_widget_show (vbox);

	mts->attendees_vbox_spacer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), mts->attendees_vbox_spacer, FALSE, FALSE, 0);
	gtk_widget_show (mts->attendees_vbox_spacer);

	mts->attendees_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), mts->attendees_vbox, TRUE, TRUE, 0);
	gtk_widget_show (mts->attendees_vbox);

	/* build the etable */
	mts->model = ems;

	if (mts->model)
		g_object_ref (mts->model);

	mts->priv->notify_free_busy_template_id = e_signal_connect_notify_swapped (
		mts->model, "notify::free-busy-template",
		G_CALLBACK (free_busy_schedule_refresh_cb), mts);
	mts->priv->notify_timezone_id = e_signal_connect_notify_swapped (
		mts->model, "notify::timezone",
		G_CALLBACK (free_busy_schedule_refresh_cb), mts);

	g_signal_connect (
		mts->model, "row_inserted",
		G_CALLBACK (row_inserted_cb), mts);
	g_signal_connect (
		mts->model, "row_changed",
		G_CALLBACK (row_changed_cb), mts);
	g_signal_connect (
		mts->model, "row_deleted",
		G_CALLBACK (row_deleted_cb), mts);

	mts->list_view = e_meeting_list_view_new (mts->model);
	e_meeting_list_view_column_set_visible (mts->list_view, E_MEETING_STORE_ROLE_COL, FALSE);
	e_meeting_list_view_column_set_visible (mts->list_view, E_MEETING_STORE_RSVP_COL, FALSE);
	e_meeting_list_view_column_set_visible (mts->list_view, E_MEETING_STORE_STATUS_COL, FALSE);
	e_meeting_list_view_column_set_visible (mts->list_view, E_MEETING_STORE_TYPE_COL, FALSE);

	gtk_widget_show (GTK_WIDGET (mts->list_view));

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_margin_top (sw, 4);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_widget_set_child_visible (
		gtk_scrolled_window_get_vscrollbar (
		GTK_SCROLLED_WINDOW (sw)), FALSE);
	gtk_widget_show (sw);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (mts->list_view));

#if 0
	/* FIXME: do we need sorting here */
	g_signal_connect (
		real_table->sort_info, "sort_info_changed",
		G_CALLBACK (sort_info_changed_cb), mts);
#endif

	gtk_box_pack_start (GTK_BOX (mts->attendees_vbox), GTK_WIDGET (sw), TRUE, TRUE, 0);

	/* The free/busy information */
	mts->display_top = gnome_canvas_new ();
	gtk_widget_set_size_request (mts->display_top, -1, mts->row_height * 3);
	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (mts->display_top),
		0, 0,
		mts->day_width * E_MEETING_TIME_SELECTOR_DAYS_SHOWN,
		mts->row_height * 3);
	/* Add some horizontal padding for the shadow around the display. */
	gtk_grid_attach (
		GTK_GRID (mts), mts->display_top,
		1, 0, 3, 1);
	gtk_widget_show (mts->display_top);
	g_signal_connect (
		mts->display_top, "realize",
		G_CALLBACK (e_meeting_time_selector_on_canvas_realized), mts);

	mts->display_main = gnome_canvas_new ();
	gtk_widget_set_hexpand (mts->display_main, TRUE);
	gtk_widget_set_vexpand (mts->display_main, TRUE);
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);
	/* Add some horizontal padding for the shadow around the display. */
	gtk_grid_attach (
		GTK_GRID (mts), mts->display_main,
		1, 1, 3, 1);
	gtk_widget_show (mts->display_main);
	g_signal_connect (
		mts->display_main, "realize",
		G_CALLBACK (e_meeting_time_selector_on_canvas_realized), mts);
	g_signal_connect (
		mts->display_main, "size_allocate",
		G_CALLBACK (e_meeting_time_selector_on_canvas_size_allocate), mts);
	g_signal_connect (
		mts->display_main, "scroll-event",
		G_CALLBACK (e_meeting_time_selector_on_canvas_scroll_event), mts);
	/* used for displaying extended free/busy (XFB) display when hovering
	 * over a busy period which carries XFB information */
	g_signal_connect (
		mts->display_main, "query-tooltip",
		G_CALLBACK (e_meeting_time_selector_on_canvas_query_tooltip),
		mts);
	g_object_set (
		G_OBJECT (mts->display_main), "has-tooltip", TRUE, NULL);

	scrollable = GTK_SCROLLABLE (mts->display_main);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	gtk_scrolled_window_set_vadjustment (
		GTK_SCROLLED_WINDOW (sw), adjustment);

	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	mts->hscrollbar = gtk_scrollbar_new (
		GTK_ORIENTATION_HORIZONTAL, adjustment);
	gtk_adjustment_set_step_increment (adjustment, mts->day_width);
	gtk_grid_attach (
		GTK_GRID (mts), mts->hscrollbar,
		1, 2, 3, 1);
	gtk_widget_show (mts->hscrollbar);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	mts->vscrollbar = gtk_scrollbar_new (
		GTK_ORIENTATION_VERTICAL, adjustment);
	gtk_adjustment_set_step_increment (adjustment, mts->row_height);
	gtk_grid_attach (
		GTK_GRID (mts), mts->vscrollbar,
		4, 1, 1, 1);
	gtk_widget_show (mts->vscrollbar);

	/* Create the item in the top canvas. */
	mts->item_top = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (GNOME_CANVAS (mts->display_top)->root),
		e_meeting_time_selector_item_get_type (),
		"EMeetingTimeSelectorItem::meeting_time_selector", mts,
		NULL);

	/* Create the item in the main canvas. */
	mts->item_main = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (GNOME_CANVAS (mts->display_main)->root),
		e_meeting_time_selector_item_get_type (),
		"EMeetingTimeSelectorItem::meeting_time_selector", mts,
		NULL);

	/* Create the hbox containing the color key. */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
	gtk_widget_set_halign (hbox, GTK_ALIGN_START);
	gtk_grid_attach (
		GTK_GRID (mts), hbox,
		1, 3, 4, 1);
	gtk_widget_show (hbox);

	e_meeting_time_selector_add_key_color (mts, hbox, _("Tentative"), &mts->busy_colors[E_MEETING_FREE_BUSY_TENTATIVE]);
	e_meeting_time_selector_add_key_color (mts, hbox, _("Busy"), &mts->busy_colors[E_MEETING_FREE_BUSY_BUSY]);
	e_meeting_time_selector_add_key_color (mts, hbox, _("Out of Office"), &mts->busy_colors[E_MEETING_FREE_BUSY_OUT_OF_OFFICE]);
	e_meeting_time_selector_add_key_color (
		mts, hbox, _("No Information"),
		NULL);

	separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_margin_top (separator, 12);
	gtk_widget_set_margin_bottom (separator, 12);
	gtk_grid_attach (
		GTK_GRID (mts), separator,
		0, 4, 5, 1);
	gtk_widget_show (separator);

	/* Create the Invite Others & Options buttons on the left. */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_end (hbox, 6);
	gtk_grid_attach (
		GTK_GRID (mts), hbox,
		0, 3, 1, 1);
	gtk_widget_show (hbox);

	mts->add_attendees_button = e_dialog_button_new_with_icon ("go-jump", _("Atte_ndees…"));
	gtk_box_pack_start (GTK_BOX (hbox), mts->add_attendees_button, TRUE, TRUE, 0);
	gtk_widget_show (mts->add_attendees_button);
	g_signal_connect (
		mts->add_attendees_button, "clicked",
		G_CALLBACK (e_meeting_time_selector_on_invite_others_button_clicked), mts);

	mts->options_button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), mts->options_button, TRUE, TRUE, 0);
	gtk_widget_show (mts->options_button);

	g_signal_connect (
		mts->options_button, "clicked",
		G_CALLBACK (e_meeting_time_selector_on_options_button_clicked), mts);

	child_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add (GTK_CONTAINER (mts->options_button), child_hbox);
	gtk_widget_show (child_hbox);

	label = gtk_label_new_with_mnemonic (_("O_ptions"));
	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 6);
	gtk_widget_show (label);
	gtk_widget_add_accelerator (
		mts->options_button, "clicked", mts->accel_group,
		accel_key, GDK_MOD1_MASK, 0);

	arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (child_hbox), arrow, FALSE, FALSE, 6);
	gtk_widget_show (arrow);

	/* Create the Options menu. */
	mts->options_menu = gtk_menu_new ();
	g_object_set_data (G_OBJECT (mts->options_menu), "EMeetingTimeSelector", mts);
	gtk_menu_attach_to_widget (
		GTK_MENU (mts->options_menu), mts->options_button,
		e_meeting_time_selector_options_menu_detacher);

	menuitem = gtk_check_menu_item_new_with_label ("");
	child = gtk_bin_get_child (GTK_BIN (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _("Show _only working hours"));
	gtk_menu_shell_append (GTK_MENU_SHELL (mts->options_menu), menuitem);
	gtk_check_menu_item_set_active (
		GTK_CHECK_MENU_ITEM (menuitem),
		mts->working_hours_only);

	g_signal_connect (
		menuitem, "toggled",
		G_CALLBACK (e_meeting_time_selector_on_working_hours_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_check_menu_item_new_with_label ("");
	child = gtk_bin_get_child (GTK_BIN (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _("Show _zoomed out"));
	gtk_menu_shell_append (GTK_MENU_SHELL (mts->options_menu), menuitem);
	gtk_check_menu_item_set_active (
		GTK_CHECK_MENU_ITEM (menuitem),
		mts->zoomed_out);

	g_signal_connect (
		menuitem, "toggled",
		G_CALLBACK (e_meeting_time_selector_on_zoomed_out_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (mts->options_menu), menuitem);
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);

	menuitem = gtk_menu_item_new_with_label ("");
	child = gtk_bin_get_child (GTK_BIN (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _("_Update free/busy"));
	gtk_menu_shell_append (GTK_MENU_SHELL (mts->options_menu), menuitem);

	g_signal_connect (
		menuitem, "activate",
		G_CALLBACK (e_meeting_time_selector_on_update_free_busy), mts);
	gtk_widget_show (menuitem);

	/* Create the 3 AutoPick buttons on the left. */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_end (hbox, 6);
	gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
	gtk_grid_attach (
		GTK_GRID (mts), hbox,
		0, 5, 1, 1);
	gtk_widget_show (hbox);

	mts->autopick_down_button = gtk_button_new_with_label ("");
	child = gtk_bin_get_child (GTK_BIN (mts->autopick_down_button));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _("_<<"));
	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (child));
	gtk_widget_add_accelerator (
		mts->autopick_down_button, "clicked", mts->accel_group,
		accel_key, GDK_MOD1_MASK | GDK_SHIFT_MASK, 0);
	gtk_box_pack_start (GTK_BOX (hbox), mts->autopick_down_button, TRUE, TRUE, 0);
	gtk_widget_show (mts->autopick_down_button);
	g_signal_connect (
		mts->autopick_down_button, "clicked",
		G_CALLBACK (e_meeting_time_selector_on_prev_button_clicked), mts);

	mts->autopick_button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), mts->autopick_button, TRUE, TRUE, 0);
	gtk_widget_show (mts->autopick_button);

	child_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add (GTK_CONTAINER (mts->autopick_button), child_hbox);
	gtk_widget_show (child_hbox);

	label = gtk_label_new_with_mnemonic (_("_Autopick"));
	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gtk_widget_add_accelerator (
		mts->autopick_button, "clicked", mts->accel_group,
		accel_key, GDK_MOD1_MASK, 0);
	g_signal_connect (
		mts->autopick_button, "clicked",
		G_CALLBACK (e_meeting_time_selector_on_autopick_button_clicked), mts);

	arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (child_hbox), arrow, FALSE, FALSE, 6);
	gtk_widget_show (arrow);

	mts->autopick_up_button = gtk_button_new_with_label ("");
	child = gtk_bin_get_child (GTK_BIN (mts->autopick_up_button));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _(">_>"));
	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (child));
	gtk_widget_add_accelerator (
		mts->autopick_up_button, "clicked", mts->accel_group,
		accel_key, GDK_MOD1_MASK | GDK_SHIFT_MASK, 0);
	gtk_box_pack_start (GTK_BOX (hbox), mts->autopick_up_button, TRUE, TRUE, 0);
	gtk_widget_show (mts->autopick_up_button);
	g_signal_connect (
		mts->autopick_up_button, "clicked",
		G_CALLBACK (e_meeting_time_selector_on_next_button_clicked), mts);

	/* Create the Autopick menu. */
	mts->autopick_menu = gtk_menu_new ();
	g_object_set_data (G_OBJECT (mts->autopick_menu), "EMeetingTimeSelector", mts);
	gtk_menu_attach_to_widget (
		GTK_MENU (mts->autopick_menu), mts->autopick_button,
		e_meeting_time_selector_autopick_menu_detacher);

	menuitem = gtk_radio_menu_item_new_with_label (NULL, "");
	mts->autopick_all_item = menuitem;
	child = gtk_bin_get_child (GTK_BIN (menuitem));
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _("_All people and resources"));
	gtk_menu_shell_append (GTK_MENU_SHELL (mts->autopick_menu), menuitem);
	g_signal_connect (
		menuitem, "toggled",
		G_CALLBACK (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_all_people_one_resource_item = menuitem;
	child = gtk_bin_get_child (GTK_BIN (menuitem));
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _("All _people and one resource"));
	gtk_menu_shell_append (GTK_MENU_SHELL (mts->autopick_menu), menuitem);
	g_signal_connect (
		menuitem, "toggled",
		G_CALLBACK (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_required_people_item = menuitem;
	child = gtk_bin_get_child (GTK_BIN (menuitem));
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _("_Required people"));
	gtk_menu_shell_append (GTK_MENU_SHELL (mts->autopick_menu), menuitem);
	g_signal_connect (
		menuitem, "activate",
		G_CALLBACK (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	menuitem = gtk_radio_menu_item_new_with_label (group, "");
	mts->autopick_required_people_one_resource_item = menuitem;
	child = gtk_bin_get_child (GTK_BIN (menuitem));
	gtk_label_set_text_with_mnemonic (GTK_LABEL (child), _("Required people and _one resource"));
	gtk_menu_shell_append (GTK_MENU_SHELL (mts->autopick_menu), menuitem);
	g_signal_connect (
		menuitem, "activate",
		G_CALLBACK (e_meeting_time_selector_on_autopick_option_toggled), mts);
	gtk_widget_show (menuitem);

	/* Create the date entry fields on the right. */
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 4);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
	gtk_grid_attach (
		GTK_GRID (mts), grid,
		1, 5, 3, 1);
	gtk_widget_show (grid);

	mts->start_date_edit = e_date_edit_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), mts->start_date_edit);
	a11y_label = gtk_widget_get_accessible (label);
	a11y_date_edit = gtk_widget_get_accessible (mts->start_date_edit);
	if (a11y_label != NULL && a11y_date_edit != NULL) {
		atk_object_add_relationship (
			a11y_date_edit,
			ATK_RELATION_LABELLED_BY,
			a11y_label);
	}
	e_date_edit_set_show_time (E_DATE_EDIT (mts->start_date_edit), TRUE);

	gtk_grid_attach (GTK_GRID (grid), mts->start_date_edit, 1, 0, 1, 1);
	gtk_widget_show (mts->start_date_edit);
	g_signal_connect (
		mts->start_date_edit, "changed",
		G_CALLBACK (e_meeting_time_selector_on_start_time_changed), mts);

	label = gtk_label_new_with_mnemonic (_("_Start time:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), (mts->start_date_edit));

	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	gtk_widget_show (label);

	mts->end_date_edit = e_date_edit_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), mts->end_date_edit);
	a11y_label = gtk_widget_get_accessible (label);
	a11y_date_edit = gtk_widget_get_accessible (mts->end_date_edit);
	if (a11y_label != NULL && a11y_date_edit != NULL) {
		atk_object_add_relationship (
			a11y_date_edit,
					ATK_RELATION_LABELLED_BY,
					a11y_label);
	}
	e_date_edit_set_show_time (E_DATE_EDIT (mts->end_date_edit), TRUE);

	gtk_grid_attach (GTK_GRID (grid), mts->end_date_edit, 1, 1, 1, 1);
	gtk_widget_show (mts->end_date_edit);
	g_signal_connect (
		mts->end_date_edit, "changed",
		G_CALLBACK (e_meeting_time_selector_on_end_time_changed), mts);

	label = gtk_label_new_with_mnemonic (_("_End time:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), (mts->end_date_edit));

	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
	gtk_widget_show (label);

	/* Allocate the colors. */
	e_meeting_time_selector_alloc_named_color (mts, "snow", &mts->bg_color);
	e_meeting_time_selector_alloc_named_color (mts, "snow3", &mts->all_attendees_bg_color);
	e_meeting_time_selector_alloc_named_color (mts, "black", &mts->grid_color);
	e_meeting_time_selector_alloc_named_color (mts, "white", &mts->grid_shadow_color);
	e_meeting_time_selector_alloc_named_color (mts, "gray50", &mts->grid_unused_color);
	e_meeting_time_selector_alloc_named_color (mts, "white", &mts->attendee_list_bg_color);

	e_meeting_time_selector_alloc_named_color (mts, "snow4", &mts->meeting_time_bg_color);
	e_meeting_time_selector_alloc_named_color (mts, "snow", &mts->busy_colors[E_MEETING_FREE_BUSY_FREE]);
	e_meeting_time_selector_alloc_named_color (mts, "#a5d3ef", &mts->busy_colors[E_MEETING_FREE_BUSY_TENTATIVE]);
	e_meeting_time_selector_alloc_named_color (mts, "blue", &mts->busy_colors[E_MEETING_FREE_BUSY_BUSY]);
	e_meeting_time_selector_alloc_named_color (mts, "#ce6194", &mts->busy_colors[E_MEETING_FREE_BUSY_OUT_OF_OFFICE]);

	/* Connect handlers to the adjustments  scroll the other items. */
	scrollable = GTK_SCROLLABLE (mts->display_main);
	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	g_signal_connect (
		adjustment, "value_changed",
		G_CALLBACK (e_meeting_time_selector_hadjustment_changed), mts);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	g_signal_connect (
		adjustment, "value_changed",
		G_CALLBACK (e_meeting_time_selector_vadjustment_changed), mts);
	g_signal_connect (
		adjustment, "changed",
		G_CALLBACK (e_meeting_time_selector_vadjustment_changed), mts);

	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
	e_meeting_time_selector_update_start_date_edit (mts);
	e_meeting_time_selector_update_end_date_edit (mts);
	e_meeting_time_selector_update_date_popup_menus (mts);

	g_signal_emit (mts, signals[CHANGED], 0);
}

/* This adds a color to the color key beneath the main display. If color is
 * NULL, it displays the No Info pattern instead. */
static void
e_meeting_time_selector_add_key_color (EMeetingTimeSelector *mts,
                                       GtkWidget *hbox,
                                       gchar *label_text,
                                       GdkRGBA *color)
{
	GtkWidget *child_hbox, *darea, *label;

	child_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (GTK_BOX (hbox), child_hbox, TRUE, TRUE, 0);
	gtk_widget_show (child_hbox);

	darea = gtk_drawing_area_new ();
	gtk_box_pack_start (GTK_BOX (child_hbox), darea, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (darea), "data", mts);
	gtk_widget_set_size_request (darea, 14, 14);
	gtk_widget_show (darea);

	label = gtk_label_new (label_text);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_box_pack_start (GTK_BOX (child_hbox), label, TRUE, TRUE, 6);
	gtk_widget_show (label);

	g_signal_connect (
		darea, "draw",
		G_CALLBACK (e_meeting_time_selector_draw_key_color), color);
}

static gint
e_meeting_time_selector_draw_key_color (GtkWidget *darea,
                                        cairo_t *cr,
                                        GdkRGBA *color)
{
	EMeetingTimeSelector * mts;
	GtkAllocation allocation;
	GtkStyleContext *style_context;

	mts = g_object_get_data (G_OBJECT (darea), "data");

	style_context = gtk_widget_get_style_context (darea);

	gtk_widget_get_allocation (darea, &allocation);

	gtk_render_frame (
		style_context, cr,
		(gdouble) 0,
		(gdouble) 0,
		(gdouble) allocation.width,
		(gdouble) allocation.height);

	if (color) {
		gdk_cairo_set_source_rgba (cr, color);
	} else {
		cairo_set_source (cr, mts->no_info_pattern);
	}
	cairo_rectangle (
		cr,
		1, 1,
		allocation.width - 2, allocation.height - 2);
	cairo_fill (cr);

	return TRUE;
}

static void
e_meeting_time_selector_alloc_named_color (EMeetingTimeSelector *mts,
                                           const gchar *name,
                                           GdkRGBA *c)
{
	g_return_if_fail (name != NULL);
	g_return_if_fail (c != NULL);

	if (!gdk_rgba_parse (c, name))
		g_warning ("Failed to parse color: %s\n", name);
}

static void
e_meeting_time_selector_options_menu_detacher (GtkWidget *widget,
                                               GtkMenu *menu)
{
	EMeetingTimeSelector *mts;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_MENU (menu));

	mts = g_object_get_data (G_OBJECT (menu), "EMeetingTimeSelector");

	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (mts));
	g_return_if_fail (mts->options_menu == (GtkWidget *) menu);

	mts->options_menu = NULL;
}

static void
e_meeting_time_selector_autopick_menu_detacher (GtkWidget *widget,
                                                GtkMenu *menu)
{
	EMeetingTimeSelector *mts;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_MENU (menu));

	mts = g_object_get_data (G_OBJECT (menu), "EMeetingTimeSelector");

	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (mts));
	g_return_if_fail (mts->autopick_menu == (GtkWidget *) menu);

	mts->autopick_menu = NULL;
}

GtkWidget *
e_meeting_time_selector_new (EMeetingStore *ems)
{
	GtkWidget *mts;

	mts = g_object_new (E_TYPE_MEETING_TIME_SELECTOR, NULL);

	e_meeting_time_selector_construct (E_MEETING_TIME_SELECTOR (mts), ems);

	return mts;
}

gboolean
e_meeting_time_selector_get_use_24_hour_format (EMeetingTimeSelector *mts)
{
	g_return_val_if_fail (E_IS_MEETING_TIME_SELECTOR (mts), FALSE);

	return mts->priv->use_24_hour_format;
}

void
e_meeting_time_selector_set_use_24_hour_format (EMeetingTimeSelector *mts,
                                                gboolean use_24_hour_format)
{
	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (mts));

	if (mts->priv->use_24_hour_format == use_24_hour_format)
		return;

	mts->priv->use_24_hour_format = use_24_hour_format;

	g_object_notify (G_OBJECT (mts), "use-24-hour-format");
}

static cairo_pattern_t *
e_meeting_time_selector_create_no_info_pattern (EMeetingTimeSelector *mts)
{
	cairo_surface_t *surface;
	cairo_pattern_t *pattern;
	GdkRGBA color = { .red = 1.0, .green = 1.0, .blue = 1.0, .alpha = 1.0 };
	cairo_t *cr;

	surface = gdk_window_create_similar_surface (
		gtk_widget_get_window (GTK_WIDGET (mts)),
		CAIRO_CONTENT_COLOR, 8, 8);
	cr = cairo_create (surface);

	gdk_cairo_set_source_rgba (cr, &color);
	cairo_paint (cr);

	gdk_cairo_set_source_rgba (cr, &mts->grid_color);
	cairo_set_line_width (cr, 1.0);
	cairo_move_to (cr, -1,  5);
	cairo_line_to (cr,  9, -5);
	cairo_move_to (cr, -1, 13);
	cairo_line_to (cr,  9,  3);
	cairo_stroke (cr);

	cairo_destroy (cr);

	pattern = cairo_pattern_create_for_surface (surface);
	cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

	cairo_surface_destroy (surface);

	return pattern;
}

static void
e_meeting_time_selector_realize (GtkWidget *widget)
{
	EMeetingTimeSelector *mts;

	if (GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->realize)
		(*GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->realize)(widget);

	mts = E_MEETING_TIME_SELECTOR (widget);

	mts->no_info_pattern = e_meeting_time_selector_create_no_info_pattern (mts);
}

static void
e_meeting_time_selector_unrealize (GtkWidget *widget)
{
	EMeetingTimeSelector *mts;

	mts = E_MEETING_TIME_SELECTOR (widget);

	cairo_pattern_destroy (mts->no_info_pattern);
	mts->no_info_pattern = NULL;

	if (GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->unrealize)(widget);
}

static gint
get_cell_height (GtkTreeView *tree)
{
	GtkTreeViewColumn *column;
	gint height = -1;

	column = gtk_tree_view_get_column (tree, 0);
	gtk_tree_view_column_cell_get_size (
		column, NULL,
		NULL, NULL,
		NULL, &height);

	return height;
}

static gboolean
style_change_idle_func (EMeetingTimeSelector *mts)
{
	EMeetingTime saved_time;
	GtkAdjustment *adjustment;
	GtkWidget *widget;
	gint hour, max_hour_width;
	/*int maxheight;      */
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;

	/* Set up Pango prerequisites */
	widget = GTK_WIDGET (mts);
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, NULL,
		pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	/* Calculate the widths of the hour strings in the style's font. */
	max_hour_width = 0;
	for (hour = 0; hour < 24; hour++) {
		if (e_meeting_time_selector_get_use_24_hour_format (mts))
			pango_layout_set_text (layout, EMeetingTimeSelectorHours[hour], -1);
		else
			pango_layout_set_text (layout, EMeetingTimeSelectorHours12[hour], -1);

		pango_layout_get_pixel_size (layout, &mts->hour_widths[hour], NULL);
		max_hour_width = MAX (max_hour_width, mts->hour_widths[hour]);
	}

	/* add also some padding for lines so it fits better */
	mts->row_height = get_cell_height (GTK_TREE_VIEW (mts->list_view)) + 2;
	mts->col_width = max_hour_width + 6;

	e_meeting_time_selector_save_position (mts, &saved_time);
	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_restore_position (mts, &saved_time);

	gtk_widget_set_size_request (mts->display_top, -1, mts->row_height * 3 + 4);

	/*
	 * FIXME: I can't find a way to get the treeview header heights
	 * other than the below but it isn't nice to realize that widget here
	 *
 *
	gtk_widget_realize (mts->list_view);
	gdk_window_get_position (
		gtk_tree_view_get_bin_window (GTK_TREE_VIEW (mts->list_view)),
		NULL, &maxheight);
	gtk_widget_set_size_request (mts->attendees_vbox_spacer, 1, mts->row_height * 3 - maxheight);
 *
	*/

	gtk_widget_set_size_request (mts->attendees_vbox_spacer, 1, mts->row_height * 2 - 6);

	widget = mts->display_main;

	adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (widget));
	gtk_adjustment_set_step_increment (adjustment, mts->day_width);

	adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (widget));
	gtk_adjustment_set_step_increment (adjustment, mts->row_height);

	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);

	mts->style_change_idle_id = 0;

	return FALSE;
}

static void
e_meeting_time_selector_style_updated (GtkWidget *widget)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (widget);

	if (GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->style_updated)
		(*GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->style_updated) (widget);

	if (!mts->style_change_idle_id)
		mts->style_change_idle_id = g_idle_add (
			(GSourceFunc) style_change_idle_func, widget);
}

/* This draws a shadow around the top display and main display. */
static gint
e_meeting_time_selector_draw (GtkWidget *widget,
                              cairo_t *cr)
{
	EMeetingTimeSelector *mts;

	mts = E_MEETING_TIME_SELECTOR (widget);

	e_meeting_time_selector_draw_shadow (mts, cr);

	if (GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->draw)
		(*GTK_WIDGET_CLASS (e_meeting_time_selector_parent_class)->draw)(widget, cr);

	return FALSE;
}

static void
e_meeting_time_selector_draw_shadow (EMeetingTimeSelector *mts,
                                     cairo_t *cr)
{
	GtkAllocation allocation;
	GtkStyleContext *style_context;

	style_context = gtk_widget_get_style_context (GTK_WIDGET (mts));

	/* Draw the shadow around the graphical displays. */
	gtk_widget_get_allocation (mts->display_top, &allocation);

	cairo_save (cr);

	gtk_render_frame (
		style_context, cr,
		(gdouble) allocation.x - 2,
		(gdouble) allocation.y - 2,
		(gdouble) allocation.width + 4,
		(gdouble) allocation.height + allocation.height + 4);

	cairo_restore (cr);
}

/* When the main canvas scrolls, we scroll the other canvases. */
static void
e_meeting_time_selector_hadjustment_changed (GtkAdjustment *adjustment,
                                             EMeetingTimeSelector *mts)
{
	GtkAdjustment *hadjustment;
	GtkScrollable *scrollable;
	gdouble value;

	scrollable = GTK_SCROLLABLE (mts->display_top);
	hadjustment = gtk_scrollable_get_hadjustment (scrollable);

	value = gtk_adjustment_get_value (adjustment);
	gtk_adjustment_set_value (hadjustment, value);
}

static void
e_meeting_time_selector_vadjustment_changed (GtkAdjustment *adjustment,
                                             EMeetingTimeSelector *mts)
{
	GtkAdjustment *vadjustment;
	GtkScrollable *scrollable;
	gdouble value;

	scrollable = GTK_SCROLLABLE (mts->list_view);
	vadjustment = gtk_scrollable_get_vadjustment (scrollable);

	value = gtk_adjustment_get_value (adjustment);
	gtk_adjustment_set_value (vadjustment, value);
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
	*start_year = g_date_get_year (&mts->meeting_start_time.date);
	*start_month = g_date_get_month (&mts->meeting_start_time.date);
	*start_day = g_date_get_day (&mts->meeting_start_time.date);
	*start_hour = mts->meeting_start_time.hour;
	*start_minute = mts->meeting_start_time.minute;

	*end_year = g_date_get_year (&mts->meeting_end_time.date);
	*end_month = g_date_get_month (&mts->meeting_end_time.date);
	*end_day = g_date_get_day (&mts->meeting_end_time.date);
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
	g_return_val_if_fail (E_IS_MEETING_TIME_SELECTOR (mts), FALSE);

	/* Check the dates are valid. */
	if (!g_date_valid_dmy (start_day, start_month, start_year)
	    || !g_date_valid_dmy (end_day, end_month, end_year)
	    || start_hour < 0 || start_hour > 23
	    || end_hour < 0 || end_hour > 23
	    || start_minute < 0 || start_minute > 59
	    || end_minute < 0 || end_minute > 59)
		return FALSE;

	g_date_set_dmy (
		&mts->meeting_start_time.date, start_day, start_month,
			start_year);
	mts->meeting_start_time.hour = start_hour;
	mts->meeting_start_time.minute = start_minute;
	g_date_set_dmy (
		&mts->meeting_end_time.date,
		end_day, end_month, end_year);
	mts->meeting_end_time.hour = end_hour;
	mts->meeting_end_time.minute = end_minute;

	mts->meeting_positions_valid = FALSE;

	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);

	/* Set the times in the EDateEdit widgets. */
	e_meeting_time_selector_update_start_date_edit (mts);
	e_meeting_time_selector_update_end_date_edit (mts);

	g_signal_emit (mts, signals[CHANGED], 0);

	return TRUE;
}

void
e_meeting_time_selector_set_all_day (EMeetingTimeSelector *mts,
                                     gboolean all_day)
{
	EMeetingTime saved_time;

	mts->all_day = all_day;

	e_date_edit_set_show_time (
		E_DATE_EDIT (mts->start_date_edit),
		!all_day);
	e_date_edit_set_show_time (
		E_DATE_EDIT (mts->end_date_edit),
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

	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (mts));

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
					   GDateWeekday for_weekday,
                                           gint day_start_hour,
                                           gint day_start_minute,
                                           gint day_end_hour,
                                           gint day_end_minute)
{
	EMeetingTime saved_time;

	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (mts));
	g_return_if_fail (for_weekday == G_DATE_MONDAY ||
			  for_weekday == G_DATE_TUESDAY ||
			  for_weekday == G_DATE_WEDNESDAY ||
			  for_weekday == G_DATE_THURSDAY ||
			  for_weekday == G_DATE_FRIDAY ||
			  for_weekday == G_DATE_SATURDAY ||
			  for_weekday == G_DATE_SUNDAY ||
			  for_weekday == G_DATE_BAD_WEEKDAY);

	if (mts->day_start_hour[for_weekday] == day_start_hour
	    && mts->day_start_minute[for_weekday] == day_start_minute
	    && mts->day_end_hour[for_weekday] == day_end_hour
	    && mts->day_end_minute[for_weekday] == day_end_minute)
		return;

	mts->day_start_hour[for_weekday] = day_start_hour;
	mts->day_start_minute[for_weekday] = day_start_minute;

	/* Make sure we always show atleast an hour */
	if (day_start_hour * 60 + day_start_minute + 60 < day_end_hour * 60 + day_end_minute) {
		mts->day_end_hour[for_weekday] = day_end_hour;
		mts->day_end_minute[for_weekday] = day_end_minute;
	} else {
		mts->day_end_hour[for_weekday] = day_start_hour + 1;
		mts->day_end_minute[for_weekday] = day_start_minute;
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

	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (mts));

	if (mts->zoomed_out == zoomed_out)
		return;

	mts->zoomed_out = zoomed_out;

	e_meeting_time_selector_save_position (mts, &saved_time);
	e_meeting_time_selector_recalc_grid (mts);
	e_meeting_time_selector_restore_position (mts, &saved_time);

	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
}

static gboolean
e_meeting_time_selector_refresh_cb (gpointer data)
{
	EMeetingTimeSelector *mts = data;

	if (!mts->model) {
		/* Destroyed, do not do anything */
		g_object_unref (mts);
		return FALSE;
	}

	if (e_meeting_store_get_num_queries (mts->model) == 0) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (mts)), "default");
		if (cursor) {
			GdkWindow *window;

			window = gtk_widget_get_window (GTK_WIDGET (mts));
			if (window)
				gdk_window_set_cursor (window, cursor);
			g_object_unref (cursor);
		}

		mts->last_cursor_set = GDK_LEFT_PTR;

		e_meeting_time_selector_item_set_normal_cursor (E_MEETING_TIME_SELECTOR_ITEM (mts->item_top));
		e_meeting_time_selector_item_set_normal_cursor (E_MEETING_TIME_SELECTOR_ITEM (mts->item_main));
	}

	if (mts->display_top != NULL)
		gtk_widget_queue_draw (mts->display_top);
	if (mts->display_main != NULL)
		gtk_widget_queue_draw (mts->display_main);

	g_object_unref (mts);

	return FALSE;
}

void
e_meeting_time_selector_refresh_free_busy (EMeetingTimeSelector *mts,
                                           gint row,
                                           gboolean all)
{
	EMeetingTime start, end;

	/* nothing to refresh, lets not leak a busy cursor */
	if (e_meeting_store_count_actual_attendees (mts->model) <= 0)
		return;

	start = mts->meeting_start_time;
	g_date_subtract_days (&start.date, E_MEETING_TIME_SELECTOR_FB_DAYS_BEFORE);
	start.hour = 0;
	start.minute = 0;
	end = mts->meeting_end_time;
	g_date_add_days (&end.date, E_MEETING_TIME_SELECTOR_FB_DAYS_AFTER);
	end.hour = 0;
	end.minute = 0;

	/* XXX This function is called during schedule page initialization
	 *     before the meeting time selector is realized, meaning it has
	 *     no GdkWindow yet.  This avoids a runtime warning. */
	if (gtk_widget_get_realized (GTK_WIDGET (mts))) {
		GdkCursor *cursor;

		/* Set the cursor to Busy.  We need to reset it to
		 * normal once the free busy queries are complete. */
		cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (mts)), "wait");
		if (cursor) {
			GdkWindow *window;

			window = gtk_widget_get_window (GTK_WIDGET (mts));
			gdk_window_set_cursor (window, cursor);
			g_object_unref (cursor);
		}

		mts->last_cursor_set = GDK_WATCH;
	}

	/* Ref ourselves in case we are called back after destruction,
	 * we can do this because we will get a call back even after
	 * an error */
	/* FIXME We should really have a mechanism to unqueue the
	 * notification */
	if (all) {
		gint i;

		for (i = 0; i < e_meeting_store_count_actual_attendees (mts->model); i++)
			g_object_ref (mts);
	} else {
		g_object_ref (mts);
	}

	if (all)
		e_meeting_store_refresh_all_busy_periods (
			mts->model, &start, &end,
			e_meeting_time_selector_refresh_cb, mts);
	else
		e_meeting_store_refresh_busy_periods (
			mts->model, row, &start, &end,
			e_meeting_time_selector_refresh_cb, mts);
}

EMeetingTimeSelectorAutopickOption
e_meeting_time_selector_get_autopick_option (EMeetingTimeSelector *mts)
{
	GtkWidget *widget;

	widget = mts->autopick_all_item;
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
		return E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_RESOURCES;

	widget = mts->autopick_all_people_one_resource_item;
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
		return E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_ONE_RESOURCE;

	widget = mts->autopick_required_people_item;
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
		return E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE;

	return E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE_AND_ONE_RESOURCE;
}

void
e_meeting_time_selector_set_autopick_option (EMeetingTimeSelector *mts,
                                             EMeetingTimeSelectorAutopickOption autopick_option)
{
	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (mts));

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

void
e_meeting_time_selector_set_read_only (EMeetingTimeSelector *mts,
                                       gboolean read_only)
{
	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (mts));

	gtk_widget_set_sensitive (GTK_WIDGET (mts->list_view), !read_only);
	gtk_widget_set_sensitive (mts->display_main, !read_only);
	gtk_widget_set_sensitive (mts->add_attendees_button, !read_only);
	gtk_widget_set_sensitive (mts->autopick_down_button, !read_only);
	gtk_widget_set_sensitive (mts->autopick_button, !read_only);
	gtk_widget_set_sensitive (mts->autopick_up_button, !read_only);
	gtk_widget_set_sensitive (mts->start_date_edit, !read_only);
	gtk_widget_set_sensitive (mts->end_date_edit, !read_only);
}

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
	g_object_set (mts->options_menu,
	              "anchor-hints", (GDK_ANCHOR_FLIP_Y |
	                               GDK_ANCHOR_SLIDE |
	                               GDK_ANCHOR_RESIZE),
	              NULL);

	gtk_menu_popup_at_widget (GTK_MENU (mts->options_menu),
	                          mts->options_button,
	                          GDK_GRAVITY_SOUTH_WEST,
	                          GDK_GRAVITY_NORTH_WEST,
	                          NULL);
}

static void
e_meeting_time_selector_on_update_free_busy (GtkWidget *button,
                                             EMeetingTimeSelector *mts)
{
	/* Make sure the menu pops down, which doesn't happen by default if
	 * keyboard accelerators are used. */
	if (gtk_widget_get_visible (mts->options_menu))
		gtk_menu_popdown (GTK_MENU (mts->options_menu));

	e_meeting_time_selector_refresh_free_busy (mts, 0, TRUE);
}

static void
e_meeting_time_selector_on_autopick_button_clicked (GtkWidget *button,
                                                    EMeetingTimeSelector *mts)
{
	g_object_set (mts->autopick_menu,
	              "anchor-hints", (GDK_ANCHOR_FLIP_Y |
	                               GDK_ANCHOR_SLIDE |
	                               GDK_ANCHOR_RESIZE),
	              NULL);

	gtk_menu_popup_at_widget (GTK_MENU (mts->autopick_menu),
	                          mts->autopick_button,
	                          GDK_GRAVITY_SOUTH_WEST,
	                          GDK_GRAVITY_NORTH_WEST,
	                          NULL);
}

static void
e_meeting_time_selector_on_autopick_option_toggled (GtkWidget *button,
                                                    EMeetingTimeSelector *mts)
{
	/* Make sure the menu pops down, which doesn't happen by default if
	 * keyboard accelerators are used. */
	if (gtk_widget_get_visible (mts->autopick_menu))
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
 * attendees will be available. */
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
	 * resource based on the autopick option. */
	autopick_option = e_meeting_time_selector_get_autopick_option (mts);
	if (autopick_option == E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE
	    || autopick_option == E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE_AND_ONE_RESOURCE)
		skip_optional = TRUE;
	if (autopick_option == E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_ONE_RESOURCE
	    || autopick_option == E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE_AND_ONE_RESOURCE)
		need_one_resource = TRUE;

	/* Keep moving forward or backward until we find a possible meeting
	 * time. */
	for (;;) {
		meeting_time_ok = TRUE;
		found_resource = FALSE;
		resource_free = NULL;

		/* Step through each attendee, checking if the meeting time
		 * intersects one of the attendees busy periods. */
		for (row = 0; row < e_meeting_store_count_actual_attendees (mts->model); row++) {
			attendee = e_meeting_store_find_attendee_at_row (mts->model, row);

			/* Skip optional people if they don't matter. */
			if (skip_optional && e_meeting_attendee_get_atype (attendee) == E_MEETING_ATTENDEE_OPTIONAL_PERSON)
				continue;

			period = e_meeting_time_selector_find_time_clash (mts, attendee, &start_time, &end_time);

			if (need_one_resource && e_meeting_attendee_get_atype (attendee) == E_MEETING_ATTENDEE_RESOURCE) {
				if (period) {
					/* We want to remember the closest
					 * prev/next time that one resource is
					 * available, in case we don't find any
					 * free resources. */
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
		 * to the closest time that a resource is free. Note that if
		 * there are no resources, resource_free will never get set,
		 * so we assume the meeting time is OK. */
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

			g_signal_emit (mts, signals[CHANGED], 0);

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
e_meeting_time_selector_calculate_time_difference (EMeetingTime *start,
                                                   EMeetingTime *end,
                                                   gint *days,
                                                   gint *hours,
                                                   gint *minutes)
{
	*days = g_date_get_julian (&end->date) - g_date_get_julian (&start->date);
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

static GDateWeekday
e_meeting_time_selector_get_time_weekday (const EMeetingTime *time)
{
	GDateWeekday weekday;

	if (!time || !g_date_valid (&time->date))
		return G_DATE_BAD_WEEKDAY;

	weekday = g_date_get_weekday (&time->date);

	if (weekday < G_DATE_BAD_WEEKDAY || weekday > G_DATE_SUNDAY)
		weekday = G_DATE_BAD_WEEKDAY;

	return weekday;
}

/* This moves the given time forward to the next suitable start of a meeting.
 * If zoomed_out is set, this means every hour. If not every half-hour. */
static void
e_meeting_time_selector_find_nearest_interval (EMeetingTimeSelector *mts,
                                               EMeetingTime *start_time,
                                               EMeetingTime *end_time,
                                               gint days,
                                               gint hours,
                                               gint mins)
{
	GDateWeekday start_weekday, end_weekday;
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
	 * If it isn't we don't worry about the working day. */
	if (!mts->working_hours_only || days > 0)
		return;

	start_weekday = e_meeting_time_selector_get_time_weekday (start_time);
	end_weekday = e_meeting_time_selector_get_time_weekday (end_time);

	minutes_shown = (mts->day_end_hour[end_weekday] - mts->day_start_hour[start_weekday]) * 60;
	minutes_shown += mts->day_end_minute[end_weekday] - mts->day_start_minute[start_weekday];
	if (hours * 60 + mins > minutes_shown)
		return;

	/* If the meeting time finishes past the end of the working day, move
	 * onto the start of the next working day. If the meeting time starts
	 * before the working day, move it on as well. */
	if (start_time->hour > mts->day_end_hour[end_weekday]
	    || (start_time->hour == mts->day_end_hour[end_weekday]
		&& start_time->minute > mts->day_end_minute[end_weekday])
	    || end_time->hour > mts->day_end_hour[end_weekday]
	    || (end_time->hour == mts->day_end_hour[end_weekday]
		&& end_time->minute > mts->day_end_minute[end_weekday])) {
		g_date_add_days (&start_time->date, 1);
		set_to_start_of_working_day = TRUE;
	} else if (start_time->hour < mts->day_start_hour[start_weekday]
		   || (start_time->hour == mts->day_start_hour[start_weekday]
		       && start_time->minute < mts->day_start_minute[start_weekday])) {
		set_to_start_of_working_day = TRUE;
	}

	if (set_to_start_of_working_day) {
		start_time->hour = mts->day_start_hour[start_weekday];
		start_time->minute = mts->day_start_minute[start_weekday];

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
 * If zoomed_out is set, this means every hour. If not every half-hour. */
static void
e_meeting_time_selector_find_nearest_interval_backward (EMeetingTimeSelector *mts,
                                                        EMeetingTime *start_time,
                                                        EMeetingTime *end_time,
                                                        gint days,
                                                        gint hours,
                                                        gint mins)
{
	GDateWeekday start_weekday, end_weekday;
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
	 * If it isn't we don't worry about the working day. */
	if (!mts->working_hours_only || days > 0)
		return;

	start_weekday = e_meeting_time_selector_get_time_weekday (start_time);
	end_weekday = e_meeting_time_selector_get_time_weekday (end_time);

	minutes_shown = (mts->day_end_hour[end_weekday] - mts->day_start_hour[start_weekday]) * 60;
	minutes_shown += mts->day_end_minute[end_weekday] - mts->day_start_minute[start_weekday];

	if (hours * 60 + mins > minutes_shown)
		return;

	/* If the meeting time finishes past the end of the working day, move
	 * back to the end of the working day. If the meeting time starts
	 * before the working day, move it back to the end of the previous
	 * working day. */
	if (start_time->hour > mts->day_end_hour[end_weekday]
	    || (start_time->hour == mts->day_end_hour[end_weekday]
		&& start_time->minute > mts->day_end_minute[end_weekday])
	    || end_time->hour > mts->day_end_hour[end_weekday]
	    || (end_time->hour == mts->day_end_hour[end_weekday]
		&& end_time->minute > mts->day_end_minute[end_weekday])) {
		set_to_end_of_working_day = TRUE;
	} else if (start_time->hour < mts->day_start_hour[start_weekday]
		   || (start_time->hour == mts->day_start_hour[start_weekday]
		       && start_time->minute < mts->day_start_minute[start_weekday])) {
		g_date_subtract_days (&end_time->date, 1);
		set_to_end_of_working_day = TRUE;
	}

	if (set_to_end_of_working_day) {
		end_time->hour = mts->day_end_hour[end_weekday];
		end_time->minute = mts->day_end_minute[end_weekday];
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
 * It is used to calculate the end of a period given a start & duration.
 * Days, hours & minutes can be negative, to move backwards, but they should
 * be within normal ranges, e.g. hours should be between -23 and 23. */
static void
e_meeting_time_selector_adjust_time (EMeetingTime *mtstime,
                                     gint days,
                                     gint hours,
                                     gint minutes)
{
	gint new_hours, new_minutes;

	/* We have to handle negative values for hous and minutes here, since
	 * EMeetingTimeuses guint8s to store them. */
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
 * the start and end time. It uses a binary search. */
static EMeetingFreeBusyPeriod *
e_meeting_time_selector_find_time_clash (EMeetingTimeSelector *mts,
                                         EMeetingAttendee *attendee,
                                         EMeetingTime *start_time,
                                         EMeetingTime *end_time)
{
	EMeetingFreeBusyPeriod *period;
	const GArray *busy_periods;
	gint period_num;

	busy_periods = e_meeting_attendee_get_busy_periods (attendee);
	period_num = e_meeting_attendee_find_first_busy_period (attendee, &start_time->date);

	if (period_num == -1)
		return NULL;

	/* Step forward through the busy periods until we find a clash or we
	 * go past the end_time. */
	while (period_num < busy_periods->len) {
		period = &g_array_index (busy_periods,  EMeetingFreeBusyPeriod, period_num);

		/* If the period starts at or after the end time, there is no
		 * clash and we are finished. The busy periods are sorted by
		 * their start times, so all the rest will be later. */
		if (e_meeting_time_compare_times (&period->start, end_time) >= 0)
			return NULL;

		/* If the period ends after the start time, we have found a
		 * clash. From the above test we already know the busy period
		 * isn't completely after the meeting time. */
		if (e_meeting_time_compare_times (&period->end, start_time) > 0)
			return period;

		period_num++;
	}

	return NULL;
}

static void
e_meeting_time_selector_on_zoomed_out_toggled (GtkCheckMenuItem *menuitem,
                                               EMeetingTimeSelector *mts)
{
	gboolean active;

	/* Make sure the menu pops down, which doesn't happen by default if
	 * keyboard accelerators are used. */
	if (gtk_widget_get_visible (mts->options_menu))
		gtk_menu_popdown (GTK_MENU (mts->options_menu));

	active = gtk_check_menu_item_get_active (menuitem);
	e_meeting_time_selector_set_zoomed_out (mts, active);
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
}

static void
e_meeting_time_selector_on_working_hours_toggled (GtkCheckMenuItem *menuitem,
                                                  EMeetingTimeSelector *mts)
{
	gboolean active;

	/* Make sure the menu pops down, which doesn't happen by default if
	 * keyboard accelerators are used. */
	if (gtk_widget_get_visible (mts->options_menu))
		gtk_menu_popdown (GTK_MENU (mts->options_menu));

	active = gtk_check_menu_item_get_active (menuitem);
	e_meeting_time_selector_set_working_hours_only (mts, active);
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
}

/* This recalculates day_width, first_hour_shown and last_hour_shown. */
static void
e_meeting_time_selector_recalc_grid (EMeetingTimeSelector *mts)
{
	if (mts->working_hours_only) {
		GDateWeekday weekday;

		mts->first_hour_shown = mts->day_start_hour[G_DATE_BAD_WEEKDAY];
		mts->last_hour_shown = mts->day_end_hour[G_DATE_BAD_WEEKDAY];

		for (weekday = G_DATE_BAD_WEEKDAY; weekday <= G_DATE_SUNDAY; weekday++) {
			if (mts->first_hour_shown > mts->day_start_hour[weekday])
				mts->first_hour_shown = mts->day_start_hour[weekday];
			if (mts->last_hour_shown <= mts->day_end_hour[weekday]) {
				mts->last_hour_shown = mts->day_end_hour[weekday];
				if (mts->day_end_minute[weekday] != 0)
					mts->last_hour_shown += 1;
			}
		}
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

	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (mts->display_top),
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
                                       EMeetingTime *mtstime)
{
	gint scroll_x, scroll_y;

	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (mts->display_main),
		&scroll_x, &scroll_y);
	e_meeting_time_selector_calculate_time (mts, scroll_x, mtstime);
}

/* This restores a saved position. */
static void
e_meeting_time_selector_restore_position (EMeetingTimeSelector *mts,
                                          EMeetingTime *mtstime)
{
	gint scroll_x, scroll_y, new_scroll_x;

	new_scroll_x = e_meeting_time_selector_calculate_time_position (
		mts, mtstime);
	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (mts->display_main),
		&scroll_x, &scroll_y);
	gnome_canvas_scroll_to (
		GNOME_CANVAS (mts->display_main),
		new_scroll_x, scroll_y);
}

/* This returns the x pixel coords of the meeting time in the entire scroll
 * region. It recalculates them if they have been marked as invalid.
 * If it returns FALSE then no meeting time is set or the meeting time is
 * not visible in the current scroll area. */
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
 * longest date strings in the widget's font and seeing if they fit. */
static void
e_meeting_time_selector_recalc_date_format (EMeetingTimeSelector *mts)
{
	/* An array of dates, one for each month in the year 2000. They must
	 * all be Sundays. */
	static const gint days[12] = { 23, 20, 19, 23, 21, 18,
				      23, 20, 17, 22, 19, 24 };
	GDate date;
	gint max_date_width, longest_weekday_width, longest_month_width, width;
	gint day, longest_weekday, month, longest_month;
	gchar buffer[128], *str;
	const gchar *name;
	PangoContext *pango_context;
	PangoLayout *layout;
	struct tm tm_time;

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (mts));
	layout = pango_layout_new (pango_context);

	/* Calculate the maximum date width we can fit into the display. */
	max_date_width = mts->day_width - 2;

	/* Find the biggest full weekday name. We start on a particular
	 * Monday and go through seven days. */
	g_date_clear (&date, 1);
	g_date_set_dmy (&date, 3, 1, 2000);	/* Monday 3rd Jan 2000. */
	longest_weekday_width = 0;
	longest_weekday = G_DATE_MONDAY;
	for (day = G_DATE_MONDAY; day <= G_DATE_SUNDAY; day++) {
		name = e_get_weekday_name (day, FALSE);
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);
		if (width > longest_weekday_width) {
			longest_weekday = day;
			longest_weekday_width = width;
		}

		/* Now try it with abbreviated weekday names. */
		name = e_get_weekday_name (day, TRUE);
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);
		if (width > longest_weekday_width) {
			longest_weekday = day;
			longest_weekday_width = width;
		}
	}

	/* Now find the biggest month name. */
	longest_month_width = 0;
	longest_month = G_DATE_JANUARY;
	for (month = G_DATE_JANUARY; month <= G_DATE_DECEMBER; month++) {
		name = e_get_month_name (month, FALSE);
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);
		if (width > longest_month_width) {
			longest_month = month;
			longest_month_width = width;
		}
	}

	g_date_set_dmy (
		&date, days[longest_month - 1] + longest_weekday,
		longest_month, 2000);

	g_date_to_struct_tm (&date, &tm_time);
	str = e_datetime_format_format_tm ("calendar", "table",  DTFormatKindDate, &tm_time);

	g_return_if_fail (str != NULL);

	if (!e_datetime_format_includes_day_name ("calendar", "table",  DTFormatKindDate)) {
		gchar *tmp;

		g_date_strftime (buffer, sizeof (buffer), "%a", &date);

		tmp = str;
		str = g_strconcat (buffer, " ", str, NULL);
		g_free (tmp);
	}

#if 0
	g_print (
		"longest_month: %i longest_weekday: %i date: %s\n",
		longest_month, longest_weekday, str);
#endif

	pango_layout_set_text (layout, str, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	if (width < max_date_width)
		mts->date_format = E_MEETING_TIME_SELECTOR_DATE_ABBREVIATED_DAY;
	else
		mts->date_format = E_MEETING_TIME_SELECTOR_DATE_SHORT;

	g_object_unref (layout);
	g_free (str);
}

/* Turn off the background of the canvas windows. This reduces flicker
 * considerably when scrolling. (Why isn't it in GnomeCanvas?). */
static void
e_meeting_time_selector_on_canvas_realized (GtkWidget *widget,
                                            EMeetingTimeSelector *mts)
{
	GdkWindow *window;

	window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
	gdk_window_set_background_pattern (window, NULL);
}

/* This is called when the meeting start time GnomeDateEdit is changed,
 * either via the "date_changed". "time_changed" or "activate" signals on one
 * of the GtkEntry widgets. So don't use the widget parameter since it may be
 * one of the child GtkEntry widgets. */
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
	g_date_set_time_t (&mtstime.date, newtime);

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

	g_signal_emit (mts, signals[CHANGED], 0);
}

/* This is called when the meeting end time GnomeDateEdit is changed,
 * either via the "date_changed", "time_changed" or "activate" signals on one
 * of the GtkEntry widgets. So don't use the widget parameter since it may be
 * one of the child GtkEntry widgets. */
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
	g_date_set_time_t (&mtstime.date, newtime);
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
		 * handler will just return. */
		mts->meeting_start_time = mtstime;
		if (mts->all_day)
			g_date_subtract_days (&mts->meeting_start_time.date, 1);
		e_meeting_time_selector_update_start_date_edit (mts);
	}

	mts->meeting_positions_valid = FALSE;
	e_meeting_time_selector_ensure_meeting_time_shown (mts);
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);

	g_signal_emit (mts, signals[CHANGED], 0);
}

/* This updates the ranges shown in the GnomeDateEdit popup menus, according
 * to working_hours_only etc. */
static void
e_meeting_time_selector_update_date_popup_menus (EMeetingTimeSelector *mts)
{
	EDateEdit *start_edit, *end_edit;
	gint low_hour, high_hour;

	start_edit = E_DATE_EDIT (mts->start_date_edit);
	end_edit = E_DATE_EDIT (mts->end_date_edit);

	if (mts->working_hours_only) {
		GDateWeekday weekday;

		low_hour = mts->day_start_hour[G_DATE_MONDAY];
		high_hour = mts->day_end_hour[G_DATE_MONDAY];

		for (weekday = G_DATE_MONDAY; weekday <= G_DATE_SUNDAY; weekday++) {
			if (low_hour > mts->day_start_hour[weekday])
				low_hour = mts->day_start_hour[weekday];
			if (high_hour <= mts->day_end_hour[weekday]) {
				high_hour = mts->day_end_hour[weekday];
			}
		}
	} else {
		low_hour = 0;
		high_hour = 24;
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

static gboolean
e_meeting_time_selector_on_canvas_scroll_event (GtkWidget *widget,
                                                GdkEventScroll *event,
                                                EMeetingTimeSelector *mts)
{
	gboolean return_val = FALSE;

	/* escalate to the list view's parent, which is a scrolled window */
	g_signal_emit_by_name (gtk_widget_get_parent (GTK_WIDGET (mts->list_view)), "scroll-event", event, &return_val);

	return return_val;
}

/* Sets a tooltip for the busy periods canvas. If the mouse pointer
 * hovers over a busy period for which extended free/busy (XFB) data
 * could be extracted from the vfreebusy calendar object, the tooltip
 * will be shown (currently displays the summary and the location of
 * for the busy period, if available). See EMeetingXfbData for a reference.
 *
 */
static gboolean
e_meeting_time_selector_on_canvas_query_tooltip (GtkWidget *widget,
                                                 gint x,
                                                 gint y,
                                                 gboolean keyboard_mode,
                                                 GtkTooltip *tooltip,
                                                 gpointer user_data)
{
	EMeetingTimeSelector *mts = NULL;
	EMeetingAttendee *attendee = NULL;
	EMeetingFreeBusyPeriod *period = NULL;
	EMeetingXfbData *xfb = NULL;
	GtkScrollable *scrollable = NULL;
	GtkAdjustment *adjustment = NULL;
	const GArray *periods = NULL;
	gint scroll_x = 0;
	gint scroll_y = 0;
	gint mouse_x = 0;
	gint row = 0;
	gint first_idx = 0;
	gint ii = 0;
	gchar *tt_text = NULL;

	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (GTK_IS_TOOLTIP (tooltip), FALSE);
	g_return_val_if_fail (E_IS_MEETING_TIME_SELECTOR (user_data), FALSE);

	mts = E_MEETING_TIME_SELECTOR (user_data);

	scrollable = GTK_SCROLLABLE (widget);
	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	scroll_x = (gint) gtk_adjustment_get_value (adjustment);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	scroll_y = (gint) gtk_adjustment_get_value (adjustment);

	/* calculate the attendee index (row) we're at */
	row = (scroll_y + y) / mts->row_height;

	/* no tooltip if we have no attendee in the row */
	if (row > e_meeting_store_count_actual_attendees (mts->model) - 1)
		return FALSE;

	/* no tooltip if attendee has no calendar info */
	attendee = e_meeting_store_find_attendee_at_row (mts->model, row);
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (attendee), FALSE);
	if (!e_meeting_attendee_get_has_calendar_info (attendee))
		return FALSE;

	/* get the attendee's busy times array */
	periods = e_meeting_attendee_get_busy_periods (attendee);
	g_return_val_if_fail (periods != NULL, FALSE);
	g_return_val_if_fail (periods->len > 0, FALSE);

	/* no tooltip if no busy period reaches into the current canvas area */
	first_idx = e_meeting_attendee_find_first_busy_period (
		attendee, &(mts->first_date_shown));
	if (first_idx < 0)
		return FALSE;

	/* calculate the mouse tip x position in the canvas area */
	mouse_x = x + scroll_x;

	/* find out whether mouse_x lies inside a busy
	 * period (we start with the index of the first
	 * one reaching into the current canvas area)
	 */
	for (ii = first_idx; ii < periods->len; ii++) {
		EMeetingFreeBusyPeriod *p = NULL;
		gint sx = 0;
		gint ex = 0;

		p = &(g_array_index (
			periods, EMeetingFreeBusyPeriod, ii));
		/* meeting start time x position */
		sx = e_meeting_time_selector_calculate_time_position (
			mts, &(p->start));
		/* meeting end time x position */
		ex = e_meeting_time_selector_calculate_time_position (
			mts, &(p->end));
		if ((mouse_x >= sx) && (mouse_x <= ex)) {
			/* found busy period the mouse tip is over */
			period = p;
			break;
		}
	}

	/* no tooltip if we did not find a busy period under
	 * the mouse pointer
	 */
	if (period == NULL)
		return FALSE;

	/* get the extended free/busy data
	 * (no tooltip if none available)
	 */
	xfb = &(period->xfb);
	if ((xfb->summary == NULL) && (xfb->location == NULL))
		return FALSE;

	/* Create the tooltip text. The data sent by the server will
	 * have been validated for UTF-8 conformance (and possibly
	 * forced into) as well as length-limited by a call to the
	 * e_meeting_xfb_utf8_string_new_from_ical() function in
	 * process_free_busy_comp_get_xfb() (e-meeting-store.c)
	 */
	if (xfb->summary && xfb->location)
		tt_text = g_strdup_printf (_("Summary: %s\nLocation: %s"), xfb->summary, xfb->location);
	else if (xfb->summary)
		tt_text = g_strdup_printf (_("Summary: %s"), xfb->summary);
	else if (xfb->location)
		tt_text = g_strdup_printf (_("Location: %s"), xfb->location);
	else
		g_return_val_if_reached (FALSE);

	/* set XFB information as tooltip text */
	gtk_tooltip_set_text (tooltip, tt_text);
	g_free (tt_text);

	return TRUE;
}

/* This updates the canvas scroll regions according to the number of attendees.
 * If the total height needed is less than the height of the canvas, we must
 * use the height of the canvas, or it causes problems. */
static void
e_meeting_time_selector_update_main_canvas_scroll_region (EMeetingTimeSelector *mts)
{
	GtkAllocation allocation;
	gint height;

	gtk_widget_get_allocation (mts->display_main, &allocation);
	height = mts->row_height * (e_meeting_store_count_actual_attendees (mts->model) + 2);
	height = MAX (height, allocation.height);

	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (mts->display_main),
		0, 0,
		mts->day_width * E_MEETING_TIME_SELECTOR_DAYS_SHOWN,
		height);
}

/* This changes the meeting time based on the given x coordinate and whether
 * we are dragging the start or end bar. It returns the new position, which
 * will be swapped if the start bar is dragged past the end bar or vice versa.
 * It make sure the meeting time is never dragged outside the visible canvas
 * area. */
void
e_meeting_time_selector_drag_meeting_time (EMeetingTimeSelector *mts,
                                           gint x)
{
	EMeetingTime first_time, last_time, drag_time, *time_to_set;
	gint scroll_x, scroll_y, canvas_width;
	gboolean set_both_times = FALSE;
	GtkAllocation allocation;

	/* Get the x coords of visible part of the canvas. */
	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (mts->display_main),
		&scroll_x, &scroll_y);
	gtk_widget_get_allocation (mts->display_main, &allocation);
	canvas_width = allocation.width;

	/* Save the x coordinate for the timeout handler. */
	mts->last_drag_x = (x < scroll_x) ? x - scroll_x
		: x - scroll_x - canvas_width + 1;

	/* Check if the mouse is off the edge of the canvas. */
	if (x < scroll_x || x > scroll_x + canvas_width) {
		/* If we haven't added a timeout function, add one. */
		if (mts->auto_scroll_timeout_id == 0) {
			mts->auto_scroll_timeout_id = e_named_timeout_add (
				60, e_meeting_time_selector_timeout_handler, mts);
			mts->scroll_count = 0;

			/* Call the handler to start scrolling now. */
			e_meeting_time_selector_timeout_handler (mts);
			return;
		}
	} else {
		e_meeting_time_selector_remove_timeout (mts);
	}

	/* Calculate the minimum & maximum times we can use, based on the
	 * scroll offsets and whether zoomed_out is set. */
	e_meeting_time_selector_calculate_time (mts, scroll_x, &first_time);
	e_meeting_time_selector_calculate_time (
		mts, scroll_x + canvas_width - 1, &last_time);
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
	 * zoomed_out is set. */
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
		g_signal_emit (mts, signals[CHANGED], 0);
}

/* This is the timeout function which handles auto-scrolling when the user is
 * dragging one of the meeting time vertical bars outside the left or right
 * edge of the canvas. */
static gboolean
e_meeting_time_selector_timeout_handler (gpointer data)
{
	EMeetingTimeSelector *mts;
	EMeetingTime drag_time, *time_to_set;
	gint scroll_x, max_scroll_x, scroll_y, canvas_width;
	gint scroll_speed, scroll_offset;
	gboolean set_both_times = FALSE;
	GtkAllocation allocation;

	mts = E_MEETING_TIME_SELECTOR (data);

	/* Return if we don't need to scroll yet. */
	if (mts->scroll_count-- > 0)
		return TRUE;

	/* Get the x coords of visible part of the canvas. */
	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (mts->display_main),
		&scroll_x, &scroll_y);
	gtk_widget_get_allocation (mts->display_main, &allocation);
	canvas_width = allocation.width;

	/* Calculate the scroll delay, between 0 and MAX_SCROLL_SPEED. */
	scroll_speed = abs (mts->last_drag_x / E_MEETING_TIME_SELECTOR_SCROLL_INCREMENT_WIDTH);
	scroll_speed = MIN (
		scroll_speed,
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
	 * we will now set the dragged time to. */
	if (scroll_offset > 0) {
		e_meeting_time_selector_calculate_time (
			mts, scroll_x + canvas_width - 1, &drag_time);
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
		e_meeting_time_selector_calculate_time (
			mts, scroll_x, &drag_time);
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
	if (e_meeting_time_compare_times (time_to_set, &drag_time) == 0)
		goto scroll;

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
		g_signal_emit (mts, signals[CHANGED], 0);

 scroll:
	/* Redraw the canvases. We freeze and thaw the layouts so that they
	 * get redrawn completely. Otherwise the pixels get scrolled left or
	 * right which is not good for us (since our vertical bars have been
	 * moved) and causes flicker. */
	gnome_canvas_scroll_to (
		GNOME_CANVAS (mts->display_main),
		scroll_x, scroll_y);
	gnome_canvas_scroll_to (
		GNOME_CANVAS (mts->display_top),
		scroll_x, scroll_y);

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
	e_date_edit_set_date_and_time_of_day (
		E_DATE_EDIT (mts->start_date_edit),
		g_date_get_year (&mts->meeting_start_time.date),
		g_date_get_month (&mts->meeting_start_time.date),
		g_date_get_day (&mts->meeting_start_time.date),
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

	e_date_edit_set_date_and_time_of_day (
		E_DATE_EDIT (mts->end_date_edit),
		g_date_get_year (&date),
		g_date_get_month (&date),
		g_date_get_day (&date),
		mts->meeting_end_time.hour,
		mts->meeting_end_time.minute);
}

/* This ensures that the meeting time is shown on screen, by scrolling the
 * canvas and possibly by changing the range of dates shown in the canvas. */
static void
e_meeting_time_selector_ensure_meeting_time_shown (EMeetingTimeSelector *mts)
{
	gint start_x, end_x, scroll_x, scroll_y;
	gint new_scroll_x;
	GtkAllocation allocation;
	EMeetingTime time;

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
	if (e_meeting_time_selector_get_meeting_time_positions (mts, &start_x,
							    &end_x)) {
		time.date = mts->meeting_start_time.date;
		time.hour = 0;
		time.minute = 0;
		start_x = e_meeting_time_selector_calculate_time_position (mts, &time);
	}

	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (mts->display_main),
		&scroll_x, &scroll_y);
	gtk_widget_get_allocation (mts->display_main, &allocation);
	if (start_x > scroll_x && end_x <= scroll_x + allocation.width)
		return;

	new_scroll_x = start_x;
	gnome_canvas_scroll_to (
		GNOME_CANVAS (mts->display_main),
		new_scroll_x, scroll_y);
}

/* This updates the range of dates shown in the canvas, to make sure that the
 * currently selected meeting time is in the range. */
static void
e_meeting_time_selector_update_dates_shown (EMeetingTimeSelector *mts)
{
	mts->first_date_shown = mts->meeting_start_time.date;
	g_date_subtract_days (
		&mts->first_date_shown,
		E_MEETING_TIME_SELECTOR_DAYS_START_BEFORE);

	mts->last_date_shown = mts->first_date_shown;
	g_date_add_days (
		&mts->last_date_shown,
		E_MEETING_TIME_SELECTOR_DAYS_SHOWN - 1);
}

/* This checks if the time's hour is over 24 or its minute is over 60 and if
 * so it updates the day/hour appropriately. Note that hours and minutes are
 * stored in guint8's so they can't overflow by much. */
void
e_meeting_time_selector_fix_time_overflows (EMeetingTime *mtstime)
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
 * returns the date in which it falls. If day_position is not NULL it also
 * returns the x coordinate within the date, relative to the visible part of
 * the canvas. It is used when painting the days in the item_draw function.
 * Note that it must handle negative x coordinates in case we are dragging off
 * the edge of the canvas. */
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
 * and minutes, depending on working_hours_only and zoomed_out. */
void
e_meeting_time_selector_convert_day_position_to_hours_and_mins (EMeetingTimeSelector *mts,
                                                                gint day_position,
                                                                guint8 *hours,
                                                                guint8 *minutes)
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
 * returns the time in which it falls. Note that it won't be extremely
 * accurate since hours may only be a few pixels wide in the display.
 * With zoomed_out set each pixel may represent 5 minutes or more, depending
 * on how small the font is. */
void
e_meeting_time_selector_calculate_time (EMeetingTimeSelector *mts,
                                        gint x,
                                        EMeetingTime *time)
{
	gint day_position;

	/* First get the day and the x position within the day. */
	e_meeting_time_selector_calculate_day_and_position (
		mts, x, &time->date, NULL);

	/* Now convert the day_position into an hour and minute. */
	if (x >= 0)
		day_position = x % mts->day_width;
	else
		day_position = mts->day_width + x % mts->day_width;

	e_meeting_time_selector_convert_day_position_to_hours_and_mins (
		mts, day_position, &time->hour, &time->minute);
}

/* This takes a EMeetingTime and calculates the x pixel coordinate
 * within the entire canvas scroll region. It is used to draw the selected
 * meeting time and all the busy periods. */
gint
e_meeting_time_selector_calculate_time_position (EMeetingTimeSelector *mts,
                                                 EMeetingTime *mtstime)
{
	gint x, date_offset, day_offset;

	/* Calculate the number of days since the first date shown in the
	 * entire canvas scroll region. */
	date_offset =
		g_date_get_julian (&mtstime->date) -
		g_date_get_julian (&mts->first_date_shown);

	/* Calculate the x pixel coordinate of the start of the day. */
	x = date_offset * mts->day_width;

	/* Add on the hours and minutes, depending on whether zoomed_out and
	 * working_hours_only are set. */
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
row_inserted_cb (GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 gpointer data)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);
	gint row = gtk_tree_path_get_indices (path)[0];
	/* Update the scroll region. */
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);

	/* Redraw */
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);

	/* Get the latest free/busy info */
	e_meeting_time_selector_refresh_free_busy (mts, row, FALSE);
}

static void
row_changed_cb (GtkTreeModel *model,
                GtkTreePath *path,
                GtkTreeIter *iter,
                gpointer data)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);
	gint row = gtk_tree_path_get_indices (path)[0];

	/* Get the latest free/busy info */
	e_meeting_time_selector_refresh_free_busy (mts, row, FALSE);
}

static void
row_deleted_cb (GtkTreeModel *model,
                GtkTreePath *path,
                gpointer data)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);

	/* Update the scroll region. */
	e_meeting_time_selector_update_main_canvas_scroll_region (mts);

	/* Redraw */
	gtk_widget_queue_draw (mts->display_top);
	gtk_widget_queue_draw (mts->display_main);
}

#define REFRESH_PAUSE 2

static gboolean
free_busy_timeout_refresh (gpointer data)
{
	EMeetingTimeSelector *mts = E_MEETING_TIME_SELECTOR (data);

	/* Update all free/busy info, so we use the new template uri */
	e_meeting_time_selector_refresh_free_busy (mts, 0, TRUE);

	mts->fb_refresh_not = 0;

	return FALSE;
}

static void
free_busy_schedule_refresh_cb (EMeetingTimeSelector *mts)
{
	/* Wait REFRESH_PAUSE before refreshing, using the latest uri value or timezone */
	if (mts->fb_refresh_not != 0)
		g_source_remove (mts->fb_refresh_not);

	mts->fb_refresh_not = e_named_timeout_add_seconds (
		REFRESH_PAUSE, free_busy_timeout_refresh, mts);
}
