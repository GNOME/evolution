/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-model.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 *
 * Author: JP Rosevear
 */

#ifndef _E_MEETING_MODEL_H_
#define _E_MEETING_MODEL_H_

#include <gtk/gtk.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-table-model.h>
#include <cal-client/cal-client.h>
#include "e-meeting-attendee.h"

G_BEGIN_DECLS

#define E_TYPE_MEETING_MODEL			(e_meeting_model_get_type ())
#define E_MEETING_MODEL(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_MEETING_MODEL, EMeetingModel))
#define E_MEETING_MODEL_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MEETING_MODEL, EMeetingModelClass))
#define E_IS_MEETING_MODEL(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_MEETING_MODEL))
#define E_IS_MEETING_MODEL_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_MEETING_MODEL))


typedef struct _EMeetingModel        EMeetingModel;
typedef struct _EMeetingModelPrivate EMeetingModelPrivate;
typedef struct _EMeetingModelClass   EMeetingModelClass;

typedef enum {
	E_MEETING_MODEL_ADDRESS_COL,
	E_MEETING_MODEL_MEMBER_COL,
	E_MEETING_MODEL_TYPE_COL,
	E_MEETING_MODEL_ROLE_COL,
	E_MEETING_MODEL_RSVP_COL,
	E_MEETING_MODEL_DELTO_COL,
	E_MEETING_MODEL_DELFROM_COL,
	E_MEETING_MODEL_STATUS_COL,
	E_MEETING_MODEL_CN_COL,
	E_MEETING_MODEL_LANGUAGE_COL,
	E_MEETING_MODEL_COLUMN_COUNT
} EMeetingModelColumns;

struct _EMeetingModel {
	ETableModel parent;

	EMeetingModelPrivate *priv;
};

struct _EMeetingModelClass {
	ETableModelClass parent_class;
};

typedef void	(* EMeetingModelRefreshCallback) (gpointer data);


GtkType    e_meeting_model_get_type (void);
GtkObject *e_meeting_model_new      (void);

CalClient *e_meeting_model_get_cal_client (EMeetingModel *im);
void e_meeting_model_set_cal_client (EMeetingModel *im, CalClient *client);

icaltimezone *e_meeting_model_get_zone (EMeetingModel *im);
void e_meeting_model_set_zone (EMeetingModel *im, icaltimezone *zone);

void e_meeting_model_add_attendee (EMeetingModel *im, EMeetingAttendee *ia);
EMeetingAttendee *e_meeting_model_add_attendee_with_defaults (EMeetingModel *im);

void e_meeting_model_remove_attendee (EMeetingModel *im, EMeetingAttendee *ia);
void e_meeting_model_remove_all_attendees (EMeetingModel *im);

EMeetingAttendee *e_meeting_model_find_attendee (EMeetingModel *im, const gchar *address, gint *row);
EMeetingAttendee *e_meeting_model_find_attendee_at_row (EMeetingModel *im, gint row);

gint e_meeting_model_count_actual_attendees (EMeetingModel *im);
const GPtrArray *e_meeting_model_get_attendees (EMeetingModel *im);

void e_meeting_model_refresh_all_busy_periods (EMeetingModel *im,
					       EMeetingTime *start,
					       EMeetingTime *end,
					       EMeetingModelRefreshCallback call_back,
					       gpointer data);
void e_meeting_model_refresh_busy_periods (EMeetingModel *im,
					   int row,
					   EMeetingTime *start,
					   EMeetingTime *end,
					   EMeetingModelRefreshCallback call_back,
					   gpointer data);


/* Helpful functions */
ETableScrolled    *e_meeting_model_etable_from_model (EMeetingModel *im, const gchar *spec_file, const gchar *state_file);
void e_meeting_model_etable_click_to_add (EMeetingModel *im, gboolean click_to_add);
int e_meeting_model_etable_model_to_view_row (ETable *et, EMeetingModel *im, int model_row);
int e_meeting_model_etable_view_to_model_row (ETable *et, EMeetingModel *im, int view_row);

void e_meeting_model_invite_others_dialog (EMeetingModel *im);

G_END_DECLS

#endif
