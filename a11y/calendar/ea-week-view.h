/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-week-view.h
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

#ifndef __EA_WEEK_VIEW_H__
#define __EA_WEEK_VIEW_H__

#include <gtk/gtkaccessible.h>
#include "e-week-view.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
	GtkAccessible parent;
};

GType ea_week_view_get_type (void);

struct _EaWeekViewClass
{
	GtkAccessibleClass parent_class;
};

AtkObject*     ea_week_view_new         (GtkWidget       *widget);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EA_WEEK_VIEW_H__ */
