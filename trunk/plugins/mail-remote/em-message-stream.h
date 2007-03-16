/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#ifndef EM_MESSAGE_STREAM_H
#define EM_MESSAGE_STREAM_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_MESSAGE_STREAM_TYPE     (em_message_stream_get_type ())
#define EM_MESSAGE_STREAM(obj)     (CAMEL_CHECK_CAST((obj), EM_MESSAGE_STREAM_TYPE, EMMessageStream))
#define EM_MESSAGE_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_MESSAGE_STREAM_TYPE, EMMessageStreamClass))
#define EM_IS_MESSAGE_STREAM(o)    (CAMEL_CHECK_TYPE((o), EM_MESSAGE_STREAM_TYPE))

#include <camel/camel-stream.h>
#include "Evolution-DataServer-Mail.h"

typedef struct _EMMessageStream {
	CamelStream parent_stream;

	Evolution_Mail_MessageStream source;
} EMMessageStream;

typedef struct {
	CamelStreamClass parent_class;
} EMMessageStreamClass;

CamelType    em_message_stream_get_type (void);

CamelStream *em_message_stream_new(const Evolution_Mail_MessageStream source);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_MESSAGE_STREAM_H */
