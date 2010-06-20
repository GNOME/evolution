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

#ifndef __ART_VPATH_H__
#define __ART_VPATH_H__

#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_pathcode.h>

/* Basic data structures and constructors for simple vector paths */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _ArtVpath ArtVpath;

/* CURVETO is not allowed! */
struct _ArtVpath {
  ArtPathcode code;
  gdouble x;
  gdouble y;
};

/* Some of the functions need to go into their own modules */

void
art_vpath_add_point (ArtVpath **p_vpath, gint *pn_points, gint *pn_points_max,
		     ArtPathcode code, gdouble x, gdouble y);

void
art_vpath_bbox_drect (const ArtVpath *vec, ArtDRect *drect);

void
art_vpath_bbox_irect (const ArtVpath *vec, ArtIRect *irect);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ART_VPATH_H__ */
