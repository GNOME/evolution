/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMedium.c : Abstract class for a medium */


/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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
#include "camel-medium.h"
#include <stdio.h>
#include "gmime-content-field.h"
#include "string-utils.h"
#include "camel-log.h"
#include "gmime-utils.h"
#include "camel-simple-data-wrapper.h"





static CamelDataWrapperClass *parent_class=NULL;

/* Returns the class for a CamelMedium */
#define CM_CLASS(so) CAMEL_MEDIUM_CLASS (GTK_OBJECT(so)->klass)

static void _add_header (CamelMedium *medium, gchar *header_name, gchar *header_value);
static void _remove_header (CamelMedium *medium, const gchar *header_name);
static const gchar *_get_header (CamelMedium *medium, const gchar *header_name);

static CamelDataWrapper *_get_content_object(CamelMedium *medium);
static void _set_content_object(CamelMedium *medium, CamelDataWrapper *content);

static void _finalize (GtkObject *object);

static void
camel_medium_class_init (CamelMediumClass *camel_medium_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_medium_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_data_wrapper_class);

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());
	
	/* virtual method definition */
	camel_medium_class->add_header = _add_header;
	camel_medium_class->remove_header = _remove_header;
	camel_medium_class->get_header = _get_header;
	
	camel_medium_class->set_content_object = _set_content_object;
	camel_medium_class->get_content_object = _get_content_object;
	
	/* virtual method overload */
	/* camel_data_wrapper_class->write_to_stream = _write_to_stream; */
	/* camel_data_wrapper_class->construct_from_stream = _construct_from_stream; */

	gtk_object_class->finalize = _finalize;
}

static void
camel_medium_init (gpointer   object,  gpointer   klass)
{
	CamelMedium *camel_medium = CAMEL_MEDIUM (object);
	
	camel_medium->headers =  g_hash_table_new (g_str_hash, g_str_equal);
	camel_medium->content =  NULL;
}




GtkType
camel_medium_get_type (void)
{
	static GtkType camel_medium_type = 0;
	
	if (!camel_medium_type)	{
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
_finalize (GtkObject *object)
{
	CamelMedium *medium = CAMEL_MEDIUM (object);


	CAMEL_LOG_FULL_DEBUG ("Entering CamelMedium::finalize\n");

	if (medium->headers) {
#warning Free hash table elements
		g_hash_table_destroy (medium->headers);
	}

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMedium::finalize\n");
}



/* **** */

static void
_add_header (CamelMedium *medium, gchar *header_name, gchar *header_value)
{
	gboolean header_exists;
	gchar *old_header_name;
	gchar *old_header_value;
	

	header_exists = g_hash_table_lookup_extended (medium->headers, header_name, 
						      (gpointer *) &old_header_name,
						      (gpointer *) &old_header_value);
	if (header_exists) {
		g_free (old_header_name);
		g_free (old_header_value);
	}
	
	g_hash_table_insert (medium->headers, header_name, header_value);
}


void
camel_medium_add_header (CamelMedium *medium, gchar *header_name, gchar *header_value)
{
	CM_CLASS(medium)->add_header(medium, header_name, header_value);
}


/* **** */


static void
_remove_header (CamelMedium *medium, const gchar *header_name)
{
	
	gboolean header_exists;
	gchar *old_header_name;
	gchar *old_header_value;
	
	header_exists = g_hash_table_lookup_extended (medium->headers, header_name, 
						      (gpointer *) &old_header_name,
						      (gpointer *) &old_header_value);
	if (header_exists) {
		g_free (old_header_name);
		g_free (old_header_value);
	}
	
	g_hash_table_remove (medium->headers, header_name);
	
}

void
camel_medium_remove_header (CamelMedium *medium, const gchar *header_name)
{
	CM_CLASS(medium)->remove_header(medium, header_name);
}


/* **** */


static const gchar *
_get_header (CamelMedium *medium, const gchar *header_name)
{
	
	gchar *old_header_name;
	gchar *old_header_value;
	gchar *header_value;
	
	header_value = (gchar *)g_hash_table_lookup (medium->headers, header_name);
	return header_value;
}

const gchar *
camel_medium_get_header (CamelMedium *medium, const gchar *header_name)
{
	return CM_CLASS(medium)->get_header (medium, header_name);
}


/* **** */


static CamelDataWrapper *
_get_content_object (CamelMedium *medium)
{
	return medium->content;
	
}


CamelDataWrapper *
camel_medium_get_content_object (CamelMedium *medium)
{
	return CM_CLASS(medium)->get_content_object (medium);
}


/* **** */


static void
_set_content_object (CamelMedium *medium, CamelDataWrapper *content)
{
	GMimeContentField *object_content_field;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMedium::set_content_object\n");
	if (medium->content) {
		CAMEL_LOG_FULL_DEBUG ("CamelMedium::set_content_object unreferencing old content object\n");
		gtk_object_unref (GTK_OBJECT (medium->content));
	}
	gtk_object_ref (GTK_OBJECT (content));
	medium->content = content;
	
}

void 
camel_medium_set_content_object (CamelMedium *medium, CamelDataWrapper *content)
{
	CM_CLASS(medium)->set_content_object (medium, content);
}


/* **** */
