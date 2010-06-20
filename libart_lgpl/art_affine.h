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

#ifndef __ART_AFFINE_H__
#define __ART_AFFINE_H__

#include <libart_lgpl/art_point.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void
art_affine_point (ArtPoint *dst, const ArtPoint *src,
		  const gdouble affine[6]);

void
art_affine_invert (gdouble dst_affine[6], const gdouble src_affine[6]);

void
art_affine_multiply (gdouble dst[6],
		     const gdouble src1[6], const gdouble src2[6]);

/* set up the identity matrix */
void
art_affine_identity (gdouble dst[6]);

/* set up a scaling matrix */
void
art_affine_scale (gdouble dst[6], gdouble sx, gdouble sy);

/* set up a translation matrix */
void
art_affine_translate (gdouble dst[6], gdouble tx, gdouble ty);

/* find the affine's "expansion factor", i.e. the scale amount */
gdouble
art_affine_expansion (const gdouble src[6]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ART_AFFINE_H__ */
