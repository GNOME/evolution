/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-hsv-utils.c - utilites for manipulating colors in HSV space
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

#include <config.h>

#include "e-hsv-utils.h"

/* tweak_hsv is a really tweaky function. it modifies its first argument, which
   should be the color you want tweaked. delta_h, delta_s and delta_v specify
   how much you want their respective channels modified (and in what direction).
   if it can't do the specified modification, it does it in the oppositon direction */
void
e_hsv_tweak (GdkColor *color,
             gdouble delta_h,
             gdouble delta_s,
             gdouble delta_v)
{
	gdouble h, s, v, r, g, b;

	r = color->red   / 65535.0f;
	g = color->green / 65535.0f;
	b = color->blue  / 65535.0f;

	gtk_rgb_to_hsv (r, g, b, &h, &s, &v);

	if (h + delta_h < 0) {
		h -= delta_h;
	} else {
		h += delta_h;
	}

	if (s + delta_s < 0) {
		s -= delta_s;
	} else {
		s += delta_s;
	}

	if (v + delta_v < 0) {
		v -= delta_v;
	} else {
		v += delta_v;
	}

	gtk_hsv_to_rgb (h, s, v, &r, &g, &b);

	color->red   = r * 65535.0f;
	color->green = g * 65535.0f;
	color->blue  = b * 65535.0f;
}

