/*
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
 *		Bolian Yin <bolian.yin@sun.com>
 *		Yang Wu <yang.wu@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EA_WEEK_VIEW_CELL_H__
#define __EA_WEEK_VIEW_CELL_H__

#include <atk/atkgobjectaccessible.h>
#include "e-week-view.h"

G_BEGIN_DECLS

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

AtkObject *     ea_week_view_cell_new         (GObject *gobj);

G_END_DECLS

#endif /* __EA_WEEK_VIEW_CELL_H__ */
