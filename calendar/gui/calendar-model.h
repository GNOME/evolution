/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CALENDAR_MODEL_H
#define CALENDAR_MODEL_H

#include <gal/e-table/e-table-model.h>
#include <cal-client/cal-client.h>

G_BEGIN_DECLS



#define TYPE_CALENDAR_MODEL            (calendar_model_get_type ())
#define CALENDAR_MODEL(obj)            (GTK_CHECK_CAST ((obj), TYPE_CALENDAR_MODEL, CalendarModel))
#define CALENDAR_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_CALENDAR_MODEL,	\
					CalendarModelClass))
#define IS_CALENDAR_MODEL(obj)         (GTK_CHECK_TYPE ((obj), TYPE_CALENDAR_MODEL))
#define IS_CALENDAR_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_CALENDAR_MODEL))

typedef struct _CalendarModel CalendarModel;
typedef struct _CalendarModelClass CalendarModelClass;

typedef struct _CalendarModelPrivate CalendarModelPrivate;

struct _CalendarModel {
	ETableModel model;

	/* Private data */
	CalendarModelPrivate *priv;
};

struct _CalendarModelClass {
	ETableModelClass parent_class;
};

GtkType		calendar_model_get_type		  (void);

CalendarModel*	calendar_model_new		  (void);

CalClient*	calendar_model_get_cal_client	  (CalendarModel   *model);
void		calendar_model_set_cal_client	  (CalendarModel   *model,
						   CalClient	   *client,
						   CalObjType	    type);

void		calendar_model_set_query	  (CalendarModel   *model,
						   const char      *sexp);

void		calendar_model_refresh		  (CalendarModel   *model);

void		calendar_model_set_new_comp_vtype  (CalendarModel     *model,
						    CalComponentVType  vtype);
CalComponentVType calendar_model_get_new_comp_vtype (CalendarModel    *model);

void		calendar_model_mark_task_complete (CalendarModel   *model,
						   gint		    row);

CalComponent*	calendar_model_get_component	  (CalendarModel   *model,
						   gint		    row);

/* Whether we use 24 hour format to display the times. */
gboolean	calendar_model_get_use_24_hour_format (CalendarModel *model);
void		calendar_model_set_use_24_hour_format (CalendarModel *model,
						       gboolean	      use_24_hour_format);

/* The current timezone. */
icaltimezone*	calendar_model_get_timezone	    (CalendarModel *model);
void		calendar_model_set_timezone	    (CalendarModel *model,
						     icaltimezone  *zone);

void		calendar_model_set_default_category (CalendarModel	*model,
						     const char		*default_category);

void            calendar_model_set_status_message   (CalendarModel      *model,
						     const char         *message);



G_END_DECLS

#endif
