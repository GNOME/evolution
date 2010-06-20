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

#ifndef __ART_BPATH_H__
#define __ART_BPATH_H__

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_point.h>
#include <libart_lgpl/art_pathcode.h>

/* Basic data structures and constructors for bezier paths */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _ArtBpath ArtBpath;

struct _ArtBpath {
  /*< public >*/
  ArtPathcode code;
  gdouble x1;
  gdouble y1;
  gdouble x2;
  gdouble y2;
  gdouble x3;
  gdouble y3;
};

ArtBpath *
art_bpath_affine_transform (const ArtBpath *src, const gdouble matrix[6]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ART_BPATH_H__ */
