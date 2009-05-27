/*
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

#ifndef _E_WEEK_VIEW_CONFIG_H_
#define _E_WEEK_VIEW_CONFIG_H_

#include "e-week-view.h"

G_BEGIN_DECLS

#define E_WEEK_VIEW_CONFIG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_week_view_config_get_type (), EWeekViewConfig)
#define E_WEEK_VIEW_CONFIG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_week_view_config_get_type (), EWeekViewConfigClass)
#define E_IS_WEEK_VIEW_CONFIG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_week_view_config_get_type ())

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
