/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 1998-2000 Raph Levien
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* Basic constructors and operations for vector paths */

#include "config.h"
#include "art_vpath.h"

#include <math.h>
#include <stdlib.h>

#include "art_misc.h"

#include "art_rect.h"

/**
 * art_vpath_add_point: Add point to vpath.
 * @p_vpath: Where the pointer to the #ArtVpath structure is stored.
 * @pn_points: Pointer to the number of points in *@p_vpath.
 * @pn_points_max: Pointer to the number of points allocated.
 * @code: The pathcode for the new point.
 * @x: The X coordinate of the new point.
 * @y: The Y coordinate of the new point.
 *
 * Adds a new point to *@p_vpath, reallocating and updating *@p_vpath
 * and *@pn_points_max as necessary. *@pn_points is incremented.
 *
 * This routine always adds the point after all points already in the
 * vpath. Thus, it should be called in the order the points are
 * desired.
 **/
void
art_vpath_add_point (ArtVpath **p_vpath, gint *pn_points, gint *pn_points_max,
		     ArtPathcode code, gdouble x, gdouble y)
{
  gint i;

  i = (*pn_points)++;
  if (i == *pn_points_max)
    art_expand (*p_vpath, ArtVpath, *pn_points_max);
  (*p_vpath)[i].code = code;
  (*p_vpath)[i].x = x;
  (*p_vpath)[i].y = y;
}

/**
 * art_vpath_bbox_drect: Determine bounding box of vpath.
 * @vec: Source vpath.
 * @drect: Where to store bounding box.
 *
 * Determines bounding box of @vec, and stores it in @drect.
 **/
void
art_vpath_bbox_drect (const ArtVpath *vec, ArtDRect *drect)
{
  gint i;
  gdouble x0, y0, x1, y1;

  if (vec[0].code == ART_END)
    {
      x0 = y0 = x1 = y1 = 0;
    }
  else
    {
      x0 = x1 = vec[0].x;
      y0 = y1 = vec[0].y;
      for (i = 1; vec[i].code != ART_END; i++)
	{
	  if (vec[i].x < x0) x0 = vec[i].x;
	  if (vec[i].x > x1) x1 = vec[i].x;
	  if (vec[i].y < y0) y0 = vec[i].y;
	  if (vec[i].y > y1) y1 = vec[i].y;
	}
    }
  drect->x0 = x0;
  drect->y0 = y0;
  drect->x1 = x1;
  drect->y1 = y1;
}

/**
 * art_vpath_bbox_irect: Determine integer bounding box of vpath.
 * @vec: Source vpath.
 * idrect: Where to store bounding box.
 *
 * Determines integer bounding box of @vec, and stores it in @irect.
 **/
void
art_vpath_bbox_irect (const ArtVpath *vec, ArtIRect *irect)
{
  ArtDRect drect;

  art_vpath_bbox_drect (vec, &drect);
  art_drect_to_irect (irect, &drect);
}

