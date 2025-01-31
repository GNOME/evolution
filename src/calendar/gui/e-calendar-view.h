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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CALENDAR_VIEW_H
#define E_CALENDAR_VIEW_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <calendar/gui/e-cal-model.h>
#include <calendar/gui/e-comp-editor.h>

/* Standard GObject macros */
#define E_TYPE_CALENDAR_VIEW \
	(e_calendar_view_get_type ())
#define E_CALENDAR_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CALENDAR_VIEW, ECalendarView))
#define E_CALENDAR_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CALENDAR_VIEW, ECalendarViewClass))
#define E_IS_CALENDAR_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CALENDAR_VIEW))
#define E_IS_CALENDAR_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CALENDAR_VIEW))
#define E_CALENDAR_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CALENDAR_VIEW, ECalendarViewClass))

G_BEGIN_DECLS

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
	gboolean is_editable; \
	GdkRGBA *color; \
	gint x,y;

typedef struct {
	E_CALENDAR_VIEW_EVENT_FIELDS
} ECalendarViewEvent;

/* checks if event->comp_data is not NULL, returns FALSE
 * when it is and prints a warning on a console */
gboolean	is_comp_data_valid_func		(ECalendarViewEvent *event,
						 const gchar *location);
#define is_comp_data_valid(_event) \
	is_comp_data_valid_func ((ECalendarViewEvent *) (_event), G_STRFUNC)

/* checks if index is within bounds for the array; returns
 * FALSE when not, and prints a warning on a console */
gboolean	is_array_index_in_bounds_func	(GArray *array,
						 gint index,
						 const gchar *location);
#define is_array_index_in_bounds(_array, _index) \
	is_array_index_in_bounds_func (_array, _index, G_STRFUNC)

typedef struct _ECalendarView ECalendarView;
typedef struct _ECalendarViewClass ECalendarViewClass;
typedef struct _ECalendarViewPrivate ECalendarViewPrivate;

struct _ECalendarView {
	GtkGrid parent;
	gboolean in_focus;
	ECalendarViewPrivate *priv;
};

typedef struct _ECalendarViewSelectionData {
	ECalClient *client;
	ICalComponent *icalcomp;
} ECalendarViewSelectionData;

ECalendarViewSelectionData *
		e_calendar_view_selection_data_new	(ECalClient *client,
							 ICalComponent *icalcomp);
void		e_calendar_view_selection_data_free	(gpointer ptr);

typedef enum {
	EDIT_EVENT_AUTODETECT,
	EDIT_EVENT_FORCE_MEETING,
	EDIT_EVENT_FORCE_APPOINTMENT
} EEditEventMode;

typedef enum {
	E_CALENDAR_VIEW_MOVE_PREVIOUS,
	E_CALENDAR_VIEW_MOVE_NEXT,
	E_CALENDAR_VIEW_MOVE_TO_TODAY,
	E_CALENDAR_VIEW_MOVE_TO_EXACT_DAY
} ECalendarViewMoveType;

typedef enum {
	E_NEW_APPOINTMENT_FLAG_NONE		  = 0,
	E_NEW_APPOINTMENT_FLAG_ALL_DAY		  = 1 << 0,
	E_NEW_APPOINTMENT_FLAG_MEETING		  = 1 << 1,
	E_NEW_APPOINTMENT_FLAG_NO_PAST_DATE	  = 1 << 2,
	E_NEW_APPOINTMENT_FLAG_FORCE_CURRENT_TIME = 1 << 3
} ENewAppointmentFlags;

struct _ECalendarViewClass {
	GtkGridClass parent_class;

	/* Notification signals */
	void		(*popup_event)		(ECalendarView *cal_view,
						 GdkEvent *button_event);
	void		(*selection_changed)	(ECalendarView *cal_view);
	void		(*selected_time_changed)(ECalendarView *cal_view);
	void		(*timezone_changed)	(ECalendarView *cal_view,
						 ICalTimezone *old_zone,
						 ICalTimezone *new_zone);
	void		(*event_changed)	(ECalendarView *day_view,
						 ECalendarViewEvent *event);
	void		(*event_added)		(ECalendarView *day_view,
						 ECalendarViewEvent *event);
	void		(*move_view_range)	(ECalendarView *cal_view,
						 ECalendarViewMoveType move_type,
						 gint64 exact_date);

	/* Virtual methods */
	GSList *	(*get_selected_events)	(ECalendarView *cal_view); /* ECalendarViewSelectionData * */
	gboolean	(*get_selected_time_range)
						(ECalendarView *cal_view,
						 time_t *start_time,
						 time_t *end_time);
	void		(*set_selected_time_range)
						(ECalendarView *cal_view,
						 time_t start_time,
						 time_t end_time);
	gboolean	(*get_visible_time_range)
						(ECalendarView *cal_view,
						 time_t *start_time,
						 time_t *end_time);
	void		(*precalc_visible_time_range)
						(ECalendarView *cal_view,
						 time_t in_start_time,
						 time_t in_end_time,
						 time_t *out_start_time,
						 time_t *out_end_time);
	void		(*update_query)		(ECalendarView *cal_view);
	void		(*open_event)		(ECalendarView *cal_view);
	void		(*paste_text)		(ECalendarView *cal_view);
	gchar *		(*get_description_text)	(ECalendarView *cal_view);
};

GType		e_calendar_view_get_type	(void);
ECalModel *	e_calendar_view_get_model	(ECalendarView *cal_view);
ICalTimezone *	e_calendar_view_get_timezone	(ECalendarView *cal_view);
void		e_calendar_view_set_timezone	(ECalendarView *cal_view,
						 const ICalTimezone *zone);
gint		e_calendar_view_get_time_divisions
						(ECalendarView *cal_view);
void		e_calendar_view_set_time_divisions
						(ECalendarView *cal_view,
						 gint time_divisions);
GtkTargetList *	e_calendar_view_get_copy_target_list
						(ECalendarView *cal_view);
GtkTargetList *	e_calendar_view_get_paste_target_list
						(ECalendarView *cal_view);

GSList *	e_calendar_view_get_selected_events /* ECalendarViewSelectionData * */
						(ECalendarView *cal_view);
gboolean	e_calendar_view_get_selected_time_range
						(ECalendarView *cal_view,
						 time_t *start_time,
						 time_t *end_time);
void		e_calendar_view_set_selected_time_range
						(ECalendarView *cal_view,
						 time_t start_time,
						 time_t end_time);
gboolean	e_calendar_view_get_visible_time_range
						(ECalendarView *cal_view,
						 time_t *start_time,
						 time_t *end_time);
void		e_calendar_view_precalc_visible_time_range
						(ECalendarView *cal_view,
						 time_t in_start_time,
						 time_t in_end_time,
						 time_t *out_start_time,
						 time_t *out_end_time);
void		e_calendar_view_update_query	(ECalendarView *cal_view);
void		e_calendar_view_paste_text	(ECalendarView *cal_view);

void		e_calendar_view_delete_selected_occurrence
						(ECalendarView *cal_view,
						 ECalObjModType mod);
ECompEditor *	e_calendar_view_open_event_with_flags
						(ECalendarView *cal_view,
						 ECalClient *client,
						 ICalComponent *icomp,
						 guint32 flags);

void		e_calendar_view_popup_event	(ECalendarView *cal_view,
						 GdkEvent *button_event);

void		e_calendar_view_add_event	(ECalendarView *cal_view,
						 ECalClient *client,
						 time_t dtstart,
						 ICalTimezone *default_zone,
						 ICalComponent *icomp,
						 gboolean in_top_canvas);
void		e_calendar_view_new_appointment	(ECalendarView *cal_view,
						 guint32 flags); /* bit-or of ENewAppointmentFlags */
void		e_calendar_view_edit_appointment
						(ECalendarView *cal_view,
						 ECalClient *client,
						 ICalComponent *icomp,
						 EEditEventMode mode);
void		e_calendar_view_open_event	(ECalendarView *cal_view);
gchar *		e_calendar_view_get_description_text
						(ECalendarView *cal_view);
void		e_calendar_view_move_view_range	(ECalendarView *cal_view,
						 ECalendarViewMoveType mode_type,
						 time_t exact_date);

gchar *		e_calendar_view_dup_component_summary
						(ICalComponent *icomp);

void		e_calendar_view_component_created_cb
						(ECalModel *model,
						 ECalClient *client,
						 ICalComponent *original_icomp,
						 const gchar *new_uid,
						 gpointer user_data);

void		draw_curved_rectangle		(cairo_t *cr,
						 gdouble x0,
						 gdouble y0,
						 gdouble rect_width,
						 gdouble rect_height,
						 gdouble radius);

GdkRGBA		get_today_background		(GdkRGBA event_background);

gboolean	e_calendar_view_is_editing	(ECalendarView *cal_view);
gboolean	e_calendar_view_get_allow_direct_summary_edit
						(ECalendarView *cal_view);
void		e_calendar_view_set_allow_direct_summary_edit
						(ECalendarView *cal_view,
						 gboolean allow);
gboolean	e_calendar_view_get_allow_event_dnd
						(ECalendarView *cal_view);
void		e_calendar_view_set_allow_event_dnd
						(ECalendarView *cal_view,
						 gboolean allow);

G_END_DECLS

#endif /* E_CALENDAR_VIEW_H */
