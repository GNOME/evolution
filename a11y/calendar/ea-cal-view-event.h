/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-cal-view-event.h
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
 *
 */

#ifndef __EA_CAL_VIEW_EVENT_H__
#define __EA_CAL_VIEW_EVENT_H__

#include <atk/atkgobjectaccessible.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EA_TYPE_CAL_VIEW_EVENT                   (ea_cal_view_event_get_type ())
#define EA_CAL_VIEW_EVENT(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_CAL_VIEW_EVENT, EaCalViewEvent))
#define EA_CAL_VIEW_EVENT_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_CAL_VIEW_EVENT, EaCalViewEventClass))
#define EA_IS_CAL_VIEW_EVENT(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_CAL_VIEW_EVENT))
#define EA_IS_CAL_VIEW_EVENT_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_CAL_VIEW_EVENT))
#define EA_CAL_VIEW_EVENT_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_CAL_VIEW_EVENT, EaCalViewEventClass))

typedef struct _EaCalViewEvent                   EaCalViewEvent;
typedef struct _EaCalViewEventClass              EaCalViewEventClass;

struct _EaCalViewEvent
{
	AtkGObjectAccessible parent;
};

GType ea_cal_view_event_get_type (void);

struct _EaCalViewEventClass
{
	AtkGObjectAccessibleClass parent_class;
};

AtkObject *ea_cal_view_event_new (GObject *obj);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EA_CAL_VIEW_EVENT_H__ */
