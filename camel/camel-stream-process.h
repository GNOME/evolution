/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2003 Ximian Inc.
 *
 *  Authors: David Woodhouse <dwmw2@infradead.org>
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
 */


#ifndef _CAMEL_STREAM_NULL_H
#define _CAMEL_STREAM_NULL_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-stream.h>

#define CAMEL_STREAM_PROCESS(obj)         CAMEL_CHECK_CAST (obj, camel_stream_process_get_type (), CamelStreamProcess)
#define CAMEL_STREAM_PROCESS_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_stream_process_get_type (), CamelStreamProcessClass)
#define CAMEL_IS_STREAM_PROCESS(obj)      CAMEL_CHECK_TYPE (obj, camel_stream_process_get_type ())

typedef struct _CamelStreamProcessClass CamelStreamProcessClass;
typedef struct _CamelStreamProcess CamelStreamProcess;

struct _CamelStreamProcess {
	CamelStream parent;
	
	int sockfd;
	pid_t childpid;
};

struct _CamelStreamProcessClass {
	CamelStreamClass parent_class;
};

CamelType		camel_stream_process_get_type	(void);
CamelStream            *camel_stream_process_new		(void);
int camel_stream_process_connect(CamelStreamProcess *, const char *, const char **);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_STREAM_PROCESS_H */
