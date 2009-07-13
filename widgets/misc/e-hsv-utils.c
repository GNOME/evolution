/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-hsv-utils.c - utilites for manipulating colours in HSV space
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
   should be the colour you want tweaked. delta_h, delta_s and delta_v specify
   how much you want their respective channels modified (and in what direction).
   if it can't do the specified modification, it does it in the oppositon direction */
void
e_hsv_tweak (GdkColor *colour, gdouble delta_h, gdouble delta_s, gdouble delta_v)
{
	gdouble h, s, v, r, g, b;

	r = colour->red   / 65535.0f;
	g = colour->green / 65535.0f;
	b = colour->blue  / 65535.0f;

	e_rgb_to_hsv (r, g, b, &h, &s, &v);

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

	e_hsv_to_rgb (h, s, v, &r, &g, &b);

	colour->red   = r * 65535.0f;
	colour->green = g * 65535.0f;
	colour->blue  = b * 65535.0f;
}

/* Copy n' Paste code from the GTK+ colour selector (gtkcolorsel.c) */
/* Originally lifted, I suspect, from "Foley, van Dam"              */
void
e_hsv_to_rgb (gdouble  h, gdouble  s, gdouble  v,
	      gdouble *r, gdouble *g, gdouble *b)
{
  gint i;
  gdouble f, w, q, t;

  if (s == 0.0)
    s = 0.000001;

  if (h == -1.0)
    {
      *r = v;
      *g = v;
      *b = v;
    }
  else
    {
      if (h == 360.0)
	h = 0.0;
      h = h / 60.0;
      i = (gint) h;
      f = h - i;
      w = v * (1.0 - s);
      q = v * (1.0 - (s * f));
      t = v * (1.0 - (s * (1.0 - f)));

      switch (i)
	{
	case 0:
	  *r = v;
	  *g = t;
	  *b = w;
	  break;
	case 1:
	  *r = q;
	  *g = v;
	  *b = w;
	  break;
	case 2:
	  *r = w;
	  *g = v;
	  *b = t;
	  break;
	case 3:
	  *r = w;
	  *g = q;
	  *b = v;
	  break;
	case 4:
	  *r = t;
	  *g = w;
	  *b = v;
	  break;
	case 5:
	  *r = v;
	  *g = w;
	  *b = q;
	  break;
	}
    }
}

void
e_rgb_to_hsv (gdouble r, gdouble g, gdouble b,
	      gdouble *h, gdouble *s, gdouble *v)
{
  double max, min, delta;

  max = r;
  if (g > max)
    max = g;
  if (b > max)
    max = b;

  min = r;
  if (g < min)
    min = g;
  if (b < min)
    min = b;

  *v = max;

  if (max != 0.0)
    *s = (max - min) / max;
  else
    *s = 0.0;

  if (*s == 0.0)
    *h = -1.0;
  else
    {
      delta = max - min;

      if (r == max)
	*h = (g - b) / delta;
      else if (g == max)
	*h = 2.0 + (b - r) / delta;
      else if (b == max)
	*h = 4.0 + (r - g) / delta;

      *h = *h * 60.0;

      if (*h < 0.0)
	*h = *h + 360;
    }
}

