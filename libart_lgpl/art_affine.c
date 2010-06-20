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

/* Simple manipulations with affine transformations */

#include "config.h"
#include "art_affine.h"
#include "art_misc.h" /* for M_PI */

#include <math.h>
#include <stdio.h> /* for sprintf */
#include <string.h> /* for strcpy */

/* According to a strict interpretation of the libart structure, this
   routine should go into its own module, art_point_affine.  However,
   it's only two lines of code, and it can be argued that it is one of
   the natural basic functions of an affine transformation.
*/

/**
 * art_affine_point: Do an affine transformation of a point.
 * @dst: Where the result point is stored.
 * @src: The original point.
 @ @affine: The affine transformation.
 **/
void
art_affine_point (ArtPoint *dst, const ArtPoint *src,
		  const gdouble affine[6])
{
  gdouble x, y;

  x = src->x;
  y = src->y;
  dst->x = x * affine[0] + y * affine[2] + affine[4];
  dst->y = x * affine[1] + y * affine[3] + affine[5];
}

/**
 * art_affine_invert: Find the inverse of an affine transformation.
 * @dst: Where the resulting affine is stored.
 * @src: The original affine transformation.
 *
 * All non-degenerate affine transforms are invertible. If the original
 * affine is degenerate or nearly so, expect numerical instability and
 * very likely core dumps on Alpha and other fp-picky architectures.
 * Otherwise, @dst multiplied with @src, or @src multiplied with @dst
 * will be (to within roundoff error) the identity affine.
 **/
void
art_affine_invert (gdouble dst[6], const gdouble src[6])
{
  gdouble r_det;

  r_det = 1.0 / (src[0] * src[3] - src[1] * src[2]);
  dst[0] = src[3] * r_det;
  dst[1] = -src[1] * r_det;
  dst[2] = -src[2] * r_det;
  dst[3] = src[0] * r_det;
  dst[4] = -src[4] * dst[0] - src[5] * dst[2];
  dst[5] = -src[4] * dst[1] - src[5] * dst[3];
}

/**
 * art_affine_multiply: Multiply two affine transformation matrices.
 * @dst: Where to store the result.
 * @src1: The first affine transform to multiply.
 * @src2: The second affine transform to multiply.
 *
 * Multiplies two affine transforms together, i.e. the resulting @dst
 * is equivalent to doing first @src1 then @src2. Note that the
 * PostScript concat operator multiplies on the left, i.e.  "M concat"
 * is equivalent to "CTM = multiply (M, CTM)";
 *
 * It is safe to call this function with @dst equal to @src1 or @src2.
 **/
void
art_affine_multiply (gdouble dst[6], const gdouble src1[6], const gdouble src2[6])
{
  gdouble d0, d1, d2, d3, d4, d5;

  d0 = src1[0] * src2[0] + src1[1] * src2[2];
  d1 = src1[0] * src2[1] + src1[1] * src2[3];
  d2 = src1[2] * src2[0] + src1[3] * src2[2];
  d3 = src1[2] * src2[1] + src1[3] * src2[3];
  d4 = src1[4] * src2[0] + src1[5] * src2[2] + src2[4];
  d5 = src1[4] * src2[1] + src1[5] * src2[3] + src2[5];
  dst[0] = d0;
  dst[1] = d1;
  dst[2] = d2;
  dst[3] = d3;
  dst[4] = d4;
  dst[5] = d5;
}

/**
 * art_affine_identity: Set up the identity matrix.
 * @dst: Where to store the resulting affine transform.
 *
 * Sets up an identity matrix.
 **/
void
art_affine_identity (gdouble dst[6])
{
  dst[0] = 1;
  dst[1] = 0;
  dst[2] = 0;
  dst[3] = 1;
  dst[4] = 0;
  dst[5] = 0;
}

/**
 * art_affine_scale: Set up a scaling matrix.
 * @dst: Where to store the resulting affine transform.
 * @sx: X scale factor.
 * @sy: Y scale factor.
 *
 * Sets up a scaling matrix.
 **/
void
art_affine_scale (gdouble dst[6], gdouble sx, gdouble sy)
{
  dst[0] = sx;
  dst[1] = 0;
  dst[2] = 0;
  dst[3] = sy;
  dst[4] = 0;
  dst[5] = 0;
}

/**
 * art_affine_translate: Set up a translation matrix.
 * @dst: Where to store the resulting affine transform.
 * @tx: X translation amount.
 * @tx: Y translation amount.
 *
 * Sets up a translation matrix.
 **/
void
art_affine_translate (gdouble dst[6], gdouble tx, gdouble ty)
{
  dst[0] = 1;
  dst[1] = 0;
  dst[2] = 0;
  dst[3] = 1;
  dst[4] = tx;
  dst[5] = ty;
}

/**
 * art_affine_expansion: Find the affine's expansion factor.
 * @src: The affine transformation.
 *
 * Finds the expansion factor, i.e. the square root of the factor
 * by which the affine transform affects area. In an affine transform
 * composed of scaling, rotation, shearing, and translation, returns
 * the amount of scaling.
 *
 * Return value: the expansion factor.
 **/
gdouble
art_affine_expansion (const gdouble src[6])
{
  return sqrt (fabs (src[0] * src[3] - src[1] * src[2]));
}

