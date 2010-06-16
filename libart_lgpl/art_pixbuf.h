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

#ifndef __ART_PIXBUF_H__
#define __ART_PIXBUF_H__

/* A generic data structure for holding a buffer of pixels. One way
   to think about this module is as a virtualization over specific
   pixel buffer formats. */

#include <libart_lgpl/art_misc.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*ArtDestroyNotify) (void *func_data, void *data);

typedef struct _ArtPixBuf ArtPixBuf;

typedef enum {
  ART_PIX_RGB
  /* gray, cmyk, lab, ... ? */
} ArtPixFormat;


/* The pixel buffer consists of width * height pixels, each of which
   has n_channels samples. It is stored in simple packed format. */

struct _ArtPixBuf {
  /*< public >*/
  ArtPixFormat format;
  int n_channels;
  int has_alpha;
  int bits_per_sample;

  art_u8 *pixels;
  int width;
  int height;
  int rowstride;
  void *destroy_data;
  ArtDestroyNotify destroy;
};

/* allocate an ArtPixBuf and notify creator upon destruction */
ArtPixBuf *
art_pixbuf_new_rgb_dnotify (art_u8 *pixels, int width, int height, int rowstride,
			    void *dfunc_data, ArtDestroyNotify dfunc);

ArtPixBuf *
art_pixbuf_new_rgba_dnotify (art_u8 *pixels, int width, int height, int rowstride,
			     void *dfunc_data, ArtDestroyNotify dfunc);

#ifdef __cplusplus
}
#endif

#endif
