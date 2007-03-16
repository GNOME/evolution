/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Author : 
 *  JP Rosevear <jpr@ximian.com>
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

#ifndef _E_CELL_DATE_EDIT_CONFIG_H_
#define _E_CELL_DATE_EDIT_CONFIG_H_

#include <misc/e-cell-date-edit.h>
#include "e-cell-date-edit-text.h"

G_BEGIN_DECLS

#define E_CELL_DATE_EDIT_CONFIG(obj)          GTK_CHECK_CAST (obj, e_cell_date_edit_config_get_type (), ECellDateEditConfig)
#define E_CELL_DATE_EDIT_CONFIG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_cell_date_edit_config_get_type (), ECellDateEditConfigClass)
#define E_IS_CELL_DATE_EDIT_CONFIG(obj)       GTK_CHECK_TYPE (obj, e_cell_date_edit_config_get_type ())
        
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
