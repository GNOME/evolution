/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2004 Ximian, Inc. (www.ximian.com)
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


#ifndef __CAMEL_MIME_FILTER_YENC_H__
#define __CAMEL_MIME_FILTER_YENC_H__

#include <camel/camel-mime-filter.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_TYPE_MIME_FILTER_YENC            (camel_mime_filter_yenc_get_type ())
#define CAMEL_MIME_FILTER_YENC(obj)            (CAMEL_CHECK_CAST ((obj), CAMEL_TYPE_MIME_FILTER_YENC, CamelMimeFilterYenc))
#define CAMEL_MIME_FILTER_YENC_CLASS(klass)    (CAMEL_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_MIME_FILTER_YENC, CamelMimeFilterYencClass))
#define CAMEL_IS_MIME_FILTER_YENC(obj)         (CAMEL_CHECK_TYPE ((obj), CAMEL_TYPE_MIME_FILTER_YENC))
#define CAMEL_IS_MIME_FILTER_YENC_CLASS(klass) (CAMEL_CHECK_CLASS_TYPE ((klass), CAMEL_TYPE_MIME_FILTER_YENC))
#define CAMEL_MIME_FILTER_YENC_GET_CLASS(obj)  (CAMEL_CHECK_GET_CLASS ((obj), CAMEL_TYPE_MIME_FILTER_YENC, CamelMimeFilterYencClass))

typedef struct _CamelMimeFilterYenc CamelMimeFilterYenc;
typedef struct _CamelMimeFilterYencClass CamelMimeFilterYencClass;

typedef enum {
	CAMEL_MIME_FILTER_YENC_DIRECTION_ENCODE,
	CAMEL_MIME_FILTER_YENC_DIRECTION_DECODE
} CamelMimeFilterYencDirection;

#define CAMEL_MIME_YDECODE_STATE_INIT     (0)
#define CAMEL_MIME_YENCODE_STATE_INIT     (0)

/* first 8 bits are reserved for saving a byte */

/* reserved for use only within camel_mime_ydecode_step */
#define CAMEL_MIME_YDECODE_STATE_EOLN     (1 << 8)
#define CAMEL_MIME_YDECODE_STATE_ESCAPE   (1 << 9)

/* bits 10 and 11 reserved for later uses? */

#define CAMEL_MIME_YDECODE_STATE_BEGIN    (1 << 12)
#define CAMEL_MIME_YDECODE_STATE_PART     (1 << 13)
#define CAMEL_MIME_YDECODE_STATE_DECODE   (1 << 14)
#define CAMEL_MIME_YDECODE_STATE_END      (1 << 15)

#define CAMEL_MIME_YENCODE_CRC_INIT       (~0)
#define CAMEL_MIME_YENCODE_CRC_FINAL(crc) (~crc)

struct _CamelMimeFilterYenc {
	CamelMimeFilter parent_object;
	
	CamelMimeFilterYencDirection direction;
	
	int part;
	
	int state;
	guint32 pcrc;
	guint32 crc;
};

struct _CamelMimeFilterYencClass {
	CamelMimeFilterClass parent_class;
	
};


CamelType camel_mime_filter_yenc_get_type (void);

CamelMimeFilter *camel_mime_filter_yenc_new (CamelMimeFilterYencDirection direction);

void camel_mime_filter_yenc_set_state (CamelMimeFilterYenc *yenc, int state);
void camel_mime_filter_yenc_set_crc (CamelMimeFilterYenc *yenc, guint32 crc);

/*int     camel_mime_filter_yenc_get_part (CamelMimeFilterYenc *yenc);*/
guint32 camel_mime_filter_yenc_get_pcrc (CamelMimeFilterYenc *yenc);
guint32 camel_mime_filter_yenc_get_crc (CamelMimeFilterYenc *yenc);


size_t camel_ydecode_step  (const unsigned char *in, size_t inlen, unsigned char *out,
			    int *state, guint32 *pcrc, guint32 *crc);
size_t camel_yencode_step  (const unsigned char *in, size_t inlen, unsigned char *out,
			    int *state, guint32 *pcrc, guint32 *crc);
size_t camel_yencode_close (const unsigned char *in, size_t inlen, unsigned char *out,
			    int *state, guint32 *pcrc, guint32 *crc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_MIME_FILTER_YENC_H__ */
