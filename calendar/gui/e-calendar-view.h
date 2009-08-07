/*
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CALENDAR_VIEW_H_
#define _E_CALENDAR_VIEW_H_

#include <libecal/e-cal.h>
#include <gtk/gtk.h>
#include "e-cal-model.h"
#include "gnome-cal.h"
#include <misc/e-activity-handler.h>
#include "dialogs/comp-editor.h"

G_BEGIN_DECLS

/*
 * EView - base widget class for the calendar views.
 */

#define E_TYPE_CALENDAR_VIEW          (e_calendar_view_get_type ())
#define E_CALENDAR_VIEW(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_calendar_view_get_type (), ECalendarView)
#define E_CALENDAR_VIEW_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_calendar_view_get_type (), ECalendarViewClass)
#define E_IS_CALENDAR_VIEW(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_calendar_view_get_type ())

typedef enum {
	E_CALENDAR_VIEW_POS_OUTSIDE,
	E_CALENDAR_VIEW_POS_NONE,
	E_CALENDAR_VIEW_POS_EVENT,
	E_CALENDAR_VIEW_POS_LEFT_EDGE,
	E_CALENDAR_VIEW_POS_RIGHT_EDGE,
	E_CALENDAR_VIEW_POS_TOP_EDGE,
	E_CALENDAR_VIEW_POS_BOTTOM_EDGE
} ECalendarViewPosition;

typedef enum {
	E_CAL_VIEW_MOVE_UP,
	E_CAL_VIEW_MOVE_DOWN,
	E_CAL_VIEW_MOVE_LEFT,
	E_CAL_VIEW_MOVE_RIGHT,
	E_CAL_VIEW_MOVE_PAGE_UP,
	E_CAL_VIEW_MOVE_PAGE_DOWN
} ECalViewMoveDirection;

#define E_CALENDAR_VIEW_EVENT_FIELDS \
        GnomeCanvasItem *canvas_item; \
        ECalModelComponent *comp_data; \
        time_t start; \
        time_t end; \
        guint16 start_minute; \
        guint16 end_minute; \
        guint different_timezone : 1; \
	gboolean is_editable;	\
	GtkWidget *tooltip; \
	gint	timeout; \
	GdkColor *color; \
	gint x,y;

typedef struct {
	E_CALENDAR_VIEW_EVENT_FIELDS
} ECalendarViewEvent;

typedef struct _ECalendarView        ECalendarView;
typedef struct _ECalendarViewClass   ECalendarViewClass;
typedef struct _ECalendarViewPrivate ECalendarViewPrivate;

struct _ECalendarView {
	GtkTable table;

	gboolean in_focus;
	ECalendarViewPrivate *priv;
};

typedef struct {
	ECalendarViewEvent * (* get_view_event) (ECalendarView *view, gint day, gint event_num);
	ECalendarView *cal_view;
	gint day;
	gint event_num;
} ECalendarViewEventData;

struct _ECalendarViewClass {
	GtkTableClass parent_class;

	/* Notification signals */
	void (* selection_changed) (ECalendarView *cal_view);
	void (* selected_time_changed) (ECalendarView *cal_view);
	void (* timezone_changed) (ECalendarView *cal_view, icaltimezone *old_zone, icaltimezone *new_zone);
	void (* event_changed) (ECalendarView *day_view, ECalendarViewEvent *event);
	void (* event_added) (ECalendarView *day_view, ECalendarViewEvent *event);
	void (* user_created) (ECalendarView *cal_view);

	/* Virtual methods */
	GList * (* get_selected_events) (ECalendarView *cal_view); /* a GList of ECalendarViewEvent's */
	gboolean (* get_selected_time_range) (ECalendarView *cal_view, time_t *start_time, time_t *end_time);
	void (* set_selected_time_range) (ECalendarView *cal_view, time_t start_time, time_t end_time);
	gboolean (* get_visible_time_range) (ECalendarView *cal_view, time_t *start_time, time_t *end_time);
	void (* update_query) (ECalendarView *cal_view);
	void (* open_event) (ECalendarView *cal_view);
	void (* paste_text) (ECalendarView *cal_view);
};

GType          e_calendar_view_get_type (void);

GnomeCalendar *e_calendar_view_get_calendar (ECalendarView *cal_view);
void           e_calendar_view_set_calendar (ECalendarView *cal_view, GnomeCalendar *calendar);
ECalModel     *e_calendar_view_get_model (ECalendarView *cal_view);
void           e_calendar_view_set_model (ECalendarView *cal_view, ECalModel *model);
icaltimezone  *e_calendar_view_get_timezone (ECalendarView *cal_view);
void           e_calendar_view_set_timezone (ECalendarView *cal_view, icaltimezone *zone);
const gchar    *e_calendar_view_get_default_category (ECalendarView *cal_view);
void           e_calendar_view_set_default_category (ECalendarView *cal_view, const gchar *category);
gboolean       e_calendar_view_get_use_24_hour_format (ECalendarView *view);
void           e_calendar_view_set_use_24_hour_format (ECalendarView *view, gboolean use_24_hour);

void           e_calendar_view_set_activity_handler (ECalendarView *cal_view, EActivityHandler *activity_handler);
void           e_calendar_view_set_status_message (ECalendarView *cal_view, const gchar *message, gint percent);

GList         *e_calendar_view_get_selected_events (ECalendarView *cal_view);
gboolean       e_calendar_view_get_selected_time_range (ECalendarView *cal_view, time_t *start_time, time_t *end_time);
void           e_calendar_view_set_selected_time_range (ECalendarView *cal_view, time_t start_time, time_t end_time);
gboolean       e_calendar_view_get_visible_time_range (ECalendarView *cal_view, time_t *start_time, time_t *end_time);
void           e_calendar_view_update_query (ECalendarView *cal_view);

void           e_calendar_view_cut_clipboard (ECalendarView *cal_view);
void           e_calendar_view_copy_clipboard (ECalendarView *cal_view);
void           e_calendar_view_paste_clipboard (ECalendarView *cal_view);
void           e_calendar_view_delete_selected_event (ECalendarView *cal_view);
void           e_calendar_view_delete_selected_events (ECalendarView *cal_view);
void           e_calendar_view_delete_selected_occurrence (ECalendarView *cal_view);
CompEditor*    e_calendar_view_open_event_with_flags (ECalendarView *cal_view, ECal *client, icalcomponent *icalcomp, guint32 flags);

GtkMenu       *e_calendar_view_create_popup_menu (ECalendarView *cal_view);

void           e_calendar_view_add_event (ECalendarView *cal_view, ECal *client, time_t dtstart,
				     icaltimezone *default_zone, icalcomponent *icalcomp, gboolean in_top_canvas);
void           e_calendar_view_new_appointment_for (ECalendarView *cal_view,
						    time_t dtstart,
						    time_t dtend,
						    gboolean all_day,
						    gboolean meeting);
void           e_calendar_view_new_appointment_full (ECalendarView *cal_view,
						     gboolean all_day,
						     gboolean meeting,
						     gboolean no_past_date);
void           e_calendar_view_new_appointment (ECalendarView *cal_view);
void           e_calendar_view_edit_appointment (ECalendarView *cal_view,
					    ECal *client,
					    icalcomponent *icalcomp,
					    gboolean meeting);
void           e_calendar_view_open_event (ECalendarView *cal_view);
void           e_calendar_view_modify_and_send (ECalComponent *comp,
					       ECal *client,
					       CalObjModType mod,
					       GtkWindow *toplevel,
					       gboolean new);
void e_calendar_utils_show_error_silent (GtkWidget *widget);
void e_calendar_utils_show_info_silent(GtkWidget *widget);

gboolean	e_calendar_view_get_tooltips (ECalendarViewEventData *data);

void           e_calendar_view_move_tip (GtkWidget *widget, gint x, gint y);

const gchar *e_calendar_view_get_icalcomponent_summary (ECal *ecal, icalcomponent *icalcomp, gboolean *free_text);
gchar *e_calendar_view_get_attendees_status_info (ECalComponent *comp, ECal *client);

void           draw_curved_rectangle (cairo_t *cr,
                                      double x0,
                                      double y0,
                                      double rect_width,
                                      double rect_height,
                                      double radius);

GdkColor get_today_background (GdkColor event_background);

G_END_DECLS

#endif
