/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
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
#ifndef _E_CAL_VIEW_H_
#define _E_CAL_VIEW_H_

#include <cal-client/cal-client.h>
#include <gtk/gtktable.h>
#include "gnome-cal.h"

G_BEGIN_DECLS

/*
 * EView - base widget class for the calendar views.
 */

#define E_CAL_VIEW(obj)          GTK_CHECK_CAST (obj, e_cal_view_get_type (), ECalView)
#define E_CAL_VIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_cal_view_get_type (), ECalViewClass)
#define E_IS_CAL_VIEW(obj)       GTK_CHECK_TYPE (obj, e_cal_view_get_type ())

typedef enum {
	E_CAL_VIEW_POS_OUTSIDE,
	E_CAL_VIEW_POS_NONE,
	E_CAL_VIEW_POS_EVENT,
	E_CAL_VIEW_POS_LEFT_EDGE,
	E_CAL_VIEW_POS_RIGHT_EDGE,
	E_CAL_VIEW_POS_TOP_EDGE,
	E_CAL_VIEW_POS_BOTTOM_EDGE
} ECalViewPosition;

#define E_CAL_VIEW_EVENT_FIELDS \
        GnomeCanvasItem *canvas_item; \
        CalClient *client; \
        CalComponent *comp; \
        time_t start; \
        time_t end; \
        guint16 start_minute; \
        guint16 end_minute; \
        guint different_timezone : 1;

typedef struct {
	E_CAL_VIEW_EVENT_FIELDS
} ECalViewEvent;
        
typedef struct _ECalView        ECalView;
typedef struct _ECalViewClass   ECalViewClass;
typedef struct _ECalViewPrivate ECalViewPrivate;

struct _ECalView {
	GtkTable table;
	ECalViewPrivate *priv;
};

struct _ECalViewClass {
	GtkTableClass parent_class;

	/* Notification signals */
	void (* selection_changed) (ECalView *cal_view);
	void (* timezone_changed) (ECalView *cal_view, icaltimezone *old_zone, icaltimezone *new_zone);

	/* Virtual methods */
	GList * (* get_selected_events) (ECalView *cal_view); /* a GList of ECalViewEvent's */
	void (* get_selected_time_range) (ECalView *cal_view, time_t *start_time, time_t *end_time);
	void (* set_selected_time_range) (ECalView *cal_view, time_t start_time, time_t end_time);
	gboolean (* get_visible_time_range) (ECalView *cal_view, time_t *start_time, time_t *end_time);
	void (* update_query) (ECalView *cal_view);
};

GType          e_cal_view_get_type (void);

GnomeCalendar *e_cal_view_get_calendar (ECalView *cal_view);
void           e_cal_view_set_calendar (ECalView *cal_view, GnomeCalendar *calendar);
CalClient     *e_cal_view_get_cal_client (ECalView *cal_view);
void           e_cal_view_set_cal_client (ECalView *cal_view, CalClient *client);
const gchar   *e_cal_view_get_query (ECalView *cal_view);
void           e_cal_view_set_query (ECalView *cal_view, const gchar *sexp);
icaltimezone  *e_cal_view_get_timezone (ECalView *cal_view);
void           e_cal_view_set_timezone (ECalView *cal_view, icaltimezone *zone);

void           e_cal_view_set_status_message (ECalView *cal_view, const gchar *message);

GList         *e_cal_view_get_selected_events (ECalView *cal_view);
void           e_cal_view_get_selected_time_range (ECalView *cal_view, time_t *start_time, time_t *end_time);
void           e_cal_view_set_selected_time_range (ECalView *cal_view, time_t start_time, time_t end_time);
gboolean       e_cal_view_get_visible_time_range (ECalView *cal_view, time_t *start_time, time_t *end_time);
void           e_cal_view_update_query (ECalView *cal_view);

void           e_cal_view_cut_clipboard (ECalView *cal_view);
void           e_cal_view_copy_clipboard (ECalView *cal_view);
void           e_cal_view_paste_clipboard (ECalView *cal_view);
void           e_cal_view_delete_selected_event (ECalView *cal_view);
void           e_cal_view_delete_selected_events (ECalView *cal_view);
void           e_cal_view_delete_selected_occurrence (ECalView *cal_view);

GtkMenu       *e_cal_view_create_popup_menu (ECalView *cal_view);

G_END_DECLS

#endif
