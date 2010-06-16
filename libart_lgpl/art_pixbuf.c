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

#include "config.h"
#include "art_pixbuf.h"

#include "art_misc.h"
#include <string.h>

/**
 * art_pixbuf_new_rgb_dnotify: Create a new RGB #ArtPixBuf with explicit destroy notification.
 * @pixels: A buffer containing the actual pixel data.
 * @width: The width of the pixbuf.
 * @height: The height of the pixbuf.
 * @rowstride: The rowstride of the pixbuf.
 * @dfunc_data: The private data passed to @dfunc.
 * @dfunc: The destroy notification function.
 *
 * Creates a generic data structure for holding a buffer of RGB
 * pixels.  It is possible to think of an #ArtPixBuf as a
 * virtualization over specific pixel buffer formats.
 *
 * @dfunc is called with @dfunc_data and @pixels as arguments when the
 * #ArtPixBuf is destroyed. Using a destroy notification function
 * allows a wide range of memory management disciplines for the pixel
 * memory. A NULL value for @dfunc is also allowed and means that no
 * special action will be taken on destruction.
 *
 * Return value: The newly created #ArtPixBuf.
 **/
ArtPixBuf *
art_pixbuf_new_rgb_dnotify (art_u8 *pixels, int width, int height, int rowstride,
			    void *dfunc_data, ArtDestroyNotify dfunc)
{
  ArtPixBuf *pixbuf;

  pixbuf = art_new (ArtPixBuf, 1);

  pixbuf->format = ART_PIX_RGB;
  pixbuf->n_channels = 3;
  pixbuf->has_alpha = 0;
  pixbuf->bits_per_sample = 8;

  pixbuf->pixels = (art_u8 *) pixels;
  pixbuf->width = width;
  pixbuf->height = height;
  pixbuf->rowstride = rowstride;
  pixbuf->destroy_data = dfunc_data;
  pixbuf->destroy = dfunc;

  return pixbuf;
}

/**
 * art_pixbuf_new_rgba_dnotify: Create a new RGBA #ArtPixBuf with explicit destroy notification.
 * @pixels: A buffer containing the actual pixel data.
 * @width: The width of the pixbuf.
 * @height: The height of the pixbuf.
 * @rowstride: The rowstride of the pixbuf.
 * @dfunc_data: The private data passed to @dfunc.
 * @dfunc: The destroy notification function.
 *
 * Creates a generic data structure for holding a buffer of RGBA
 * pixels.  It is possible to think of an #ArtPixBuf as a
 * virtualization over specific pixel buffer formats.
 *
 * @dfunc is called with @dfunc_data and @pixels as arguments when the
 * #ArtPixBuf is destroyed. Using a destroy notification function
 * allows a wide range of memory management disciplines for the pixel
 * memory. A NULL value for @dfunc is also allowed and means that no
 * special action will be taken on destruction.
 *
 * Return value: The newly created #ArtPixBuf.
 **/
ArtPixBuf *
art_pixbuf_new_rgba_dnotify (art_u8 *pixels, int width, int height, int rowstride,
			     void *dfunc_data, ArtDestroyNotify dfunc)
{
  ArtPixBuf *pixbuf;

  pixbuf = art_new (ArtPixBuf, 1);

  pixbuf->format = ART_PIX_RGB;
  pixbuf->n_channels = 4;
  pixbuf->has_alpha = 1;
  pixbuf->bits_per_sample = 8;

  pixbuf->pixels = (art_u8 *) pixels;
  pixbuf->width = width;
  pixbuf->height = height;
  pixbuf->rowstride = rowstride;
  pixbuf->destroy_data = dfunc_data;
  pixbuf->destroy = dfunc;

  return pixbuf;
}

