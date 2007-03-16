/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-week-view-cell.h
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
 * Author: Yang Wu <yang.wu@sun.com> Sun Microsystem Inc., 2003
 *
 */

#ifndef __EA_WEEK_VIEW_CELL_H__
#define __EA_WEEK_VIEW_CELL_H__

#include <atk/atkgobjectaccessible.h>
#include "e-week-view.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TYPE_WEEK_VIEW_CELL                     (e_week_view_cell_get_type ())
#define E_WEEK_VIEW_CELL(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_WEEK_VIEW_CELL, EWeekViewCell))
#define E_WEEK_VIEW_CELL_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_WEEK_VIEW_CELL, EWeekViewCellClass))
#define E_IS_WEEK_VIEW_CELL(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_WEEK_VIEW_CELL))
#define E_IS_WEEK_VIEW_CELL_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_WEEK_VIEW_CELL))
#define E_WEEK_VIEW_CELL_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_WEEK_VIEW_CELL, EWeekViewCellClass))

typedef struct _EWeekViewCell                   EWeekViewCell;
typedef struct _EWeekViewCellClass              EWeekViewCellClass;

struct _EWeekViewCell
{
	GObject parent;
	EWeekView *week_view;
	gint row;
	gint column;
};

GType e_week_view_cell_get_type (void);

struct _EWeekViewCellClass
{
	GObjectClass parent_class;
};

EWeekViewCell * e_week_view_cell_new (EWeekView *week_view, gint row, gint column);

#define EA_TYPE_WEEK_VIEW_CELL                     (ea_week_view_cell_get_type ())
#define EA_WEEK_VIEW_CELL(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_WEEK_VIEW_CELL, EaWeekViewCell))
#define EA_WEEK_VIEW_CELL_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_WEEK_VIEW_CELL, EaWeekViewCellClass))
#define EA_IS_WEEK_VIEW_CELL(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_WEEK_VIEW_CELL))
#define EA_IS_WEEK_VIEW_CELL_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_WEEK_VIEW_CELL))
#define EA_WEEK_VIEW_CELL_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_WEEK_VIEW_CELL, EaWeekViewCellClass))

typedef struct _EaWeekViewCell                   EaWeekViewCell;
typedef struct _EaWeekViewCellClass              EaWeekViewCellClass;

struct _EaWeekViewCell
{
	AtkGObjectAccessible parent;
};

GType ea_week_view_cell_get_type (void);

struct _EaWeekViewCellClass
{
	AtkGObjectAccessibleClass parent_class;
};

AtkObject*     ea_week_view_cell_new         (GObject *gobj);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EA_WEEK_VIEW_CELL_H__ */
