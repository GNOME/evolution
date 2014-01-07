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

#ifndef __EA_WEEK_VIEW_H__
#define __EA_WEEK_VIEW_H__

#include "ea-cal-view.h"
#include "e-week-view.h"

G_BEGIN_DECLS

#define EA_TYPE_WEEK_VIEW                     (ea_week_view_get_type ())
#define EA_WEEK_VIEW(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_WEEK_VIEW, EaWeekView))
#define EA_WEEK_VIEW_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_WEEK_VIEW, EaWeekViewClass))
#define EA_IS_WEEK_VIEW(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_WEEK_VIEW))
#define EA_IS_WEEK_VIEW_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_WEEK_VIEW))
#define EA_WEEK_VIEW_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_WEEK_VIEW, EaWeekViewClass))

typedef struct _EaWeekView                   EaWeekView;
typedef struct _EaWeekViewClass              EaWeekViewClass;

struct _EaWeekView
{
	EaCalView parent;
};

GType ea_week_view_get_type (void);

struct _EaWeekViewClass
{
	EaCalViewClass parent_class;
};

AtkObject *     ea_week_view_new         (GtkWidget       *widget);

G_END_DECLS

#endif /* __EA_WEEK_VIEW_H__ */
