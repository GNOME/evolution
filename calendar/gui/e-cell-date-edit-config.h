/*
 *
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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CELL_DATE_EDIT_CONFIG_H_
#define _E_CELL_DATE_EDIT_CONFIG_H_

#include <misc/e-cell-date-edit.h>
#include "e-cell-date-edit-text.h"

G_BEGIN_DECLS

#define E_CELL_DATE_EDIT_CONFIG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_cell_date_edit_config_get_type (), ECellDateEditConfig)
#define E_CELL_DATE_EDIT_CONFIG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_cell_date_edit_config_get_type (), ECellDateEditConfigClass)
#define E_IS_CELL_DATE_EDIT_CONFIG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_cell_date_edit_config_get_type ())

typedef struct _ECellDateEditConfig        ECellDateEditConfig;
typedef struct _ECellDateEditConfigClass   ECellDateEditConfigClass;
typedef struct _ECellDateEditConfigPrivate ECellDateEditConfigPrivate;

struct _ECellDateEditConfig {
	GObject parent;

	ECellDateEditConfigPrivate *priv;
};

struct _ECellDateEditConfigClass {
	GObjectClass parent_class;
};

GType          e_cell_date_edit_config_get_type (void);
ECellDateEditConfig *e_cell_date_edit_config_new (ECellDateEdit *cell);
ECellDateEdit *e_cell_date_edit_config_get_cell (ECellDateEditConfig *cell_config);
void e_cell_date_edit_config_set_cell (ECellDateEditConfig *view_config, ECellDateEdit *cell);

G_END_DECLS

#endif
