/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <NotZed@Ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
 *  Copyright 2001 Ximian Inc. (www.ximian.com)
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

#ifndef CAMEL_FILTER_SEARCH_H
#define CAMEL_FILTER_SEARCH_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-folder-summary.h>

enum {
	CAMEL_SEARCH_ERROR    = -1,
	CAMEL_SEARCH_NOMATCH  =  0,
	CAMEL_SEARCH_MATCHED  =  1,
};

typedef CamelMimeMessage * (*CamelFilterSearchGetMessageFunc) (void *data, CamelException *ex);

int camel_filter_search_match (CamelSession *session,
			       CamelFilterSearchGetMessageFunc get_message, void *data,
			       CamelMessageInfo *info, const char *source,
			       const char *expression, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! CAMEL_FILTER_SEARCH_H */
