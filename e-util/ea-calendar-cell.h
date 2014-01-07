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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __EA_CALENDAR_CELL_H__
#define __EA_CALENDAR_CELL_H__

#include <atk/atkgobjectaccessible.h>
#include <e-util/e-calendar-item.h>

G_BEGIN_DECLS

#define E_TYPE_CALENDAR_CELL                     (e_calendar_cell_get_type ())
#define E_CALENDAR_CELL(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CALENDAR_CELL, ECalendarCell))
#define E_CALENDAR_CELL_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CALENDAR_CELL, ECalendarCellClass))
#define E_IS_CALENDAR_CELL(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CALENDAR_CELL))
#define E_IS_CALENDAR_CELL_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CALENDAR_CELL))
#define E_CALENDAR_CELL_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CALENDAR_CELL, ECalendarCellClass))

typedef struct _ECalendarCell                   ECalendarCell;
typedef struct _ECalendarCellClass              ECalendarCellClass;

struct _ECalendarCell
{
	GObject parent;
	ECalendarItem *calitem;
	gint row;
	gint column;
};

GType e_calendar_cell_get_type (void);

struct _ECalendarCellClass
{
	GObjectClass parent_class;
};

ECalendarCell * e_calendar_cell_new (ECalendarItem *calitem,
				     gint row, gint column);

#define EA_TYPE_CALENDAR_CELL                     (ea_calendar_cell_get_type ())
#define EA_CALENDAR_CELL(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_CALENDAR_CELL, EaCalendarCell))
#define EA_CALENDAR_CELL_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_CALENDAR_CELL, EaCalendarCellClass))
#define EA_IS_CALENDAR_CELL(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_CALENDAR_CELL))
#define EA_IS_CALENDAR_CELL_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_CALENDAR_CELL))
#define EA_CALENDAR_CELL_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_CALENDAR_CELL, EaCalendarCellClass))

typedef struct _EaCalendarCell                   EaCalendarCell;
typedef struct _EaCalendarCellClass              EaCalendarCellClass;

struct _EaCalendarCell
{
	AtkGObjectAccessible parent;
	AtkStateSet *state_set;
};

GType ea_calendar_cell_get_type (void);

struct _EaCalendarCellClass
{
	AtkGObjectAccessibleClass parent_class;
};

AtkObject *     ea_calendar_cell_new         (GObject *gobj);

G_END_DECLS

#endif /* __EA_CALENDAR_CELL_H__ */
