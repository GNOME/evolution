/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef CAMEL_TCP_STREAM_RAW_H
#define CAMEL_TCP_STREAM_RAW_H


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-tcp-stream.h>

#define CAMEL_TCP_STREAM_RAW_TYPE     (camel_tcp_stream_raw_get_type ())
#define CAMEL_TCP_STREAM_RAW(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_TCP_STREAM_RAW_TYPE, CamelTcpStreamRaw))
#define CAMEL_TCP_STREAM_RAW_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_TCP_STREAM_RAW_TYPE, CamelTcpStreamRawClass))
#define CAMEL_IS_TCP_STREAM_RAW(o)    (CAMEL_CHECK_TYPE((o), CAMEL_TCP_STREAM_RAW_TYPE))

struct _CamelTcpStreamRaw
{
	CamelTcpStream parent_object;
	
	int sockfd;
};

typedef struct {
	CamelTcpStreamClass parent_class;
	
	/* virtual functions */
	
} CamelTcpStreamRawClass;

/* Standard Camel function */
CamelType camel_tcp_stream_raw_get_type (void);

/* public methods */
CamelStream *camel_tcp_stream_raw_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_TCP_STREAM_RAW_H */
