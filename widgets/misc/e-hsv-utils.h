/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-hsv-utils.h - utilites for manipulating colors in HSV space
 * Copyright (C) 1995-2001 Seth Nickell, Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Authors:
 *   Seth Nickell <seth@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _E_HSV_UTILS_H_
#define _E_HSV_UTILS_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

void  e_hsv_tweak   (GdkColor *color,
		     gdouble   delta_h,
		     gdouble   delta_s,
		     gdouble   delta_v);

G_END_DECLS

#endif /* _E_HSV_UTILS_H_ */
