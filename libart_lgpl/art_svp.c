/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 1998 Raph Levien
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

/* Basic constructors and operations for sorted vector paths */

#include "config.h"
#include "art_svp.h"

#include "art_misc.h"

/**
 * art_svp_free: Free an #ArtSVP structure.
 * @svp: #ArtSVP to free.
 *
 * Frees an #ArtSVP structure and all the segments in it.
 **/
void
art_svp_free (ArtSVP *svp)
{
  gint n_segs = svp->n_segs;
  gint i;

  for (i = 0; i < n_segs; i++)
    art_free (svp->segs[i].points);
  art_free (svp);
}

#ifdef ART_USE_NEW_INTERSECTOR
#define EPSILON 0
#else
#define EPSILON 1e-6
#endif

/**
 * art_svp_seg_compare: Compare two segments of an svp.
 * @seg1: First segment to compare.
 * @seg2: Second segment to compare.
 *
 * Compares two segments of an svp. Return 1 if @seg2 is below or to the
 * right of @seg1, -1 otherwise.
 **/
gint
art_svp_seg_compare (gconstpointer s1, gconstpointer s2)
{
  const ArtSVPSeg *seg1 = s1;
  const ArtSVPSeg *seg2 = s2;

  if (seg1->points[0].y - EPSILON > seg2->points[0].y) return 1;
  else if (seg1->points[0].y + EPSILON < seg2->points[0].y) return -1;
  else if (seg1->points[0].x - EPSILON > seg2->points[0].x) return 1;
  else if (seg1->points[0].x + EPSILON < seg2->points[0].x) return -1;
  else if ((seg1->points[1].x - seg1->points[0].x) *
	   (seg2->points[1].y - seg2->points[0].y) -
	   (seg1->points[1].y - seg1->points[0].y) *
	   (seg2->points[1].x - seg2->points[0].x) > 0) return 1;
  else return -1;
}

