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

#include <config.h>
#include <gal/util/e-util.h>
#include "e-cal-view.h"

struct _ECalViewPrivate {
};

static void e_cal_view_class_init (ECalViewClass *klass);
static void e_cal_view_init (ECalView *cal_view, ECalViewClass *klass);
static void e_cal_view_destroy (GtkObject *object);

static GObjectClass *parent_class = NULL;

static void
e_cal_view_class_init (ECalViewClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->destroy = e_cal_view_destroy;
}

static void
e_cal_view_init (ECalView *cal_view, ECalViewClass *klass)
{
	cal_view->priv = g_new0 (ECalViewPrivate, 1);
}

static void
e_cal_view_destroy (GtkObject *object)
{
	ECalView *cal_view = (ECalView *) object;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (cal_view->priv) {
		g_free (cal_view->priv);
		cal_view->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

E_MAKE_TYPE (e_cal_view, "ECalView", ECalView, e_cal_view_class_init,
	     e_cal_view_init, GTK_TYPE_TABLE);
