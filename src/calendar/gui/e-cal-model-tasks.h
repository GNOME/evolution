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

#ifndef E_CAL_MODEL_TASKS_H
#define E_CAL_MODEL_TASKS_H

#include "e-cal-model.h"

/* Standard GObject macros */
#define E_TYPE_CAL_MODEL_TASKS \
	(e_cal_model_tasks_get_type ())
#define E_CAL_MODEL_TASKS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_MODEL_TASKS, ECalModelTasks))
#define E_CAL_MODEL_TASKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_MODEL_TASKS, ECalModelTasksClass))
#define E_IS_CAL_MODEL_TASKS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_MODEL_TASKS))
#define E_IS_CAL_MODEL_TASKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_MODEL_TASKS))
#define E_CAL_MODEL_TASKS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_MODEL_TASKS, ECalModelTasksClass))

G_BEGIN_DECLS

typedef struct _ECalModelTasks ECalModelTasks;
typedef struct _ECalModelTasksClass ECalModelTasksClass;
typedef struct _ECalModelTasksPrivate ECalModelTasksPrivate;

typedef enum {
	/* If you add new items here or reorder them, you have to update the
	 * .etspec files for the tables using this model */
	E_CAL_MODEL_TASKS_FIELD_COMPLETED = E_CAL_MODEL_FIELD_LAST,
	E_CAL_MODEL_TASKS_FIELD_COMPLETE,
	E_CAL_MODEL_TASKS_FIELD_DUE,
	E_CAL_MODEL_TASKS_FIELD_GEO,
	E_CAL_MODEL_TASKS_FIELD_OVERDUE,
	E_CAL_MODEL_TASKS_FIELD_PERCENT,
	E_CAL_MODEL_TASKS_FIELD_PRIORITY,
	E_CAL_MODEL_TASKS_FIELD_STATUS,
	E_CAL_MODEL_TASKS_FIELD_URL,
	E_CAL_MODEL_TASKS_FIELD_STRIKEOUT, /* it's another virtual readonly column */
	E_CAL_MODEL_TASKS_FIELD_LOCATION,
	E_CAL_MODEL_TASKS_FIELD_ESTIMATED_DURATION,
	E_CAL_MODEL_TASKS_FIELD_LAST
} ECalModelTasksField;

struct _ECalModelTasks {
	ECalModel parent;
	ECalModelTasksPrivate *priv;
};

struct _ECalModelTasksClass {
	ECalModelClass parent_class;
};

GType		e_cal_model_tasks_get_type	(void);
ECalModel *	e_cal_model_tasks_new		(ECalDataModel *data_model,
						 ESourceRegistry *registry,
						 EShell *shell);
gboolean	e_cal_model_tasks_get_highlight_due_today
						(ECalModelTasks *model);
void		e_cal_model_tasks_set_highlight_due_today
						(ECalModelTasks *model,
						 gboolean highlight);
const gchar *	e_cal_model_tasks_get_color_due_today
						(ECalModelTasks *model);
void		e_cal_model_tasks_set_color_due_today
						(ECalModelTasks *model,
						 const gchar *color_due_today);
gboolean	e_cal_model_tasks_get_highlight_overdue
						(ECalModelTasks *model);
void		e_cal_model_tasks_set_highlight_overdue
						(ECalModelTasks *model,
						 gboolean highlight);
const gchar *	e_cal_model_tasks_get_color_overdue
						(ECalModelTasks *model);
void		e_cal_model_tasks_set_color_overdue
						(ECalModelTasks *model,
						 const gchar *color_overdue);
void		e_cal_model_tasks_mark_comp_complete
						(ECalModelTasks *model,
						 ECalModelComponent *comp_data);
void		e_cal_model_tasks_mark_comp_incomplete
						(ECalModelTasks *model,
						 ECalModelComponent *comp_data);
void		e_cal_model_tasks_update_due_tasks
						(ECalModelTasks *model);

G_END_DECLS

#endif /* E_CAL_MODEL_TASKS_H */
