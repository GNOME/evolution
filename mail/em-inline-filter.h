/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef EM_INLINE_FILTER_H
#define EM_INLINE_FILTER_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_INLINE_FILTER_TYPE     (em_inline_filter_get_type ())
#define EM_INLINE_FILTER(obj)     (CAMEL_CHECK_CAST((obj), EM_INLINE_FILTER_TYPE, EMInlineFilter))
#define EM_INLINE_FILTER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_INLINE_FILTER_TYPE, EMInlineFilterClass))
#define EM_IS_INLINE_FILTER(o)    (CAMEL_CHECK_TYPE((o), EM_INLINE_FILTER_TYPE))

#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-utils.h>

typedef struct _EMInlineFilter {
	CamelMimeFilter filter;

	int state;

	CamelTransferEncoding base_encoding;
	CamelContentType *base_type;

	GByteArray *data;
	char *filename;
	GSList *parts;
} EMInlineFilter;

typedef struct _EMInlineFilterClass {
	CamelMimeFilterClass filter_class;
} EMInlineFilterClass;

CamelType    em_inline_filter_get_type(void);
EMInlineFilter *em_inline_filter_new(CamelTransferEncoding base_encoding, CamelContentType *type);
struct _CamelMultipart *em_inline_filter_get_multipart(EMInlineFilter *emif);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_INLINE_FILTER_H */
