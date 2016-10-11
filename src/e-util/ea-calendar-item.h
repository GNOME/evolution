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

#ifndef __EA_CALENDAR_ITEM_H__
#define __EA_CALENDAR_ITEM_H__

#include <atk/atkgobjectaccessible.h>
#include <e-util/e-calendar-item.h>

G_BEGIN_DECLS

#define EA_TYPE_CALENDAR_ITEM                   (ea_calendar_item_get_type ())
#define EA_CALENDAR_ITEM(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_CALENDAR_ITEM, EaCalendarItem))
#define EA_CALENDAR_ITEM_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_CALENDAR_ITEM, EaCalendarItemClass))
#define EA_IS_CALENDAR_ITEM(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_CALENDAR_ITEM))
#define EA_IS_CALENDAR_ITEM_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_CALENDAR_ITEM))
#define EA_CALENDAR_ITEM_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_CALENDAR_ITEM, EaCalendarItemClass))

typedef struct _EaCalendarItem                   EaCalendarItem;
typedef struct _EaCalendarItemClass              EaCalendarItemClass;

struct _EaCalendarItem
{
	AtkGObjectAccessible parent;
};

GType ea_calendar_item_get_type (void);

struct _EaCalendarItemClass
{
	AtkGObjectAccessibleClass parent_class;
};

AtkObject *ea_calendar_item_new (GObject *obj);
gboolean e_calendar_item_get_day_extents (ECalendarItem *calitem,
					  gint year, gint month, gint date,
					  gint *x, gint *y,
					  gint *width, gint *height);
gboolean e_calendar_item_get_date_for_offset (ECalendarItem *calitem,
					      gint day_offset,
					      gint *year, gint *month,
					      gint *day);
gboolean e_calendar_item_get_date_for_cell (ECalendarItem *calitem,
					      gint row,
					      gint column,
					      gint *year, gint *month,
					      gint *day);
gint e_calendar_item_get_n_days_from_week_start (ECalendarItem *calitem,
						 gint year, gint month);

G_END_DECLS

#endif /* __EA_CALENDAR_ITEM_H__ */
