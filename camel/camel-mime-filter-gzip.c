/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <zlib.h>

#include "camel-mime-filter-gzip.h"


/* rfc1952 */

enum {
	GZIP_FLAG_FTEXT     = (1 << 0),
	GZIP_FLAG_FHCRC     = (1 << 1),
	GZIP_FLAG_FEXTRA    = (1 << 2),
	GZIP_FLAG_FNAME     = (1 << 3),
	GZIP_FLAG_FCOMMENT  = (1 << 4),
	GZIP_FLAG_RESERVED0 = (1 << 5),
	GZIP_FLAG_RESERVED1 = (1 << 6),
	GZIP_FLAG_RESERVED2 = (1 << 7),
};

#define GZIP_FLAG_RESERVED (GZIP_FLAG_RESERVED0 | GZIP_FLAG_RESERVED1 | GZIP_FLAG_RESERVED2)

typedef union {
	unsigned char buf[10];
	struct {
		guint8 id1;
		guint8 id2;
		guint8 cm;
		guint8 flg;
		guint32 mtime;
		guint8 xfl;
		guint8 os;
	} v;
} gzip_hdr_t;

typedef union {
	struct {
		guint16 xlen;
		guint16 xlen_nread;
		guint16 crc16;
		
		guint8 got_hdr:1;
		guint8 is_valid:1;
		guint8 got_xlen:1;
		guint8 got_fname:1;
		guint8 got_fcomment:1;
		guint8 got_crc16:1;
	} unzip;
	struct {
		guint32 wrote_hdr:1;
	} zip;
} gzip_state_t;

struct _CamelMimeFilterGZipPrivate {
	z_stream *stream;
	
	gzip_state_t state;
	gzip_hdr_t hdr;
	
	guint32 crc32;
	guint32 isize;
};

static void camel_mime_filter_gzip_class_init (CamelMimeFilterGZipClass *klass);
static void camel_mime_filter_gzip_init (CamelMimeFilterGZip *filter, CamelMimeFilterGZipClass *klass);
static void camel_mime_filter_gzip_finalize (CamelObject *object);

static void filter_filter (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (CamelMimeFilter *filter);


static CamelMimeFilterClass *parent_class = NULL;


CamelType
camel_mime_filter_gzip_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type (),
					    "CamelMimeFilterGZip",
					    sizeof (CamelMimeFilterGZip),
					    sizeof (CamelMimeFilterGZipClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_gzip_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_filter_gzip_init,
					    (CamelObjectFinalizeFunc) camel_mime_filter_gzip_finalize);
	}
	
	return type;
}


static void
camel_mime_filter_gzip_class_init (CamelMimeFilterGZipClass *klass)
{
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	
	parent_class = CAMEL_MIME_FILTER_CLASS (camel_type_get_global_classfuncs (camel_mime_filter_get_type ()));
	
	filter_class->reset = filter_reset;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
}

static void
camel_mime_filter_gzip_init (CamelMimeFilterGZip *filter, CamelMimeFilterGZipClass *klass)
{
	filter->priv = g_new0 (struct _CamelMimeFilterGZipPrivate, 1);
	filter->priv->stream = g_new0 (z_stream, 1);
	filter->priv->crc32 = crc32 (0, Z_NULL, 0);
}

static void
camel_mime_filter_gzip_finalize (CamelObject *object)
{
	CamelMimeFilterGZip *gzip = (CamelMimeFilterGZip *) object;
	struct _CamelMimeFilterGZipPrivate *priv = gzip->priv;
	
	if (gzip->mode == CAMEL_MIME_FILTER_GZIP_MODE_ZIP)
		deflateEnd (priv->stream);
	else
		inflateEnd (priv->stream);
	
	g_free (priv->stream);
	g_free (priv);
}


static void
gzip_filter (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
	     char **out, size_t *outlen, size_t *outprespace, int flush)
{
	CamelMimeFilterGZip *gzip = (CamelMimeFilterGZip *) filter;
	struct _CamelMimeFilterGZipPrivate *priv = gzip->priv;
	int retval;
	
	if (!priv->state.zip.wrote_hdr) {
		priv->hdr.v.id1 = 31;
		priv->hdr.v.id2 = 139;
		priv->hdr.v.cm = Z_DEFLATED;
		priv->hdr.v.mtime = 0;
		priv->hdr.v.flg = 0;
		if (gzip->level == Z_BEST_COMPRESSION)
			priv->hdr.v.xfl = 2;
		else if (gzip->level == Z_BEST_SPEED)
			priv->hdr.v.xfl = 4;
		else
			priv->hdr.v.xfl = 0;
		priv->hdr.v.os = 255;
		
		camel_mime_filter_set_size (filter, (len * 2) + 22, FALSE);
		
		memcpy (filter->outbuf, priv->hdr.buf, 10);
		
		priv->stream->next_out = filter->outbuf + 10;
		priv->stream->avail_out = filter->outsize - 10;
		
		priv->state.zip.wrote_hdr = TRUE;
	} else {
		camel_mime_filter_set_size (filter, (len * 2) + 12, FALSE);
		
		priv->stream->next_out = filter->outbuf;
		priv->stream->avail_out = filter->outsize;
	}
	
	priv->stream->next_in = in;
	priv->stream->avail_in = len;
	
	do {
		/* FIXME: handle error cases? */
		if ((retval = deflate (priv->stream, flush)) != Z_OK)
			fprintf (stderr, "gzip: %d: %s\n", retval, priv->stream->msg);
		
		if (flush == Z_FULL_FLUSH) {
			size_t outlen;
			
			outlen = filter->outsize - priv->stream->avail_out;
			camel_mime_filter_set_size (filter, outlen + (priv->stream->avail_in * 2) + 12, TRUE);
			priv->stream->avail_out = filter->outsize - outlen;
			priv->stream->next_out = filter->outbuf + outlen;
			
			if (priv->stream->avail_in == 0) {
				guint32 val;
				
				val = GUINT32_TO_LE (priv->crc32);
				memcpy (priv->stream->next_out, &val, 4);
				priv->stream->avail_out -= 4;
				priv->stream->next_out += 4;
				
				val = GUINT32_TO_LE (priv->isize);
				memcpy (priv->stream->next_out, &val, 4);
				priv->stream->avail_out -= 4;
				priv->stream->next_out += 4;
				
				break;
			}
		} else {
			if (priv->stream->avail_in > 0)
				camel_mime_filter_backup (filter, priv->stream->next_in, priv->stream->avail_in);
			
			break;
		}
	} while (1);
	
	priv->crc32 = crc32 (priv->crc32, in, len - priv->stream->avail_in);
	priv->isize += len - priv->stream->avail_in;
	
	*out = filter->outbuf;
	*outlen = filter->outsize - priv->stream->avail_out;
	*outprespace = filter->outpre;
}

static void
gunzip_filter (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace, int flush)
{
	CamelMimeFilterGZip *gzip = (CamelMimeFilterGZip *) filter;
	struct _CamelMimeFilterGZipPrivate *priv = gzip->priv;
	guint16 need, val;
	int retval;
	
	if (!priv->state.unzip.got_hdr) {
		if (len < 10) {
			camel_mime_filter_backup (filter, in, len);
			return;
		}
		
		memcpy (priv->hdr.buf, in, 10);
		priv->state.unzip.got_hdr = TRUE;
		len -= 10;
		in += 10;
		
		priv->state.unzip.is_valid = (priv->hdr.v.id1 == 31 &&
					      priv->hdr.v.id2 == 139 &&
					      priv->hdr.v.cm == Z_DEFLATED);
	}
	
	if (!priv->state.unzip.is_valid)
		return;
	
	if (priv->hdr.v.flg & GZIP_FLAG_FEXTRA) {
		if (!priv->state.unzip.got_xlen) {
			if (len < 2) {
				camel_mime_filter_backup (filter, in, len);
				return;
			}
			
			memcpy (&val, in, 2);
			priv->state.unzip.xlen = GUINT16_FROM_LE (val);
			priv->state.unzip.got_xlen = TRUE;
			len -= 2;
			in += 2;
		}
		
		if (priv->state.unzip.xlen_nread < priv->state.unzip.xlen) {
			need = priv->state.unzip.xlen - priv->state.unzip.xlen_nread;
			
			if (need < len) {
				priv->state.unzip.xlen_nread += need;
				len -= need;
				in += need;
			} else {
				priv->state.unzip.xlen_nread += len;
				return;
			}
		}
	}
	
	if ((priv->hdr.v.flg & GZIP_FLAG_FNAME) && !priv->state.unzip.got_fname) {
		while (*in && len > 0) {
			len--;
			in++;
		}
		
		if (*in == '\0' && len > 0) {
			priv->state.unzip.got_fname = TRUE;
			len--;
			in++;
		} else {
			return;
		}
	}
	
	if ((priv->hdr.v.flg & GZIP_FLAG_FCOMMENT) && !priv->state.unzip.got_fcomment) {
		while (*in && len > 0) {
			len--;
			in++;
		}
		
		if (*in == '\0' && len > 0) {
			priv->state.unzip.got_fcomment = TRUE;
			len--;
			in++;
		} else {
			return;
		}
	}
	
	if ((priv->hdr.v.flg & GZIP_FLAG_FHCRC) && !priv->state.unzip.got_crc16) {
		if (len < 2) {
			camel_mime_filter_backup (filter, in, len);
			return;
		}
		
		memcpy (&val, in, 2);
		priv->state.unzip.crc16 = GUINT16_FROM_LE (val);
		len -= 2;
		in += 2;
	}
	
	if (len == 0)
		return;
	
	camel_mime_filter_set_size (filter, (len * 2) + 12, FALSE);
	
	priv->stream->next_in = in;
	priv->stream->avail_in = len - 8;
	
	priv->stream->next_out = filter->outbuf;
	priv->stream->avail_out = filter->outsize;
	
	do {
		/* FIXME: handle error cases? */
		if ((retval = inflate (priv->stream, flush)) != Z_OK)
			fprintf (stderr, "gunzip: %d: %s\n", retval, priv->stream->msg);
		
		if (flush == Z_FULL_FLUSH) {
			size_t outlen;
			
			if (priv->stream->avail_in == 0) {
				/* FIXME: extract & compare calculated crc32 and isize values? */
				break;
			}
			
			outlen = filter->outsize - priv->stream->avail_out;
			camel_mime_filter_set_size (filter, outlen + (priv->stream->avail_in * 2) + 12, TRUE);
			priv->stream->avail_out = filter->outsize - outlen;
			priv->stream->next_out = filter->outbuf + outlen;
		} else {
			priv->stream->avail_in += 8;
			
			if (priv->stream->avail_in > 0)
				camel_mime_filter_backup (filter, priv->stream->next_in, priv->stream->avail_in);
			
			break;
		}
	} while (1);
	
	/* FIXME: if we keep this, we could check that the gzip'd
	 * stream is sane, but how would we tell our consumer if it
	 * was/wasn't? */
	/*priv->crc32 = crc32 (priv->crc32, in, len - priv->stream->avail_in - 8);
	  priv->isize += len - priv->stream->avail_in - 8;*/
	
	*out = filter->outbuf;
	*outlen = filter->outsize - priv->stream->avail_out;
	*outprespace = filter->outpre;
}

static void
filter_filter (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterGZip *gzip = (CamelMimeFilterGZip *) filter;
	
	if (gzip->mode == CAMEL_MIME_FILTER_GZIP_MODE_ZIP)
		gzip_filter (filter, in, len, prespace, out, outlen, outprespace, Z_SYNC_FLUSH);
	else
		gunzip_filter (filter, in, len, prespace, out, outlen, outprespace, Z_SYNC_FLUSH);
}

static void
filter_complete (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterGZip *gzip = (CamelMimeFilterGZip *) filter;
	
	if (gzip->mode == CAMEL_MIME_FILTER_GZIP_MODE_ZIP)
		gzip_filter (filter, in, len, prespace, out, outlen, outprespace, Z_FULL_FLUSH);
	else
		gunzip_filter (filter, in, len, prespace, out, outlen, outprespace, Z_FULL_FLUSH);
}

/* should this 'flush' outstanding state/data bytes? */
static void
filter_reset (CamelMimeFilter *filter)
{
	CamelMimeFilterGZip *gzip = (CamelMimeFilterGZip *) filter;
	struct _CamelMimeFilterGZipPrivate *priv = gzip->priv;
	
	memset (&priv->state, 0, sizeof (priv->state));
	
	if (gzip->mode == CAMEL_MIME_FILTER_GZIP_MODE_ZIP)
		deflateReset (priv->stream);
	else
		inflateReset (priv->stream);
	
	priv->crc32 = crc32 (0, Z_NULL, 0);
	priv->isize = 0;
}


/**
 * camel_mime_filter_gzip_new:
 * @mode: zip or unzip
 * @level: compression level
 *
 * Creates a new gzip (or gunzip) filter.
 *
 * Returns a new gzip (or gunzip) filter.
 **/
CamelMimeFilter *
camel_mime_filter_gzip_new (CamelMimeFilterGZipMode mode, int level)
{
	CamelMimeFilterGZip *new;
	int retval;
	
	new = (CamelMimeFilterGZip *) camel_object_new (CAMEL_TYPE_MIME_FILTER_GZIP);
	new->mode = mode;
	new->level = level;
	
	if (mode == CAMEL_MIME_FILTER_GZIP_MODE_ZIP)
		retval = deflateInit2 (new->priv->stream, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	else
		retval = inflateInit2 (new->priv->stream, -MAX_WBITS);
	
	if (retval != Z_OK) {
		camel_object_unref (new);
		return NULL;
	}
	
	return (CamelMimeFilter *) new;
}
