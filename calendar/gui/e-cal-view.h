/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#ifndef _E_CAL_VIEW_H_
#define _E_CAL_VIEW_H_

#include <gtk/gtktable.h>

G_BEGIN_DECLS

/*
 * EView - base widget class for the calendar views.
 */

#define E_CAL_VIEW(obj)          GTK_CHECK_CAST (obj, e_cal_view_get_type (), ECalView)
#define E_CAL_VIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_cal_view_get_type (), ECalViewClass)
#define E_IS_CAL_VIEW(obj)       GTK_CHECK_TYPE (obj, e_cal_view_get_type ())

typedef struct _ECalView        ECalView;
typedef struct _ECalViewClass   ECalViewClass;
typedef struct _ECalViewPrivate ECalViewPrivate;

struct _ECalView {
	GtkTable table;
	ECalViewPrivate *priv;
};

struct _ECalViewClass {
	GtkTableClass parent_class;
};

GType e_cal_view_get_type (void);

G_END_DECLS

#endif
