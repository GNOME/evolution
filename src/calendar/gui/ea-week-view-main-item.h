/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Bolian Yin <bolian.yin@sun.com>
 * SPDX-FileContributor: Yang Wu <Yang.Wu@sun.com>
 */

#ifndef __EA_WEEK_VIEW_MAIN_ITEM_H__
#define __EA_WEEK_VIEW_MAIN_ITEM_H__

#include <atk/atkgobjectaccessible.h>
#include "e-week-view-main-item.h"
#include <libgnomecanvas/gailcanvasitem.h>

G_BEGIN_DECLS

#define EA_TYPE_WEEK_VIEW_MAIN_ITEM                     (ea_week_view_main_item_get_type ())
#define EA_WEEK_VIEW_MAIN_ITEM(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_WEEK_VIEW_MAIN_ITEM, EaWeekViewMainItem))
#define EA_WEEK_VIEW_MAIN_ITEM_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_WEEK_VIEW_MAIN_ITEM, EaWeekViewMainItemClass))
#define EA_IS_WEEK_VIEW_MAIN_ITEM(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_WEEK_VIEW_MAIN_ITEM))
#define EA_IS_WEEK_VIEW_MAIN_ITEM_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_WEEK_VIEW_MAIN_ITEM))
#define EA_WEEK_VIEW_MAIN_ITEM_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_WEEK_VIEW_MAIN_ITEM, EaWeekViewMainItemClass))

typedef struct _EaWeekViewMainItem                   EaWeekViewMainItem;
typedef struct _EaWeekViewMainItemClass              EaWeekViewMainItemClass;

struct _EaWeekViewMainItem
{
	GailCanvasItem parent;
};

GType ea_week_view_main_item_get_type (void);

struct _EaWeekViewMainItemClass
{
	GailCanvasItemClass parent_class;
};

AtkObject *     ea_week_view_main_item_new         (GObject *obj);

G_END_DECLS

#endif /* __EA_WEEK_VIEW_MAIN_ITEM_H__ */
