/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-medium.h : class for a medium object */

/*
 *
 * Authors:  Bertrand Guiheneuf <bertrand@helixcode.com>
 *	     Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef CAMEL_MEDIUM_H
#define CAMEL_MEDIUM_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-data-wrapper.h>

#define CAMEL_MEDIUM_TYPE     (camel_medium_get_type ())
#define CAMEL_MEDIUM(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_MEDIUM_TYPE, CamelMedium))
#define CAMEL_MEDIUM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_MEDIUM_TYPE, CamelMediumClass))
#define CAMEL_IS_MEDIUM(o)    (CAMEL_CHECK_TYPE((o), CAMEL_MEDIUM_TYPE))


typedef struct {
	const char *name;
	const char *value;
} CamelMediumHeader;

struct _CamelMedium {
	CamelDataWrapper parent_object;

	/* The content of the medium, as opposed to our parent
	 * CamelDataWrapper, which wraps both the headers and the
	 * content.
	 */
	CamelDataWrapper *content;
};

typedef struct {
	CamelDataWrapperClass parent_class;

	/* Virtual methods */
	void  (*add_header) (CamelMedium *medium, const char *name, const void *value);
	void  (*set_header) (CamelMedium *medium, const char *name, const void *value);
	void  (*remove_header) (CamelMedium *medium, const char *name);
	const void * (*get_header) (CamelMedium *medium,  const char *name);

	GArray * (*get_headers) (CamelMedium *medium);
	void (*free_headers) (CamelMedium *medium, GArray *headers);

	CamelDataWrapper * (*get_content_object) (CamelMedium *medium);
	void (*set_content_object) (CamelMedium *medium, CamelDataWrapper *content);
} CamelMediumClass;

/* Standard Camel function */
CamelType camel_medium_get_type (void);

/* Header get/set interface */
void camel_medium_add_header (CamelMedium *medium, const char *name, const void *value);
void camel_medium_set_header (CamelMedium *medium, const char *name, const void *value);
void camel_medium_remove_header (CamelMedium *medium, const char *name);
const void *camel_medium_get_header (CamelMedium *medium, const char *name);

GArray *camel_medium_get_headers (CamelMedium *medium);
void camel_medium_free_headers (CamelMedium *medium, GArray *headers);

/* accessor methods */
CamelDataWrapper *camel_medium_get_content_object (CamelMedium *medium);
void camel_medium_set_content_object (CamelMedium *medium,
				      CamelDataWrapper *content);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MEDIUM_H */

