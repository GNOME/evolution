/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
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

#ifndef E_CAL_MODEL_TASKS_H
#define E_CAL_MODEL_TASKS_H

#include "e-cal-model.h"

G_BEGIN_DECLS

#define E_TYPE_CAL_MODEL_TASKS            (e_cal_model_tasks_get_type ())
#define E_CAL_MODEL_TASKS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_MODEL_TASKS, ECalModelTasks))
#define E_CAL_MODEL_TASKS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_MODEL_TASKS, ECalModelTasksClass))
#define E_IS_CAL_MODEL_TASKS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_MODEL_TASKS))
#define E_IS_CAL_MODEL_TASKS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_MODEL_TASKS))

typedef struct _ECalModelTasksPrivate ECalModelTasksPrivate;

typedef enum {
	/* If you add new items here or reorder them, you have to update the
	   .etspec files for the tables using this model */
	E_CAL_MODEL_TASKS_FIELD_COMPLETED = E_CAL_MODEL_FIELD_LAST,
	E_CAL_MODEL_TASKS_FIELD_COMPLETE,
	E_CAL_MODEL_TASKS_FIELD_DUE,
	E_CAL_MODEL_TASKS_FIELD_GEO,
	E_CAL_MODEL_TASKS_FIELD_OVERDUE,
	E_CAL_MODEL_TASKS_FIELD_PERCENT,
	E_CAL_MODEL_TASKS_FIELD_PRIORITY,
	E_CAL_MODEL_TASKS_FIELD_STATUS,
	E_CAL_MODEL_TASKS_FIELD_URL,
	E_CAL_MODEL_TASKS_FIELD_LAST
} ECalModelTasksField;

typedef struct {
	ECalModel model;
	ECalModelTasksPrivate *priv;
} ECalModelTasks;

typedef struct {
	ECalModelClass parent_class;
} ECalModelTasksClass;

GType           e_cal_model_tasks_get_type (void);
ECalModelTasks *e_cal_model_tasks_new (void);

void            e_cal_model_tasks_mark_task_complete (ECalModelTasks *model, gint model_row);
void e_cal_model_tasks_update_due_tasks (ECalModelTasks *model);

G_END_DECLS

#endif
