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

#ifndef __EA_DAY_VIEW_MAIN_ITEM_H__
#define __EA_DAY_VIEW_MAIN_ITEM_H__

#include <atk/atkgobjectaccessible.h>
#include "e-day-view-main-item.h"
#include <libgnomecanvas/gailcanvasitem.h>

G_BEGIN_DECLS

#define EA_TYPE_DAY_VIEW_MAIN_ITEM                     (ea_day_view_main_item_get_type ())
#define EA_DAY_VIEW_MAIN_ITEM(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_DAY_VIEW_MAIN_ITEM, EaDayViewMainItem))
#define EA_DAY_VIEW_MAIN_ITEM_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_DAY_VIEW_MAIN_ITEM, EaDayViewMainItemClass))
#define EA_IS_DAY_VIEW_MAIN_ITEM(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_DAY_VIEW_MAIN_ITEM))
#define EA_IS_DAY_VIEW_MAIN_ITEM_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_DAY_VIEW_MAIN_ITEM))
#define EA_DAY_VIEW_MAIN_ITEM_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_DAY_VIEW_MAIN_ITEM, EaDayViewMainItemClass))

typedef struct _EaDayViewMainItem                   EaDayViewMainItem;
typedef struct _EaDayViewMainItemClass              EaDayViewMainItemClass;

struct _EaDayViewMainItem
{
	GailCanvasItem parent;
};

GType ea_day_view_main_item_get_type (void);

struct _EaDayViewMainItemClass
{
	GailCanvasItemClass parent_class;
};

AtkObject *     ea_day_view_main_item_new         (GObject *obj);

G_END_DECLS

#endif /* __EA_DAY_VIEW_MAIN_ITEM_H__ */
