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

#ifndef _E_DAY_VIEW_CONFIG_H_
#define _E_DAY_VIEW_CONFIG_H_

#include "e-day-view.h"

G_BEGIN_DECLS

/*
 * EView - base widget class for the calendar views.
 */

#define E_DAY_VIEW_CONFIG(obj)          GTK_CHECK_CAST (obj, e_day_view_config_get_type (), EDayViewConfig)
#define E_DAY_VIEW_CONFIG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_day_view_config_get_type (), EDayViewConfigClass)
#define E_IS_DAY_VIEW_CONFIG(obj)       GTK_CHECK_TYPE (obj, e_day_view_config_get_type ())
        
typedef struct _EDayViewConfig        EDayViewConfig;
typedef struct _EDayViewConfigClass   EDayViewConfigClass;
typedef struct _EDayViewConfigPrivate EDayViewConfigPrivate;

struct _EDayViewConfig {
	ECalView parent;

	EDayViewConfigPrivate *priv;
};

struct _EDayViewConfigClass {
	ECalViewClass parent_class;
};

GType          e_day_view_config_get_type (void);
EDayViewConfig *e_day_view_config_new (EDayView *day_view);
EDayView *e_day_view_config_get_view (EDayViewConfig *view_config);
void e_day_view_config_set_view (EDayViewConfig *view_config, EDayView *day_view);

G_END_DECLS

#endif
