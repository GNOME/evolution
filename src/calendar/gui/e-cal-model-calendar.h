/*
 *
 * Evolution calendar - Data model for ETable
 *
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

#ifndef E_CAL_MODEL_CALENDAR_H
#define E_CAL_MODEL_CALENDAR_H

#include "e-cal-model.h"

/* Standard GObject macros */
#define E_TYPE_CAL_MODEL_CALENDAR \
	(e_cal_model_calendar_get_type ())
#define E_CAL_MODEL_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_MODEL_CALENDAR, ECalModelCalendar))
#define E_CAL_MODEL_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_MODEL_CALENDAR, ECalModelCalendarClass))
#define E_IS_CAL_MODEL_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_MODEL_CALENDAR))
#define E_IS_CAL_MODEL_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_MODEL_CALENDAR))
#define E_CAL_MODEL_CALENDAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_MODEL_CALENDAR, ECalModelCalendarClass))

G_BEGIN_DECLS

typedef struct _ECalModelCalendar ECalModelCalendar;
typedef struct _ECalModelCalendarClass ECalModelCalendarClass;
typedef struct _ECalModelCalendarPrivate ECalModelCalendarPrivate;

typedef enum {
	/* If you add new items here or reorder them, you have to update the
	 * .etspec files for the tables using this model */
	E_CAL_MODEL_CALENDAR_FIELD_DTEND = E_CAL_MODEL_FIELD_LAST,
	E_CAL_MODEL_CALENDAR_FIELD_LOCATION,
	E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY,
	E_CAL_MODEL_CALENDAR_FIELD_STATUS,
	E_CAL_MODEL_CALENDAR_FIELD_LAST
} ECalModelCalendarField;

struct _ECalModelCalendar {
	ECalModel parent;
	ECalModelCalendarPrivate *priv;
};

struct _ECalModelCalendarClass {
	ECalModelClass parent_class;
};

GType		e_cal_model_calendar_get_type	(void);
ECalModel *	e_cal_model_calendar_new	(ECalDataModel *data_model,
						 ESourceRegistry *registry,
						 EShell *shell);

G_END_DECLS

#endif /* E_CAL_MODEL_CALENDAR_H */
