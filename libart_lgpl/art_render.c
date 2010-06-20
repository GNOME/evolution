/*
 * art_render.c: Modular rendering architecture.
 *
 * Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 2000 Raph Levien
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
#include "art_render.h"

#include "art_rgb.h"

typedef struct _ArtRenderPriv ArtRenderPriv;

struct _ArtRenderPriv {
  ArtRender super;

  ArtImageSource *image_source;

  gint n_mask_source;
  ArtMaskSource **mask_source;

  gint n_callbacks;
  ArtRenderCallback **callbacks;
};

static void
art_render_nop_done (ArtRenderCallback *self, ArtRender *render)
{
}

static void
art_render_clear_render_rgb8 (ArtRenderCallback *self, ArtRender *render,
			      art_u8 *dest, gint y)
{
  gint width = render->x1 - render->x0;
  art_u8 r, g, b;
  ArtPixMaxDepth color_max;

  color_max = render->clear_color[0];
  r = ART_PIX_8_FROM_MAX (color_max);
  color_max = render->clear_color[1];
  g = ART_PIX_8_FROM_MAX (color_max);
  color_max = render->clear_color[2];
  b = ART_PIX_8_FROM_MAX (color_max);

  art_rgb_fill_run (dest, r, g, b, width);
}

static void
art_render_clear_render_8 (ArtRenderCallback *self, ArtRender *render,
			   art_u8 *dest, gint y)
{
  gint width = render->x1 - render->x0;
  gint i, j;
  gint n_ch = render->n_chan + (render->alpha_type != ART_ALPHA_NONE);
  gint ix;
  art_u8 color[ART_MAX_CHAN + 1];

  for (j = 0; j < n_ch; j++)
    {
      ArtPixMaxDepth color_max = render->clear_color[j];
      color[j] = ART_PIX_8_FROM_MAX (color_max);
    }

  ix = 0;
  for (i = 0; i < width; i++)
    for (j = 0; j < n_ch; j++)
      dest[ix++] = color[j];
}

const ArtRenderCallback art_render_clear_rgb8_obj =
{
  art_render_clear_render_rgb8,
  art_render_nop_done
};

const ArtRenderCallback art_render_clear_8_obj =
{
  art_render_clear_render_8,
  art_render_nop_done
};

#if ART_MAX_DEPTH >= 16

static void
art_render_clear_render_16 (ArtRenderCallback *self, ArtRender *render,
			    art_u8 *dest, gint y)
{
  gint width = render->x1 - render->x0;
  gint i, j;
  gint n_ch = render->n_chan + (render->alpha_type != ART_ALPHA_NONE);
  gint ix;
  art_u16 *dest_16 = (art_u16 *)dest;
  art_u8 color[ART_MAX_CHAN + 1];

  for (j = 0; j < n_ch; j++)
    {
      gint color_16 = render->clear_color[j];
      color[j] = color_16;
    }

  ix = 0;
  for (i = 0; i < width; i++)
    for (j = 0; j < n_ch; j++)
      dest_16[ix++] = color[j];
}

const ArtRenderCallback art_render_clear_16_obj =
{
  art_render_clear_render_16,
  art_render_nop_done
};

#endif /* ART_MAX_DEPTH >= 16 */

/* This is the most general form of the function. It is slow but
   (hopefully) correct. Actually, I'm still worried about roundoff
   errors in the premul case - it seems to me that an off-by-one could
   lead to overflow. */
static void
art_render_composite (ArtRenderCallback *self, ArtRender *render,
					art_u8 *dest, gint y)
{
  ArtRenderMaskRun *run = render->run;
  art_u32 depth = render->depth;
  gint n_run = render->n_run;
  gint x0 = render->x0;
  gint x;
  gint run_x0, run_x1;
  art_u8 *alpha_buf = render->alpha_buf;
  art_u8 *image_buf = render->image_buf;
  gint i, j;
  art_u32 tmp;
  art_u32 run_alpha;
  art_u32 alpha;
  gint image_ix;
  art_u16 src[ART_MAX_CHAN + 1];
  art_u16 dst[ART_MAX_CHAN + 1];
  gint n_chan = render->n_chan;
  ArtAlphaType alpha_type = render->alpha_type;
  gint n_ch = n_chan + (alpha_type != ART_ALPHA_NONE);
  gint dst_pixstride = n_ch * (depth >> 3);
  gint buf_depth = render->buf_depth;
  ArtAlphaType buf_alpha = render->buf_alpha;
  gint buf_n_ch = n_chan + (buf_alpha != ART_ALPHA_NONE);
  gint buf_pixstride = buf_n_ch * (buf_depth >> 3);
  art_u8 *bufptr;
  art_u32 src_alpha;
  art_u32 src_mul;
  art_u8 *dstptr;
  art_u32 dst_alpha;
  art_u32 dst_mul;

  image_ix = 0;
  for (i = 0; i < n_run - 1; i++)
    {
      run_x0 = run[i].x;
      run_x1 = run[i + 1].x;
      tmp = run[i].alpha;
      if (tmp < 0x8100)
	continue;

      run_alpha = (tmp + (tmp >> 8) + (tmp >> 16) - 0x8000) >> 8;
      bufptr = image_buf + (run_x0 - x0) * buf_pixstride;
      dstptr = dest + (run_x0 - x0) * dst_pixstride;
      for (x = run_x0; x < run_x1; x++)
	{
	  if (alpha_buf)
	    {
	      if (depth == 8)
		{
		  tmp = run_alpha * alpha_buf[x - x0] + 0x80;
		  /* range 0x80 .. 0xff0080 */
		  alpha = (tmp + (tmp >> 8) + (tmp >> 16)) >> 8;
		}
	      else /* (depth == 16) */
		{
		  tmp = ((art_u16 *)alpha_buf)[x - x0];
		  tmp = (run_alpha * tmp + 0x8000) >> 8;
		  /* range 0x80 .. 0xffff80 */
		  alpha = (tmp + (tmp >> 16)) >> 8;
		}
	    }
	  else
	    alpha = run_alpha;
	  /* alpha is run_alpha * alpha_buf[x], range 0 .. 0x10000 */

	  /* convert (src pixel * alpha) to premul alpha form,
	     store in src as 0..0xffff range */
	  if (buf_alpha == ART_ALPHA_NONE)
	    {
	      src_alpha = alpha;
	      src_mul = src_alpha;
	    }
	  else
	    {
	      if (buf_depth == 8)
		{
		  tmp = alpha * bufptr[n_chan] + 0x80;
		  /* range 0x80 .. 0xff0080 */
		  src_alpha = (tmp + (tmp >> 8) + (tmp >> 16)) >> 8;
		}
	      else /* (depth == 16) */
		{
		  tmp = ((art_u16 *)bufptr)[n_chan];
		  tmp = (alpha * tmp + 0x8000) >> 8;
		  /* range 0x80 .. 0xffff80 */
		  src_alpha = (tmp + (tmp >> 16)) >> 8;
		}
	      if (buf_alpha == ART_ALPHA_SEPARATE)
		src_mul = src_alpha;
	      else /* buf_alpha == (ART_ALPHA_PREMUL) */
		src_mul = alpha;
	    }
	  /* src_alpha is the (alpha of the source pixel * alpha),
	     range 0..0x10000 */

	  if (buf_depth == 8)
	    {
	      src_mul *= 0x101;
	      for (j = 0; j < n_chan; j++)
		src[j] = (bufptr[j] * src_mul + 0x8000) >> 16;
	    }
	  else if (buf_depth == 16)
	    {
	      for (j = 0; j < n_chan; j++)
		src[j] = (((art_u16 *)bufptr)[j] * src_mul + 0x8000) >> 16;
	    }
	  bufptr += buf_pixstride;

	  /* src[0..n_chan - 1] (range 0..0xffff) and src_alpha (range
             0..0x10000) now contain the source pixel with
             premultiplied alpha */

	  /* convert dst pixel to premul alpha form,
	     store in dst as 0..0xffff range */
	  if (alpha_type == ART_ALPHA_NONE)
	    {
	      dst_alpha = 0x10000;
	      dst_mul = dst_alpha;
	    }
	  else
	    {
	      if (depth == 8)
		{
		  tmp = dstptr[n_chan];
		  /* range 0..0xff */
		  dst_alpha = (tmp << 8) + tmp + (tmp >> 7);
		}
	      else /* (depth == 16) */
		{
		  tmp = ((art_u16 *)dstptr)[n_chan];
		  dst_alpha = (tmp + (tmp >> 15));
		}
	      if (alpha_type == ART_ALPHA_SEPARATE)
		dst_mul = dst_alpha;
	      else /* (alpha_type == ART_ALPHA_PREMUL) */
		dst_mul = 0x10000;
	    }
	  /* dst_alpha is the alpha of the dest pixel,
	     range 0..0x10000 */

	  if (depth == 8)
	    {
	      dst_mul *= 0x101;
	      for (j = 0; j < n_chan; j++)
		dst[j] = (dstptr[j] * dst_mul + 0x8000) >> 16;
	    }
	  else if (buf_depth == 16)
	    {
	      for (j = 0; j < n_chan; j++)
		dst[j] = (((art_u16 *)dstptr)[j] * dst_mul + 0x8000) >> 16;
	    }

	  /* do the compositing, dst = (src over dst) */
	  for (j = 0; j < n_chan; j++)
	    {
	      art_u32 srcv, dstv;
	      art_u32 tmp;

	      srcv = src[j];
	      dstv = dst[j];
	      tmp = ((dstv * (0x10000 - src_alpha) + 0x8000) >> 16) + srcv;
	      tmp -= tmp >> 16;
	      dst[j] = tmp;
	    }

	  if (alpha_type == ART_ALPHA_NONE)
	    {
	      if (depth == 8)
		dst_mul = 0xff;
	      else /* (depth == 16) */
		dst_mul = 0xffff;
	    }
	  else
	    {
	      if (src_alpha >= 0x10000)
		dst_alpha = 0x10000;
	      else
		dst_alpha += ((((0x10000 - dst_alpha) * src_alpha) >> 8) + 0x80) >> 8;
	      if (alpha_type == ART_ALPHA_PREMUL || dst_alpha == 0)
		{
		  if (depth == 8)
		    dst_mul = 0xff;
		  else /* (depth == 16) */
		    dst_mul = 0xffff;
		}
	      else /* (ALPHA_TYPE == ART_ALPHA_SEPARATE && dst_alpha != 0) */
		{
		  if (depth == 8)
		    dst_mul = 0xff0000 / dst_alpha;
		  else /* (depth == 16) */
		    dst_mul = 0xffff0000 / dst_alpha;
		}
	    }
	  if (depth == 8)
	    {
	      for (j = 0; j < n_chan; j++)
		dstptr[j] = (dst[j] * dst_mul + 0x8000) >> 16;
	      if (alpha_type != ART_ALPHA_NONE)
		dstptr[n_chan] = (dst_alpha * 0xff + 0x8000) >> 16;
	    }
	  else if (depth == 16)
	    {
	      for (j = 0; j < n_chan; j++)
		((art_u16 *)dstptr)[j] = (dst[j] * dst_mul + 0x8000) >> 16;
	      if (alpha_type != ART_ALPHA_NONE)
		((art_u16 *)dstptr)[n_chan] = (dst_alpha * 0xffff + 0x8000) >> 16;
	    }
	  dstptr += dst_pixstride;
	}
    }
}

const ArtRenderCallback art_render_composite_obj =
{
  art_render_composite,
  art_render_nop_done
};

static void
art_render_composite_8 (ArtRenderCallback *self, ArtRender *render,
			art_u8 *dest, gint y)
{
  ArtRenderMaskRun *run = render->run;
  gint n_run = render->n_run;
  gint x0 = render->x0;
  gint x;
  gint run_x0, run_x1;
  art_u8 *alpha_buf = render->alpha_buf;
  art_u8 *image_buf = render->image_buf;
  gint i, j;
  art_u32 tmp;
  art_u32 run_alpha;
  art_u32 alpha;
  gint image_ix;
  gint n_chan = render->n_chan;
  ArtAlphaType alpha_type = render->alpha_type;
  gint n_ch = n_chan + (alpha_type != ART_ALPHA_NONE);
  gint dst_pixstride = n_ch;
  ArtAlphaType buf_alpha = render->buf_alpha;
  gint buf_n_ch = n_chan + (buf_alpha != ART_ALPHA_NONE);
  gint buf_pixstride = buf_n_ch;
  art_u8 *bufptr;
  art_u32 src_alpha;
  art_u32 src_mul;
  art_u8 *dstptr;
  art_u32 dst_alpha;
  art_u32 dst_mul, dst_save_mul;

  image_ix = 0;
  for (i = 0; i < n_run - 1; i++)
    {
      run_x0 = run[i].x;
      run_x1 = run[i + 1].x;
      tmp = run[i].alpha;
      if (tmp < 0x10000)
	continue;

      run_alpha = (tmp + (tmp >> 8) + (tmp >> 16) - 0x8000) >> 8;
      bufptr = image_buf + (run_x0 - x0) * buf_pixstride;
      dstptr = dest + (run_x0 - x0) * dst_pixstride;
      for (x = run_x0; x < run_x1; x++)
	{
	  if (alpha_buf)
	    {
	      tmp = run_alpha * alpha_buf[x - x0] + 0x80;
	      /* range 0x80 .. 0xff0080 */
	      alpha = (tmp + (tmp >> 8) + (tmp >> 16)) >> 8;
	    }
	  else
	    alpha = run_alpha;
	  /* alpha is run_alpha * alpha_buf[x], range 0 .. 0x10000 */

	  /* convert (src pixel * alpha) to premul alpha form,
	     store in src as 0..0xffff range */
	  if (buf_alpha == ART_ALPHA_NONE)
	    {
	      src_alpha = alpha;
	      src_mul = src_alpha;
	    }
	  else
	    {
	      tmp = alpha * bufptr[n_chan] + 0x80;
	      /* range 0x80 .. 0xff0080 */
	      src_alpha = (tmp + (tmp >> 8) + (tmp >> 16)) >> 8;

	      if (buf_alpha == ART_ALPHA_SEPARATE)
		src_mul = src_alpha;
	      else /* buf_alpha == (ART_ALPHA_PREMUL) */
		src_mul = alpha;
	    }
	  /* src_alpha is the (alpha of the source pixel * alpha),
	     range 0..0x10000 */

	  src_mul *= 0x101;

	  if (alpha_type == ART_ALPHA_NONE)
	    {
	      dst_alpha = 0x10000;
	      dst_mul = dst_alpha;
	    }
	  else
	    {
	      tmp = dstptr[n_chan];
	      /* range 0..0xff */
	      dst_alpha = (tmp << 8) + tmp + (tmp >> 7);
	      if (alpha_type == ART_ALPHA_SEPARATE)
		dst_mul = dst_alpha;
	      else /* (alpha_type == ART_ALPHA_PREMUL) */
		dst_mul = 0x10000;
	    }
	  /* dst_alpha is the alpha of the dest pixel,
	     range 0..0x10000 */

	  dst_mul *= 0x101;

	  if (alpha_type == ART_ALPHA_NONE)
	    {
	      dst_save_mul = 0xff;
	    }
	  else
	    {
	      if (src_alpha >= 0x10000)
		dst_alpha = 0x10000;
	      else
		dst_alpha += ((((0x10000 - dst_alpha) * src_alpha) >> 8) + 0x80) >> 8;
	      if (alpha_type == ART_ALPHA_PREMUL || dst_alpha == 0)
		{
		  dst_save_mul = 0xff;
		}
	      else /* (ALPHA_TYPE == ART_ALPHA_SEPARATE && dst_alpha != 0) */
		{
		  dst_save_mul = 0xff0000 / dst_alpha;
		}
	    }

	  for (j = 0; j < n_chan; j++)
	    {
	      art_u32 src, dst;
	      art_u32 tmp;

	      src = (bufptr[j] * src_mul + 0x8000) >> 16;
	      dst = (dstptr[j] * dst_mul + 0x8000) >> 16;
	      tmp = ((dst * (0x10000 - src_alpha) + 0x8000) >> 16) + src;
	      tmp -= tmp >> 16;
	      dstptr[j] = (tmp * dst_save_mul + 0x8000) >> 16;
	    }
	  if (alpha_type != ART_ALPHA_NONE)
	    dstptr[n_chan] = (dst_alpha * 0xff + 0x8000) >> 16;

	  bufptr += buf_pixstride;
	  dstptr += dst_pixstride;
	}
    }
}

const ArtRenderCallback art_render_composite_8_obj =
{
  art_render_composite_8,
  art_render_nop_done
};

/* Assumes:
 * alpha_buf is NULL
 * buf_alpha = ART_ALPHA_NONE  (source)
 * alpha_type = ART_ALPHA_SEPARATE (dest)
 * n_chan = 3;
 */
static void
art_render_composite_8_opt1 (ArtRenderCallback *self, ArtRender *render,
			     art_u8 *dest, gint y)
{
  ArtRenderMaskRun *run = render->run;
  gint n_run = render->n_run;
  gint x0 = render->x0;
  gint x;
  gint run_x0, run_x1;
  art_u8 *image_buf = render->image_buf;
  gint i, j;
  art_u32 tmp;
  art_u32 run_alpha;
  gint image_ix;
  art_u8 *bufptr;
  art_u32 src_mul;
  art_u8 *dstptr;
  art_u32 dst_alpha;
  art_u32 dst_mul, dst_save_mul;

  image_ix = 0;
  for (i = 0; i < n_run - 1; i++)
    {
      run_x0 = run[i].x;
      run_x1 = run[i + 1].x;
      tmp = run[i].alpha;
      if (tmp < 0x10000)
	continue;

      run_alpha = (tmp + (tmp >> 8) + (tmp >> 16) - 0x8000) >> 8;
      bufptr = image_buf + (run_x0 - x0) * 3;
      dstptr = dest + (run_x0 - x0) * 4;
      if (run_alpha == 0x10000)
	{
	  for (x = run_x0; x < run_x1; x++)
	    {
	      *dstptr++ = *bufptr++;
	      *dstptr++ = *bufptr++;
	      *dstptr++ = *bufptr++;
	      *dstptr++ = 0xff;
	    }
	}
      else
	{
	  for (x = run_x0; x < run_x1; x++)
	    {
	      src_mul = run_alpha * 0x101;

	      tmp = dstptr[3];
	      /* range 0..0xff */
	      dst_alpha = (tmp << 8) + tmp + (tmp >> 7);
	      dst_mul = dst_alpha;
	      /* dst_alpha is the alpha of the dest pixel,
		 range 0..0x10000 */

	      dst_mul *= 0x101;

	      dst_alpha += ((((0x10000 - dst_alpha) * run_alpha) >> 8) + 0x80) >> 8;
	      if (dst_alpha == 0)
		  dst_save_mul = 0xff;
	      else /* (dst_alpha != 0) */
		  dst_save_mul = 0xff0000 / dst_alpha;

	      for (j = 0; j < 3; j++)
		{
		  art_u32 src, dst;
		  art_u32 tmp;

		  src = (bufptr[j] * src_mul + 0x8000) >> 16;
		  dst = (dstptr[j] * dst_mul + 0x8000) >> 16;
		  tmp = ((dst * (0x10000 - run_alpha) + 0x8000) >> 16) + src;
		  tmp -= tmp >> 16;
		  dstptr[j] = (tmp * dst_save_mul + 0x8000) >> 16;
		}
	      dstptr[3] = (dst_alpha * 0xff + 0x8000) >> 16;

	      bufptr += 3;
	      dstptr += 4;
	    }
	}
    }
}

const ArtRenderCallback art_render_composite_8_opt1_obj =
{
  art_render_composite_8_opt1,
  art_render_nop_done
};

/* Assumes:
 * alpha_buf is NULL
 * buf_alpha = ART_ALPHA_PREMUL  (source)
 * alpha_type = ART_ALPHA_SEPARATE (dest)
 * n_chan = 3;
 */
static void
art_render_composite_8_opt2 (ArtRenderCallback *self, ArtRender *render,
			     art_u8 *dest, gint y)
{
  ArtRenderMaskRun *run = render->run;
  gint n_run = render->n_run;
  gint x0 = render->x0;
  gint x;
  gint run_x0, run_x1;
  art_u8 *image_buf = render->image_buf;
  gint i, j;
  art_u32 tmp;
  art_u32 run_alpha;
  gint image_ix;
  art_u8 *bufptr;
  art_u32 src_alpha;
  art_u32 src_mul;
  art_u8 *dstptr;
  art_u32 dst_alpha;
  art_u32 dst_mul, dst_save_mul;

  image_ix = 0;
  for (i = 0; i < n_run - 1; i++)
    {
      run_x0 = run[i].x;
      run_x1 = run[i + 1].x;
      tmp = run[i].alpha;
      if (tmp < 0x10000)
	continue;

      run_alpha = (tmp + (tmp >> 8) + (tmp >> 16) - 0x8000) >> 8;
      bufptr = image_buf + (run_x0 - x0) * 4;
      dstptr = dest + (run_x0 - x0) * 4;
      if (run_alpha == 0x10000)
	{
	  for (x = run_x0; x < run_x1; x++)
	    {
	      src_alpha = (bufptr[3] << 8) + bufptr[3] + (bufptr[3] >> 7);
	      /* src_alpha is the (alpha of the source pixel),
		 range 0..0x10000 */

	      dst_alpha = (dstptr[3] << 8) + dstptr[3] + (dstptr[3] >> 7);
	      /* dst_alpha is the alpha of the dest pixel,
		 range 0..0x10000 */

	      dst_mul = dst_alpha*0x101;

	      if (src_alpha >= 0x10000)
		dst_alpha = 0x10000;
	      else
		dst_alpha += ((((0x10000 - dst_alpha) * src_alpha) >> 8) + 0x80) >> 8;

	      if (dst_alpha == 0)
		  dst_save_mul = 0xff;
	      else /* dst_alpha != 0) */
		  dst_save_mul = 0xff0000 / dst_alpha;

	      for (j = 0; j < 3; j++)
		{
		  art_u32 src, dst;
		  art_u32 tmp;

		  src = (bufptr[j] << 8) |  bufptr[j];
		  dst = (dstptr[j] * dst_mul + 0x8000) >> 16;
		  tmp = ((dst * (0x10000 - src_alpha) + 0x8000) >> 16) + src;
		  tmp -= tmp >> 16;
		  dstptr[j] = (tmp * dst_save_mul + 0x8000) >> 16;
		}
	      dstptr[3] = (dst_alpha * 0xff + 0x8000) >> 16;

	      bufptr += 4;
	      dstptr += 4;
	    }
	}
      else
	{
	  for (x = run_x0; x < run_x1; x++)
	    {
	      tmp = run_alpha * bufptr[3] + 0x80;
	      /* range 0x80 .. 0xff0080 */
	      src_alpha = (tmp + (tmp >> 8) + (tmp >> 16)) >> 8;
	      /* src_alpha is the (alpha of the source pixel * alpha),
		 range 0..0x10000 */

	      src_mul = run_alpha * 0x101;

	      tmp = dstptr[3];
	      /* range 0..0xff */
	      dst_alpha = (tmp << 8) + tmp + (tmp >> 7);
	      dst_mul = dst_alpha;
	      /* dst_alpha is the alpha of the dest pixel,
		 range 0..0x10000 */

	      dst_mul *= 0x101;

	      if (src_alpha >= 0x10000)
		dst_alpha = 0x10000;
	      else
		dst_alpha += ((((0x10000 - dst_alpha) * src_alpha) >> 8) + 0x80) >> 8;

	      if (dst_alpha == 0)
		{
		  dst_save_mul = 0xff;
		}
	      else /* dst_alpha != 0) */
		{
		  dst_save_mul = 0xff0000 / dst_alpha;
		}

	      for (j = 0; j < 3; j++)
		{
		  art_u32 src, dst;
		  art_u32 tmp;

		  src = (bufptr[j] * src_mul + 0x8000) >> 16;
		  dst = (dstptr[j] * dst_mul + 0x8000) >> 16;
		  tmp = ((dst * (0x10000 - src_alpha) + 0x8000) >> 16) + src;
		  tmp -= tmp >> 16;
		  dstptr[j] = (tmp * dst_save_mul + 0x8000) >> 16;
		}
	      dstptr[3] = (dst_alpha * 0xff + 0x8000) >> 16;

	      bufptr += 4;
	      dstptr += 4;
	    }
	}
    }
}

const ArtRenderCallback art_render_composite_8_opt2_obj =
{
  art_render_composite_8_opt2,
  art_render_nop_done
};

