/* 
 * e-meeting-store.h
 *
 * Copyright (C) 2003  Ximian, Inc.
 *
 * Author: Mike Kestner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _E_MEETING_STORE_H_
#define _E_MEETING_STORE_H_

#include <gtk/gtkliststore.h>
#include <libecal/e-cal.h>
#include "e-meeting-attendee.h"

G_BEGIN_DECLS

#define E_TYPE_MEETING_STORE		(e_meeting_store_get_type ())
#define E_MEETING_STORE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MEETING_STORE, EMeetingStore))
#define E_MEETING_STORE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MEETING_STORE, EMeetingStoreClass))
#define E_IS_MEETING_STORE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MEETING_STORE))
#define E_IS_MEETING_STORE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MEETING_STORE))

typedef struct _EMeetingStore        EMeetingStore;
typedef struct _EMeetingStorePrivate EMeetingStorePrivate;
typedef struct _EMeetingStoreClass   EMeetingStoreClass;

typedef enum {
	E_MEETING_STORE_ADDRESS_COL,
	E_MEETING_STORE_MEMBER_COL,
	E_MEETING_STORE_TYPE_COL,
	E_MEETING_STORE_ROLE_COL,
	E_MEETING_STORE_RSVP_COL,
	E_MEETING_STORE_DELTO_COL,
	E_MEETING_STORE_DELFROM_COL,
	E_MEETING_STORE_STATUS_COL,
	E_MEETING_STORE_CN_COL,
	E_MEETING_STORE_LANGUAGE_COL,
	E_MEETING_STORE_ATTENDEE_COL,
	E_MEETING_STORE_ATTENDEE_UNDERLINE_COL,
	E_MEETING_STORE_COLUMN_COUNT
} EMeetingStoreColumns;

struct _EMeetingStore {
	GtkListStore parent;

	EMeetingStorePrivate *priv;
};

struct _EMeetingStoreClass {
	GtkListStoreClass parent_class;
};

typedef void	(* EMeetingStoreRefreshCallback) (gpointer data);

GType    e_meeting_store_get_type (void);
GObject *e_meeting_store_new      (void);

void e_meeting_store_set_value (EMeetingStore *im, int row, int col, const gchar *val);

ECal *e_meeting_store_get_e_cal (EMeetingStore *im);
void e_meeting_store_set_e_cal (EMeetingStore *im, ECal *client);

icaltimezone *e_meeting_store_get_zone (EMeetingStore *im);
void e_meeting_store_set_zone (EMeetingStore *im, icaltimezone *zone);

gchar *e_meeting_store_get_fb_uri (EMeetingStore *im);
void e_meeting_store_set_fb_uri (EMeetingStore *im, const gchar *fb_uri);

void e_meeting_store_add_attendee (EMeetingStore *im, EMeetingAttendee *ia);
EMeetingAttendee *e_meeting_store_add_attendee_with_defaults (EMeetingStore *im);

void e_meeting_store_remove_attendee (EMeetingStore *im, EMeetingAttendee *ia);
void e_meeting_store_remove_all_attendees (EMeetingStore *im);

EMeetingAttendee *e_meeting_store_find_attendee (EMeetingStore *im, const gchar *address, gint *row);
EMeetingAttendee *e_meeting_store_find_attendee_at_row (EMeetingStore *im, gint row);
GtkTreePath *e_meeting_store_find_attendee_path (EMeetingStore *store, EMeetingAttendee *attendee);

gint e_meeting_store_count_actual_attendees (EMeetingStore *im);
const GPtrArray *e_meeting_store_get_attendees (EMeetingStore *im);

void e_meeting_store_refresh_all_busy_periods (EMeetingStore *im,
					       EMeetingTime *start,
					       EMeetingTime *end,
					       EMeetingStoreRefreshCallback call_back,
					       gpointer data);
void e_meeting_store_refresh_busy_periods (EMeetingStore *im,
					   int row,
					   EMeetingTime *start,
					   EMeetingTime *end,
					   EMeetingStoreRefreshCallback call_back,
					   gpointer data);


G_END_DECLS

#endif
