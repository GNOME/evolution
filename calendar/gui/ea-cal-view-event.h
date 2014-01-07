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

#ifndef __EA_CAL_VIEW_EVENT_H__
#define __EA_CAL_VIEW_EVENT_H__

#include <atk/atkgobjectaccessible.h>

G_BEGIN_DECLS

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
	AtkStateSet *state_set;
};

GType ea_cal_view_event_get_type (void);

struct _EaCalViewEventClass
{
	AtkGObjectAccessibleClass parent_class;
};

AtkObject *ea_cal_view_event_new (GObject *obj);

G_END_DECLS

#endif /* __EA_CAL_VIEW_EVENT_H__ */
