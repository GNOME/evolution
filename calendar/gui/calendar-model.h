/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CALENDAR_MODEL_H
#define CALENDAR_MODEL_H

#include <libgnome/gnome-defs.h>
#include <e-table/e-table-model.h>
#include <cal-util/cal-util.h>

BEGIN_GNOME_DECLS



#define TYPE_CALENDAR_MODEL            (calendar_model_get_type ())
#define CALENDAR_MODEL(obj)            (GTK_CHECK_CAST ((obj), TYPE_CALENDAR_MODEL, CalendarModel))
#define CALENDAR_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_CALENDAR_MODEL,	\
					CalendarModelClass))
#define IS_CALENDAR_MODEL(obj)         (GTK_CHECK_TYPE ((obj), TYPE_CALENDAR_MODEL))
#define IS_CALENDAR_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_CALENDAR_MODEL))

typedef struct _CalendarModel CalendarModel;
typedef struct _CalendarModelClass CalendarModelClass;

struct _CalendarModel {
	ETableModel model;

	/* Private data */
	gpointer priv;
};

struct _CalendarModelClass {
	ETableModelClass parent_class;
};

GtkType calendar_model_get_type (void);

CalendarModel *calendar_model_new ();

void calendar_model_set_cal_client (CalendarModel *model, CalClient *client, CalObjType type);



END_GNOME_DECLS

#endif
