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

#ifndef _E_CAL_LIST_VIEW_CONFIG_H_
#define _E_CAL_LIST_VIEW_CONFIG_H_

#include "e-cal-list-view.h"

G_BEGIN_DECLS

#define E_CAL_LIST_VIEW_CONFIG(obj)          GTK_CHECK_CAST (obj, e_cal_list_view_config_get_type (), ECalListViewConfig)
#define E_CAL_LIST_VIEW_CONFIG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_cal_list_view_config_get_type (), ECalListViewConfigClass)
#define E_IS_CAL_LIST_VIEW_CONFIG(obj)       GTK_CHECK_TYPE (obj, e_cal_list_view_config_get_type ())
        
typedef struct _ECalListViewConfig        ECalListViewConfig;
typedef struct _ECalListViewConfigClass   ECalListViewConfigClass;
typedef struct _ECalListViewConfigPrivate ECalListViewConfigPrivate;

struct _ECalListViewConfig {
	GObject parent;

	ECalListViewConfigPrivate *priv;
};

struct _ECalListViewConfigClass {
	GObjectClass parent_class;
};

GType          e_cal_list_view_config_get_type (void);
ECalListViewConfig *e_cal_list_view_config_new (ECalListView *list_view);
ECalListView *e_cal_list_view_config_get_view (ECalListViewConfig *view_config);
void e_cal_list_view_config_set_view (ECalListViewConfig *view_config, ECalListView *list_view);

G_END_DECLS

#endif
