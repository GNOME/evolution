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

#include "config.h"
#include "art_svp_vpath_stroke.h"

#include <stdlib.h>
#include <math.h>

#include "art_misc.h"

#include "art_vpath.h"
#include "art_svp.h"
#ifdef ART_USE_NEW_INTERSECTOR
#include "art_svp_intersect.h"
#else
#include "art_svp_wind.h"
#endif
#include "art_svp_vpath.h"

#define EPSILON 1e-6
#define EPSILON_2 1e-12

#define yes_OPTIMIZE_INNER

/* Render an arc segment starting at (xc + x0, yc + y0) to (xc + x1,
   yc + y1), centered at (xc, yc), and with given radius. Both x0^2 +
   y0^2 and x1^2 + y1^2 should be equal to radius^2.

   A positive value of radius means curve to the left, negative means
   curve to the right.
*/
static void
art_svp_vpath_stroke_arc (ArtVpath **p_vpath, gint *pn, gint *pn_max,
			  gdouble xc, gdouble yc,
			  gdouble x0, gdouble y0,
			  gdouble x1, gdouble y1,
			  gdouble radius,
			  gdouble flatness)
{
  gdouble theta;
  gdouble th_0, th_1;
  gint n_pts;
  gint i;
  gdouble aradius;

  aradius = fabs (radius);
  theta = 2 * M_SQRT2 * sqrt (flatness / aradius);
  th_0 = atan2 (y0, x0);
  th_1 = atan2 (y1, x1);
  if (radius > 0)
    {
      /* curve to the left */
      if (th_0 < th_1) th_0 += M_PI * 2;
      n_pts = ceil ((th_0 - th_1) / theta);
    }
  else
    {
      /* curve to the right */
      if (th_1 < th_0) th_1 += M_PI * 2;
      n_pts = ceil ((th_1 - th_0) / theta);
    }
  art_vpath_add_point (p_vpath, pn, pn_max,
		       ART_LINETO, xc + x0, yc + y0);
  for (i = 1; i < n_pts; i++)
    {
      theta = th_0 + (th_1 - th_0) * i / n_pts;
      art_vpath_add_point (p_vpath, pn, pn_max,
			   ART_LINETO, xc + cos (theta) * aradius,
			   yc + sin (theta) * aradius);
    }
  art_vpath_add_point (p_vpath, pn, pn_max,
		       ART_LINETO, xc + x1, yc + y1);
}

/* Assume that forw and rev are at point i0. Bring them to i1,
   joining with the vector i1 - i2.

   This used to be true, but isn't now that the stroke_raw code is
   filtering out (near)zero length vectors: {It so happens that all
   invocations of this function maintain the precondition i1 = i0 + 1,
   so we could decrease the number of arguments by one. We haven't
   done that here, though.}

   forw is to the line's right and rev is to its left.

   Precondition: no zero-length vectors, otherwise a divide by
   zero will happen.  */
static void
render_seg (ArtVpath **p_forw, gint *pn_forw, gint *pn_forw_max,
	    ArtVpath **p_rev, gint *pn_rev, gint *pn_rev_max,
	    ArtVpath *vpath, gint i0, gint i1, gint i2,
	    ArtPathStrokeJoinType join,
	    gdouble line_width, gdouble miter_limit, gdouble flatness)
{
  gdouble dx0, dy0;
  gdouble dx1, dy1;
  gdouble dlx0, dly0;
  gdouble dlx1, dly1;
  gdouble dmx, dmy;
  gdouble dmr2;
  gdouble scale;
  gdouble cross;

  /* The vectors of the lines from i0 to i1 and i1 to i2. */
  dx0 = vpath[i1].x - vpath[i0].x;
  dy0 = vpath[i1].y - vpath[i0].y;

  dx1 = vpath[i2].x - vpath[i1].x;
  dy1 = vpath[i2].y - vpath[i1].y;

  /* Set dl[xy]0 to the vector from i0 to i1, rotated counterclockwise
     90 degrees, and scaled to the length of line_width. */
  scale = line_width / sqrt (dx0 * dx0 + dy0 * dy0);
  dlx0 = dy0 * scale;
  dly0 = -dx0 * scale;

  /* Set dl[xy]1 to the vector from i1 to i2, rotated counterclockwise
     90 degrees, and scaled to the length of line_width. */
  scale = line_width / sqrt (dx1 * dx1 + dy1 * dy1);
  dlx1 = dy1 * scale;
  dly1 = -dx1 * scale;

  /* now, forw's last point is expected to be colinear along d[xy]0
     to point i0 - dl[xy]0, and rev with i0 + dl[xy]0. */

  /* positive for positive area (i.e. left turn) */
  cross = dx1 * dy0 - dx0 * dy1;

  dmx = (dlx0 + dlx1) * 0.5;
  dmy = (dly0 + dly1) * 0.5;
  dmr2 = dmx * dmx + dmy * dmy;

  if (join == ART_PATH_STROKE_JOIN_MITER &&
      dmr2 * miter_limit * miter_limit < line_width * line_width)
    join = ART_PATH_STROKE_JOIN_BEVEL;

  /* the case when dmr2 is zero or very small bothers me
     (i.e. near a 180 degree angle)
     ALEX: So, we avoid the optimization when dmr2 is very small. This should
     be safe since dmx/y is only used in optimization and in MITER case, and MITER
     should be converted to BEVEL when dmr2 is very small. */
  if (dmr2 > EPSILON_2)
    {
      scale = line_width * line_width / dmr2;
      dmx *= scale;
      dmy *= scale;
    }

  if (cross * cross < EPSILON_2 && dx0 * dx1 + dy0 * dy1 >= 0)
    {
      /* going straight */
      art_vpath_add_point (p_forw, pn_forw, pn_forw_max,
		       ART_LINETO, vpath[i1].x - dlx0, vpath[i1].y - dly0);
      art_vpath_add_point (p_rev, pn_rev, pn_rev_max,
		       ART_LINETO, vpath[i1].x + dlx0, vpath[i1].y + dly0);
    }
  else if (cross > 0)
    {
      /* left turn, forw is outside and rev is inside */

      if (
#ifdef NO_OPTIMIZE_INNER
	  0 &&
#endif
	  (dmr2 > EPSILON_2) &&
	  /* check that i1 + dm[xy] is inside i0-i1 rectangle */
	  (dx0 + dmx) * dx0 + (dy0 + dmy) * dy0 > 0 &&
	  /* and that i1 + dm[xy] is inside i1-i2 rectangle */
	  ((dx1 - dmx) * dx1 + (dy1 - dmy) * dy1 > 0)
#ifdef PEDANTIC_INNER
	  &&
	  /* check that i1 + dl[xy]1 is inside i0-i1 rectangle */
	  (dx0 + dlx1) * dx0 + (dy0 + dly1) * dy0 > 0 &&
	  /* and that i1 + dl[xy]0 is inside i1-i2 rectangle */
	  ((dx1 - dlx0) * dx1 + (dy1 - dly0) * dy1 > 0)
#endif
	  )
	{
	  /* can safely add single intersection point */
	  art_vpath_add_point (p_rev, pn_rev, pn_rev_max,
			   ART_LINETO, vpath[i1].x + dmx, vpath[i1].y + dmy);
	}
      else
	{
	  /* need to loop-de-loop the inside */
	  art_vpath_add_point (p_rev, pn_rev, pn_rev_max,
			   ART_LINETO, vpath[i1].x + dlx0, vpath[i1].y + dly0);
	  art_vpath_add_point (p_rev, pn_rev, pn_rev_max,
			   ART_LINETO, vpath[i1].x, vpath[i1].y);
	  art_vpath_add_point (p_rev, pn_rev, pn_rev_max,
			   ART_LINETO, vpath[i1].x + dlx1, vpath[i1].y + dly1);
	}

      if (join == ART_PATH_STROKE_JOIN_BEVEL)
	{
	  /* bevel */
	  art_vpath_add_point (p_forw, pn_forw, pn_forw_max,
			   ART_LINETO, vpath[i1].x - dlx0, vpath[i1].y - dly0);
	  art_vpath_add_point (p_forw, pn_forw, pn_forw_max,
			   ART_LINETO, vpath[i1].x - dlx1, vpath[i1].y - dly1);
	}
      else if (join == ART_PATH_STROKE_JOIN_MITER)
	{
	  art_vpath_add_point (p_forw, pn_forw, pn_forw_max,
			   ART_LINETO, vpath[i1].x - dmx, vpath[i1].y - dmy);
	}
      else if (join == ART_PATH_STROKE_JOIN_ROUND)
	art_svp_vpath_stroke_arc (p_forw, pn_forw, pn_forw_max,
				  vpath[i1].x, vpath[i1].y,
				  -dlx0, -dly0,
				  -dlx1, -dly1,
				  line_width,
				  flatness);
    }
  else
    {
      /* right turn, rev is outside and forw is inside */

      if (
#ifdef NO_OPTIMIZE_INNER
	  0 &&
#endif
	  (dmr2 > EPSILON_2) &&
	  /* check that i1 - dm[xy] is inside i0-i1 rectangle */
	  (dx0 - dmx) * dx0 + (dy0 - dmy) * dy0 > 0 &&
	  /* and that i1 - dm[xy] is inside i1-i2 rectangle */
	  ((dx1 + dmx) * dx1 + (dy1 + dmy) * dy1 > 0)
#ifdef PEDANTIC_INNER
	  &&
	  /* check that i1 - dl[xy]1 is inside i0-i1 rectangle */
	  (dx0 - dlx1) * dx0 + (dy0 - dly1) * dy0 > 0 &&
	  /* and that i1 - dl[xy]0 is inside i1-i2 rectangle */
	  ((dx1 + dlx0) * dx1 + (dy1 + dly0) * dy1 > 0)
#endif
	  )
	{
	  /* can safely add single intersection point */
	  art_vpath_add_point (p_forw, pn_forw, pn_forw_max,
			   ART_LINETO, vpath[i1].x - dmx, vpath[i1].y - dmy);
	}
      else
	{
	  /* need to loop-de-loop the inside */
	  art_vpath_add_point (p_forw, pn_forw, pn_forw_max,
			   ART_LINETO, vpath[i1].x - dlx0, vpath[i1].y - dly0);
	  art_vpath_add_point (p_forw, pn_forw, pn_forw_max,
			   ART_LINETO, vpath[i1].x, vpath[i1].y);
	  art_vpath_add_point (p_forw, pn_forw, pn_forw_max,
			   ART_LINETO, vpath[i1].x - dlx1, vpath[i1].y - dly1);
	}

      if (join == ART_PATH_STROKE_JOIN_BEVEL)
	{
	  /* bevel */
	  art_vpath_add_point (p_rev, pn_rev, pn_rev_max,
			   ART_LINETO, vpath[i1].x + dlx0, vpath[i1].y + dly0);
	  art_vpath_add_point (p_rev, pn_rev, pn_rev_max,
			   ART_LINETO, vpath[i1].x + dlx1, vpath[i1].y + dly1);
	}
      else if (join == ART_PATH_STROKE_JOIN_MITER)
	{
	  art_vpath_add_point (p_rev, pn_rev, pn_rev_max,
			   ART_LINETO, vpath[i1].x + dmx, vpath[i1].y + dmy);
	}
      else if (join == ART_PATH_STROKE_JOIN_ROUND)
	art_svp_vpath_stroke_arc (p_rev, pn_rev, pn_rev_max,
				  vpath[i1].x, vpath[i1].y,
				  dlx0, dly0,
				  dlx1, dly1,
				  -line_width,
				  flatness);

    }
}

/* caps i1, under the assumption of a vector from i0 */
static void
render_cap (ArtVpath **p_result, gint *pn_result, gint *pn_result_max,
	    ArtVpath *vpath, gint i0, gint i1,
	    ArtPathStrokeCapType cap, gdouble line_width, gdouble flatness)
{
  gdouble dx0, dy0;
  gdouble dlx0, dly0;
  gdouble scale;
  gint n_pts;
  gint i;

  dx0 = vpath[i1].x - vpath[i0].x;
  dy0 = vpath[i1].y - vpath[i0].y;

  /* Set dl[xy]0 to the vector from i0 to i1, rotated counterclockwise
     90 degrees, and scaled to the length of line_width. */
  scale = line_width / sqrt (dx0 * dx0 + dy0 * dy0);
  dlx0 = dy0 * scale;
  dly0 = -dx0 * scale;

  switch (cap)
    {
    case ART_PATH_STROKE_CAP_BUTT:
      art_vpath_add_point (p_result, pn_result, pn_result_max,
			   ART_LINETO, vpath[i1].x - dlx0, vpath[i1].y - dly0);
      art_vpath_add_point (p_result, pn_result, pn_result_max,
			   ART_LINETO, vpath[i1].x + dlx0, vpath[i1].y + dly0);
      break;
    case ART_PATH_STROKE_CAP_ROUND:
      n_pts = ceil (M_PI / (2.0 * M_SQRT2 * sqrt (flatness / line_width)));
      art_vpath_add_point (p_result, pn_result, pn_result_max,
			   ART_LINETO, vpath[i1].x - dlx0, vpath[i1].y - dly0);
      for (i = 1; i < n_pts; i++)
	{
	  gdouble theta, c_th, s_th;

	  theta = M_PI * i / n_pts;
	  c_th = cos (theta);
	  s_th = sin (theta);
	  art_vpath_add_point (p_result, pn_result, pn_result_max,
			       ART_LINETO,
			       vpath[i1].x - dlx0 * c_th - dly0 * s_th,
			       vpath[i1].y - dly0 * c_th + dlx0 * s_th);
	}
      art_vpath_add_point (p_result, pn_result, pn_result_max,
			   ART_LINETO, vpath[i1].x + dlx0, vpath[i1].y + dly0);
      break;
    case ART_PATH_STROKE_CAP_SQUARE:
      art_vpath_add_point (p_result, pn_result, pn_result_max,
			   ART_LINETO,
			   vpath[i1].x - dlx0 - dly0,
			   vpath[i1].y - dly0 + dlx0);
      art_vpath_add_point (p_result, pn_result, pn_result_max,
			   ART_LINETO,
			   vpath[i1].x + dlx0 - dly0,
			   vpath[i1].y + dly0 + dlx0);
      break;
    }
}

/**
 * art_svp_from_vpath_raw: Stroke a vector path, raw version
 * @vpath: #ArtVPath to stroke.
 * @join: Join style.
 * @cap: Cap style.
 * @line_width: Width of stroke.
 * @miter_limit: Miter limit.
 * @flatness: Flatness.
 *
 * Exactly the same as art_svp_vpath_stroke(), except that the resulting
 * stroke outline may self-intersect and have regions of winding number
 * greater than 1.
 *
 * Return value: Resulting raw stroked outline in svp format.
 **/
ArtVpath *
art_svp_vpath_stroke_raw (ArtVpath *vpath,
			  ArtPathStrokeJoinType join,
			  ArtPathStrokeCapType cap,
			  gdouble line_width,
			  gdouble miter_limit,
			  gdouble flatness)
{
  gint begin_idx, end_idx;
  gint i;
  ArtVpath *forw, *rev;
  gint n_forw, n_rev;
  gint n_forw_max, n_rev_max;
  ArtVpath *result;
  gint n_result, n_result_max;
  gdouble half_lw = 0.5 * line_width;
  gint closed;
  gint last, this, next, second;
  gdouble dx, dy;

  n_forw_max = 16;
  forw = art_new (ArtVpath, n_forw_max);

  n_rev_max = 16;
  rev = art_new (ArtVpath, n_rev_max);

  n_result = 0;
  n_result_max = 16;
  result = art_new (ArtVpath, n_result_max);

  for (begin_idx = 0; vpath[begin_idx].code != ART_END; begin_idx = end_idx)
    {
      n_forw = 0;
      n_rev = 0;

      closed = (vpath[begin_idx].code == ART_MOVETO);

      /* we don't know what the first point joins with until we get to the
	 last point and see if it's closed. So we start with the second
	 line in the path.

	 Note: this is not strictly true (we now know it's closed from
	 the opening pathcode), but why fix code that isn't broken?
      */

      this = begin_idx;
      /* skip over identical points at the beginning of the subpath */
      for (i = this + 1; vpath[i].code == ART_LINETO; i++)
	{
	  dx = vpath[i].x - vpath[this].x;
	  dy = vpath[i].y - vpath[this].y;
	  if (dx * dx + dy * dy > EPSILON_2)
	    break;
	}
      next = i;
      second = next;

      /* invariant: this doesn't coincide with next */
      while (vpath[next].code == ART_LINETO)
	{
	  last = this;
	  this = next;
	  /* skip over identical points after the beginning of the subpath */
	  for (i = this + 1; vpath[i].code == ART_LINETO; i++)
	    {
	      dx = vpath[i].x - vpath[this].x;
	      dy = vpath[i].y - vpath[this].y;
	      if (dx * dx + dy * dy > EPSILON_2)
		break;
	    }
	  next = i;
	  if (vpath[next].code != ART_LINETO)
	    {
	      /* reached end of path */
	      /* make "closed" detection conform to PostScript
		 semantics (i.e. explicit closepath code rather than
		 just the fact that end of the path is the beginning) */
	      if (closed &&
		  vpath[this].x == vpath[begin_idx].x &&
		  vpath[this].y == vpath[begin_idx].y)
		{
		  gint j;

		  /* path is closed, render join to beginning */
		  render_seg (&forw, &n_forw, &n_forw_max,
			      &rev, &n_rev, &n_rev_max,
			      vpath, last, this, second,
			      join, half_lw, miter_limit, flatness);

		  /* do forward path */
		  art_vpath_add_point (&result, &n_result, &n_result_max,
				   ART_MOVETO, forw[n_forw - 1].x,
				   forw[n_forw - 1].y);
		  for (j = 0; j < n_forw; j++)
		    art_vpath_add_point (&result, &n_result, &n_result_max,
				     ART_LINETO, forw[j].x,
				     forw[j].y);

		  /* do reverse path, reversed */
		  art_vpath_add_point (&result, &n_result, &n_result_max,
				   ART_MOVETO, rev[0].x,
				   rev[0].y);
		  for (j = n_rev - 1; j >= 0; j--)
		    art_vpath_add_point (&result, &n_result, &n_result_max,
				     ART_LINETO, rev[j].x,
				     rev[j].y);
		}
	      else
		{
		  /* path is open */
		  gint j;

		  /* add to forw rather than result to ensure that
		     forw has at least one point. */
		  render_cap (&forw, &n_forw, &n_forw_max,
			      vpath, last, this,
			      cap, half_lw, flatness);
		  art_vpath_add_point (&result, &n_result, &n_result_max,
				   ART_MOVETO, forw[0].x,
				   forw[0].y);
		  for (j = 1; j < n_forw; j++)
		    art_vpath_add_point (&result, &n_result, &n_result_max,
				     ART_LINETO, forw[j].x,
				     forw[j].y);
		  for (j = n_rev - 1; j >= 0; j--)
		    art_vpath_add_point (&result, &n_result, &n_result_max,
				     ART_LINETO, rev[j].x,
				     rev[j].y);
		  render_cap (&result, &n_result, &n_result_max,
			      vpath, second, begin_idx,
			      cap, half_lw, flatness);
		  art_vpath_add_point (&result, &n_result, &n_result_max,
				   ART_LINETO, forw[0].x,
				   forw[0].y);
		}
	    }
	  else
	    render_seg (&forw, &n_forw, &n_forw_max,
			&rev, &n_rev, &n_rev_max,
			vpath, last, this, next,
			join, half_lw, miter_limit, flatness);
	}
      end_idx = next;
    }

  art_free (forw);
  art_free (rev);
  art_vpath_add_point (&result, &n_result, &n_result_max, ART_END, 0, 0);
  return result;
}

/* Render a vector path into a stroked outline.

   Status of this routine:

   Basic correctness: Only miter and bevel line joins are implemented,
   and only butt line caps. Otherwise, seems to be fine.

   Numerical stability: We cheat (adding random perturbation). Thus,
   it seems very likely that no numerical stability problems will be
   seen in practice.

   Speed: Should be pretty good.

   Precision: The perturbation fuzzes the coordinates slightly,
   but not enough to be visible.  */
/**
 * art_svp_vpath_stroke: Stroke a vector path.
 * @vpath: #ArtVPath to stroke.
 * @join: Join style.
 * @cap: Cap style.
 * @line_width: Width of stroke.
 * @miter_limit: Miter limit.
 * @flatness: Flatness.
 *
 * Computes an svp representing the stroked outline of @vpath. The
 * width of the stroked line is @line_width.
 *
 * Lines are joined according to the @join rule. Possible values are
 * ART_PATH_STROKE_JOIN_MITER (for mitered joins),
 * ART_PATH_STROKE_JOIN_ROUND (for round joins), and
 * ART_PATH_STROKE_JOIN_BEVEL (for bevelled joins). The mitered join
 * is converted to a bevelled join if the miter would extend to a
 * distance of more than @miter_limit * @line_width from the actual
 * join point.
 *
 * If there are open subpaths, the ends of these subpaths are capped
 * according to the @cap rule. Possible values are
 * ART_PATH_STROKE_CAP_BUTT (squared cap, extends exactly to end
 * point), ART_PATH_STROKE_CAP_ROUND (rounded half-circle centered at
 * the end point), and ART_PATH_STROKE_CAP_SQUARE (squared cap,
 * extending half @line_width past the end point).
 *
 * The @flatness parameter controls the accuracy of the rendering. It
 * is most important for determining the number of points to use to
 * approximate circular arcs for round lines and joins. In general, the
 * resulting vector path will be within @flatness pixels of the "ideal"
 * path containing actual circular arcs. I reserve the right to use
 * the @flatness parameter to convert bevelled joins to miters for very
 * small turn angles, as this would reduce the number of points in the
 * resulting outline path.
 *
 * The resulting path is "clean" with respect to self-intersections, i.e.
 * the winding number is 0 or 1 at each point.
 *
 * Return value: Resulting stroked outline in svp format.
 **/
ArtSVP *
art_svp_vpath_stroke (ArtVpath *vpath,
		      ArtPathStrokeJoinType join,
		      ArtPathStrokeCapType cap,
		      gdouble line_width,
		      gdouble miter_limit,
		      gdouble flatness)
{
#ifdef ART_USE_NEW_INTERSECTOR
  ArtVpath *vpath_stroke;
  ArtSVP *svp, *svp2;
  ArtSvpWriter *swr;

  vpath_stroke = art_svp_vpath_stroke_raw (vpath, join, cap,
					   line_width, miter_limit, flatness);
  svp = art_svp_from_vpath (vpath_stroke);
  art_free (vpath_stroke);

  swr = art_svp_writer_rewind_new (ART_WIND_RULE_NONZERO);
  art_svp_intersector (svp, swr);

  svp2 = art_svp_writer_rewind_reap (swr);
  art_svp_free (svp);
  return svp2;
#else
  ArtVpath *vpath_stroke, *vpath2;
  ArtSVP *svp, *svp2, *svp3;

  vpath_stroke = art_svp_vpath_stroke_raw (vpath, join, cap,
					   line_width, miter_limit, flatness);
  vpath2 = art_vpath_perturb (vpath_stroke);
  art_free (vpath_stroke);
  svp = art_svp_from_vpath (vpath2);
  art_free (vpath2);
  svp2 = art_svp_uncross (svp);
  art_svp_free (svp);
  svp3 = art_svp_rewind_uncrossed (svp2, ART_WIND_RULE_NONZERO);
  art_svp_free (svp2);

  return svp3;
#endif
}
