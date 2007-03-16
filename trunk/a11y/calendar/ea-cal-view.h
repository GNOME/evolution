/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-cal-view.h
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

#ifndef __EA_CAL_VIEW_H__
#define __EA_CAL_VIEW_H__

#include <gtk/gtkaccessible.h>
#include "e-calendar-view.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EA_TYPE_CAL_VIEW                     (ea_cal_view_get_type ())
#define EA_CAL_VIEW(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_CAL_VIEW, EaCalView))
#define EA_CAL_VIEW_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_CAL_VIEW, EaCalViewClass))
#define EA_IS_CAL_VIEW(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_CAL_VIEW))
#define EA_IS_CAL_VIEW_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_CAL_VIEW))
#define EA_CAL_VIEW_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_CAL_VIEW, EaCalViewClass))

typedef struct _EaCalView                   EaCalView;
typedef struct _EaCalViewClass              EaCalViewClass;

struct _EaCalView
{
	GtkAccessible parent;
};

GType ea_cal_view_get_type (void);

struct _EaCalViewClass
{
	GtkAccessibleClass parent_class;
};

AtkObject* ea_cal_view_new (GtkWidget *widget);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EA_CAL_VIEW_H__ */
