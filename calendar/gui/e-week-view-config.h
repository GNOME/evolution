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

#ifndef _E_WEEK_VIEW_CONFIG_H_
#define _E_WEEK_VIEW_CONFIG_H_

#include "e-week-view.h"

G_BEGIN_DECLS

#define E_WEEK_VIEW_CONFIG(obj)          GTK_CHECK_CAST (obj, e_week_view_config_get_type (), EWeekViewConfig)
#define E_WEEK_VIEW_CONFIG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_week_view_config_get_type (), EWeekViewConfigClass)
#define E_IS_WEEK_VIEW_CONFIG(obj)       GTK_CHECK_TYPE (obj, e_week_view_config_get_type ())
        
typedef struct _EWeekViewConfig        EWeekViewConfig;
typedef struct _EWeekViewConfigClass   EWeekViewConfigClass;
typedef struct _EWeekViewConfigPrivate EWeekViewConfigPrivate;

struct _EWeekViewConfig {
	GObject parent;

	EWeekViewConfigPrivate *priv;
};

struct _EWeekViewConfigClass {
	GObjectClass parent_class;
};

GType          e_week_view_config_get_type (void);
EWeekViewConfig *e_week_view_config_new (EWeekView *week_view);
EWeekView *e_week_view_config_get_view (EWeekViewConfig *view_config);
void e_week_view_config_set_view (EWeekViewConfig *view_config, EWeekView *week_view);

G_END_DECLS

#endif
