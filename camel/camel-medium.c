/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* camelMedium.c : Abstract class for a medium
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 * 	    Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include "camel-medium.h"
#include "gmime-content-field.h"
#include "string-utils.h"
#include "gmime-utils.h"
#include "hash-table-utils.h"
#include "camel-simple-data-wrapper.h"

#define d(x)

static CamelDataWrapperClass *parent_class = NULL;

/* Returns the class for a CamelMedium */
#define CM_CLASS(so) CAMEL_MEDIUM_CLASS (GTK_OBJECT (so)->klass)

static void add_header (CamelMedium *medium, const gchar *header_name,
			const void *header_value);
static void set_header (CamelMedium *medium, const gchar *header_name, const void *header_value);
static void remove_header (CamelMedium *medium, const gchar *header_name);
static const void *get_header (CamelMedium *medium, const gchar *header_name);

static CamelDataWrapper *get_content_object (CamelMedium *medium);
static void set_content_object (CamelMedium *medium,
				CamelDataWrapper *content);
static void finalize (GtkObject *object);

static void
camel_medium_class_init (CamelMediumClass *camel_medium_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class =
		CAMEL_DATA_WRAPPER_CLASS (camel_medium_class);
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_data_wrapper_class);

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());

	/* virtual method definition */
	camel_medium_class->add_header = add_header;
	camel_medium_class->set_header = set_header;
	camel_medium_class->remove_header = remove_header;
	camel_medium_class->get_header = get_header;

	camel_medium_class->set_content_object = set_content_object;
	camel_medium_class->get_content_object = get_content_object;

	gtk_object_class->finalize = finalize;
}

static void
camel_medium_init (gpointer object, gpointer klass)
{
	CamelMedium *camel_medium = CAMEL_MEDIUM (object);

	camel_medium->content = NULL;
}


GtkType
camel_medium_get_type (void)
{
	static GtkType camel_medium_type = 0;

	if (!camel_medium_type) {
		GtkTypeInfo camel_medium_info =
		{
			"CamelMedium",
			sizeof (CamelMedium),
			sizeof (CamelMediumClass),
			(GtkClassInitFunc) camel_medium_class_init,
			(GtkObjectInitFunc) camel_medium_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_medium_type = gtk_type_unique (camel_data_wrapper_get_type (), &camel_medium_info);
	}

	return camel_medium_type;
}

static void
finalize (GtkObject *object)
{
	CamelMedium *medium = CAMEL_MEDIUM (object);

	if (medium->content)
		gtk_object_unref (GTK_OBJECT (medium->content));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
add_header (CamelMedium *medium, const gchar *header_name,
	    const void *header_value)
{
	g_warning("No %s::add_header implemented, adding %s", gtk_type_name(((GtkObject *)medium)->klass->type), header_name);
}

/**
 * camel_medium_add_header:
 * @medium: a CamelMedium
 * @header_name: name of the header
 * @header_value: value of the header
 *
 * Adds a header to a medium.
 *
 * FIXME: Where does it add it? We need to be able to prepend and
 * append headers, and also be able to insert them relative to other
 * headers.   No we dont, order isn't important! Z
 **/
void
camel_medium_add_header (CamelMedium *medium, const gchar *header_name,
			 const void *header_value)
{
	g_return_if_fail (CAMEL_IS_MEDIUM (medium));
	g_return_if_fail (header_name != NULL);
	g_return_if_fail (header_value != NULL);

	CM_CLASS (medium)->add_header (medium, header_name, header_value);
}

static void
set_header (CamelMedium *medium, const gchar *header_name, const void *header_value)
{
	g_warning("No %s::set_header implemented, setting %s", gtk_type_name(((GtkObject *)medium)->klass->type), header_name);
}

/**
 * camel_medium_set_header:
 * @medium: a CamelMedium
 * @header_name: name of the header
 * @header_value: value of the header
 *
 * Sets the value of a header.  Any other occurances of the header
 * will be removed.
 **/
void
camel_medium_set_header (CamelMedium *medium, const gchar *header_name, const void *header_value)
{
	g_return_if_fail (CAMEL_IS_MEDIUM (medium));
	g_return_if_fail (header_name != NULL);
	g_return_if_fail (header_value != NULL);

	CM_CLASS (medium)->add_header (medium, header_name, header_value);
}

static void
remove_header (CamelMedium *medium, const gchar *header_name)
{
	g_warning("No %s::remove_header implemented, removing %s", gtk_type_name(((GtkObject *)medium)->klass->type), header_name);
}

/**
 * camel_medium_remove_header:
 * @medium: a medium
 * @header_name: the name of the header
 *
 * Removes the named header from the medium.  All occurances of the
 * header are removed.
 **/
void
camel_medium_remove_header (CamelMedium *medium, const gchar *header_name)
{
	g_return_if_fail (CAMEL_IS_MEDIUM (medium));
	g_return_if_fail (header_name != NULL);

	CM_CLASS (medium)->remove_header (medium, header_name);
}


static const void *
get_header (CamelMedium *medium, const gchar *header_name)
{
	g_warning("No %s::get_header implemented, getting %s", gtk_type_name(((GtkObject *)medium)->klass->type), header_name);
	return NULL;
}

/**
 * camel_medium_get_header:
 * @medium: a medium
 * @header_name: the name of the header
 *
 * Returns the value of the named header in the medium, or %NULL if
 * it is unset. The caller should not modify or free the data.
 *
 * FIXME: What if the header occurs more than once?
 *
 * Return value: the value of the named header, or %NULL
 **/
const void *
camel_medium_get_header (CamelMedium *medium, const gchar *header_name)
{
	g_return_val_if_fail (CAMEL_IS_MEDIUM (medium), NULL);
	g_return_val_if_fail (header_name != NULL, NULL);

#warning No way to get multi-valued headers?

	return CM_CLASS (medium)->get_header (medium, header_name);
}


static CamelDataWrapper *
get_content_object (CamelMedium *medium)
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
		gtk_object_unref (GTK_OBJECT (medium->content));
	gtk_object_ref (GTK_OBJECT (content));
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
