/*
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
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EA_DAY_VIEW_CELL_H__
#define __EA_DAY_VIEW_CELL_H__

#include <atk/atkgobjectaccessible.h>
#include "e-day-view.h"

G_BEGIN_DECLS

#define E_TYPE_DAY_VIEW_CELL                     (e_day_view_cell_get_type ())
#define E_DAY_VIEW_CELL(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_DAY_VIEW_CELL, EDayViewCell))
#define E_DAY_VIEW_CELL_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_DAY_VIEW_CELL, EDayViewCellClass))
#define E_IS_DAY_VIEW_CELL(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_DAY_VIEW_CELL))
#define E_IS_DAY_VIEW_CELL_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_DAY_VIEW_CELL))
#define E_DAY_VIEW_CELL_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_DAY_VIEW_CELL, EDayViewCellClass))

typedef struct _EDayViewCell                   EDayViewCell;
typedef struct _EDayViewCellClass              EDayViewCellClass;

struct _EDayViewCell
{
	GObject parent;
	EDayView *day_view;
	gint row;
	gint column;
};

GType e_day_view_cell_get_type (void);

struct _EDayViewCellClass
{
	GObjectClass parent_class;
};

EDayViewCell * e_day_view_cell_new (EDayView *day_view, gint row, gint column);

#define EA_TYPE_DAY_VIEW_CELL                     (ea_day_view_cell_get_type ())
#define EA_DAY_VIEW_CELL(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_DAY_VIEW_CELL, EaDayViewCell))
#define EA_DAY_VIEW_CELL_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_DAY_VIEW_CELL, EaDayViewCellClass))
#define EA_IS_DAY_VIEW_CELL(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_DAY_VIEW_CELL))
#define EA_IS_DAY_VIEW_CELL_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_DAY_VIEW_CELL))
#define EA_DAY_VIEW_CELL_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_DAY_VIEW_CELL, EaDayViewCellClass))

typedef struct _EaDayViewCell                   EaDayViewCell;
typedef struct _EaDayViewCellClass              EaDayViewCellClass;

struct _EaDayViewCell
{
	AtkGObjectAccessible parent;
};

GType ea_day_view_cell_get_type (void);

struct _EaDayViewCellClass
{
	AtkGObjectAccessibleClass parent_class;
};

AtkObject *     ea_day_view_cell_new         (GObject *gobj);

G_END_DECLS

#endif /* __EA_DAY_VIEW_CELL_H__ */
