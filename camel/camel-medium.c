/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* camelMedium.c : Abstract class for a medium
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 * 	    Michael Zucchi <notzed@ximian.com>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>

#include "camel-medium.h"

#define d(x)

static CamelDataWrapperClass *parent_class = NULL;

/* Returns the class for a CamelMedium */
#define CM_CLASS(so) CAMEL_MEDIUM_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static gboolean is_offline (CamelDataWrapper *data_wrapper);
static void add_header (CamelMedium *medium, const char *name,
			const void *value);
static void set_header (CamelMedium *medium, const char *name, const void *value);
static void remove_header (CamelMedium *medium, const char *name);
static const void *get_header (CamelMedium *medium, const char *name);

static GArray *get_headers (CamelMedium *medium);
static void free_headers (CamelMedium *medium, GArray *headers);

static CamelDataWrapper *get_content_object (CamelMedium *medium);
static void set_content_object (CamelMedium *medium,
				CamelDataWrapper *content);

static void
camel_medium_class_init (CamelMediumClass *camel_medium_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class =
		CAMEL_DATA_WRAPPER_CLASS (camel_medium_class);

	parent_class = CAMEL_DATA_WRAPPER_CLASS (camel_type_get_global_classfuncs (camel_data_wrapper_get_type ()));

	/* virtual method overload */
	camel_data_wrapper_class->is_offline = is_offline;

	/* virtual method definition */
	camel_medium_class->add_header = add_header;
	camel_medium_class->set_header = set_header;
	camel_medium_class->remove_header = remove_header;
	camel_medium_class->get_header = get_header;

	camel_medium_class->get_headers = get_headers;
	camel_medium_class->free_headers = free_headers;

	camel_medium_class->set_content_object = set_content_object;
	camel_medium_class->get_content_object = get_content_object;
}

static void
camel_medium_init (gpointer object, gpointer klass)
{
	CamelMedium *camel_medium = CAMEL_MEDIUM (object);

	camel_medium->content = NULL;
}

static void
camel_medium_finalize (CamelObject *object)
{
	CamelMedium *medium = CAMEL_MEDIUM (object);

	if (medium->content)
		camel_object_unref (medium->content);
}


CamelType
camel_medium_get_type (void)
{
	static CamelType camel_medium_type = CAMEL_INVALID_TYPE;

	if (camel_medium_type == CAMEL_INVALID_TYPE) {
		camel_medium_type = camel_type_register (CAMEL_DATA_WRAPPER_TYPE, "medium",
							 sizeof (CamelMedium),
							 sizeof (CamelMediumClass),
							 (CamelObjectClassInitFunc) camel_medium_class_init,
							 NULL,
							 (CamelObjectInitFunc) camel_medium_init,
							 (CamelObjectFinalizeFunc) camel_medium_finalize);
	}

	return camel_medium_type;
}

static gboolean
is_offline (CamelDataWrapper *data_wrapper)
{
	return parent_class->is_offline (data_wrapper) ||
		camel_data_wrapper_is_offline (CAMEL_MEDIUM (data_wrapper)->content);
}

static void
add_header (CamelMedium *medium, const char *name, const void *value)
{
	g_warning("No %s::add_header implemented, adding %s", camel_type_to_name(CAMEL_OBJECT_GET_TYPE(medium)), name);
}

/**
 * camel_medium_add_header:
 * @medium: a CamelMedium
 * @name: name of the header
 * @value: value of the header
 *
 * Adds a header to a medium.
 *
 * FIXME: Where does it add it? We need to be able to prepend and
 * append headers, and also be able to insert them relative to other
 * headers.   No we dont, order isn't important! Z
 **/
void
camel_medium_add_header (CamelMedium *medium, const char *name, const void *value)
{
	g_return_if_fail (CAMEL_IS_MEDIUM (medium));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	CM_CLASS (medium)->add_header(medium, name, value);
}

static void
set_header (CamelMedium *medium, const char *name, const void *value)
{
	g_warning("No %s::set_header implemented, setting %s", camel_type_to_name(CAMEL_OBJECT_GET_TYPE(medium)), name);
}

/**
 * camel_medium_set_header:
 * @medium: a CamelMedium
 * @name: name of the header
 * @value: value of the header
 *
 * Sets the value of a header.  Any other occurances of the header
 * will be removed.  Setting a %NULL header can be used to remove
 * the header also.
 **/
void
camel_medium_set_header (CamelMedium *medium, const char *name, const void *value)
{
	g_return_if_fail (CAMEL_IS_MEDIUM (medium));
	g_return_if_fail (name != NULL);

	if (value == NULL)
		CM_CLASS(medium)->remove_header(medium, name);
	else
		CM_CLASS(medium)->set_header(medium, name, value);
}

static void
remove_header(CamelMedium *medium, const char *name)
{
	g_warning("No %s::remove_header implemented, removing %s", camel_type_to_name(CAMEL_OBJECT_GET_TYPE(medium)), name);
}

/**
 * camel_medium_remove_header:
 * @medium: a medium
 * @name: the name of the header
 *
 * Removes the named header from the medium.  All occurances of the
 * header are removed.
 **/
void
camel_medium_remove_header(CamelMedium *medium, const char *name)
{
	g_return_if_fail (CAMEL_IS_MEDIUM (medium));
	g_return_if_fail (name != NULL);

	CM_CLASS(medium)->remove_header(medium, name);
}


static const void *
get_header(CamelMedium *medium, const char *name)
{
	g_warning("No %s::get_header implemented, getting %s", camel_type_to_name(CAMEL_OBJECT_GET_TYPE(medium)), name);
	return NULL;
}

/**
 * camel_medium_get_header:
 * @medium: a medium
 * @name: the name of the header
 *
 * Returns the value of the named header in the medium, or %NULL if
 * it is unset. The caller should not modify or free the data.
 *
 * If the header occurs more than once, only retrieve the first
 * instance of the header.  For multi-occuring headers, use
 * :get_headers().
 *
 * Return value: the value of the named header, or %NULL
 **/
const void *
camel_medium_get_header(CamelMedium *medium, const char *name)
{
	g_return_val_if_fail (CAMEL_IS_MEDIUM (medium), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return CM_CLASS (medium)->get_header (medium, name);
}


static GArray *
get_headers(CamelMedium *medium)
{
	g_warning("No %s::get_headers implemented", camel_type_to_name(CAMEL_OBJECT_GET_TYPE(medium)));
	return NULL;
}

/**
 * camel_medium_get_headers:
 * @medium: a medium
 *
 * Returns an array of all header name/value pairs (as
 * CamelMediumHeader structures). The values will be decoded
 * to UTF-8 for any headers that are recognized by Camel. The
 * caller should not modify the returned data.
 *
 * Return value: the array of headers, which must be freed with
 * camel_medium_free_headers().
 **/
GArray *
camel_medium_get_headers(CamelMedium *medium)
{
	g_return_val_if_fail (CAMEL_IS_MEDIUM (medium), NULL);

	return CM_CLASS (medium)->get_headers (medium);
}

static void
free_headers (CamelMedium *medium, GArray *headers)
{
	g_warning("No %s::free_headers implemented", camel_type_to_name(CAMEL_OBJECT_GET_TYPE(medium)));
}

/**
 * camel_medium_free_headers:
 * @medium: a medium
 * @headers: an array of headers returned from camel_medium_get_headers()
 *
 * Frees @headers
 **/
void
camel_medium_free_headers (CamelMedium *medium, GArray *headers)
{
	g_return_if_fail (CAMEL_IS_MEDIUM (medium));
	g_return_if_fail (headers != NULL);

	CM_CLASS (medium)->free_headers (medium, headers);
}


static CamelDataWrapper *
get_content_object(CamelMedium *medium)
{
	return medium->content;
}

/**
 * camel_medium_get_content_object:
 * @medium: a medium
 *
 * Returns a data wrapper that represents the content of the medium,
 * without its headers.
 *
 * Return value: the medium's content object.
 **/
CamelDataWrapper *
camel_medium_get_content_object (CamelMedium *medium)
{
	g_return_val_if_fail (CAMEL_IS_MEDIUM (medium), NULL);

	return CM_CLASS (medium)->get_content_object (medium);
}


static void
set_content_object (CamelMedium *medium, CamelDataWrapper *content)
{
	if (medium->content)
		camel_object_unref (medium->content);
	camel_object_ref (content);
	medium->content = content;
}

/**
 * camel_medium_set_content_object:
 * @medium: a medium
 * @content: a data wrapper representing the medium's content
 *
 * Sets the content of @medium to be @content.
 **/
void
camel_medium_set_content_object (CamelMedium *medium,
				 CamelDataWrapper *content)
{
	g_return_if_fail (CAMEL_IS_MEDIUM (medium));
	g_return_if_fail (CAMEL_IS_DATA_WRAPPER (content));

	CM_CLASS (medium)->set_content_object (medium, content);
}
