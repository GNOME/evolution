/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mime-content_field.c : mime content type field utilities  */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
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
#include "gmime-content-field.h"
#include "string-utils.h"
#include <string.h>
#include "camel-mime-utils.h"

/**
 * gmime_content_field_new: Creates a new GMimeContentField object
 * @type: mime type
 * @subtype: mime subtype 
 * 
 * Creates a GMimeContentField object and initialize it with
 * a mime type and a mime subtype. For example, 
 * gmime_content_field_new ("application", "postcript");
 * will create a content field with complete mime type 
 * "application/postscript"
 * 
 * Return value: The newly created GMimeContentField object
 **/
GMimeContentField *
gmime_content_field_new (const gchar *type, const gchar *subtype)
{
	GMimeContentField *ctf;

	ctf = g_new (GMimeContentField, 1);
	ctf->content_type = header_content_type_new(type, subtype);
	ctf->type = ctf->content_type->type;
	ctf->subtype = ctf->content_type->subtype;
	ctf->ref = 1;
	return ctf;
} 

/**
 * gmime_content_field_ref: add a reference to a GMimeContentField object
 * @content_field: GMimeContentField object
 * 
 * Tell a GMimeContentField object that something holds a reference
 * on him. This, coupled with the corresponding 
 * gmime_content_field_unref() method allow several 
 * objects to use the same GMimeContentField object.
 **/
void 
gmime_content_field_ref (GMimeContentField *content_field)
{
	content_field->ref += 1;
	header_content_type_ref (content_field->content_type);
}

/**
 * gmime_content_field_unref: remove a reference to a GMimeContentField object
 * @content_field: GMimeContentField object
 * 
 * Tell a GMimeContentField object that something which 
 * was holding a reference to him does not need it anymore.
 * When no more reference exist, the GMimeContentField object
 * is freed using gmime_content_field_free().
 *
 **/
void 
gmime_content_field_unref (GMimeContentField *content_field)
{
	if (!content_field) return;
	
	content_field->ref -= 1;
	header_content_type_unref (content_field->content_type);
	if (content_field->ref <= 0)
		g_free (content_field);
}



/**
 * gmime_content_field_set_parameter: set a parameter for a GMimeContentField object
 * @content_field: content field
 * @attribute: parameter name 
 * @value: paramteter value
 * 
 * set a parameter (called attribute in RFC 2045) of a content field. Meaningfull
 * or valid parameters name depend on the content type object. For example, 
 * gmime_content_field_set_parameter (cf, "charset", "us-ascii");
 * will make sense for a "text/plain" content field but not for a 
 * "image/gif". This routine does not check parameter validity.
 **/
void 
gmime_content_field_set_parameter (GMimeContentField *content_field, const gchar *attribute, const gchar *value)
{
	header_content_type_set_param(content_field->content_type, attribute, value);
}

/**
 * gmime_content_field_write_to_stream: write a mime content type to a stream
 * @content_field: content type object
 * @stream: the stream
 * 
 * 
 **/
void
gmime_content_field_write_to_stream (GMimeContentField *content_field, CamelStream *stream)
{
	char *txt;

	if (!content_field)
		return;

	txt = header_content_type_format(content_field->content_type);
	if (txt) {
		camel_stream_printf (stream, "Content-Type: %s\n", txt);
		g_free(txt);
	}
}

/**
 * gmime_content_field_get_mime_type: return the mime type of the content field object
 * @content_field: content field object
 * 
 * A RFC 2045 content type field contains the mime type in the
 * form "type/subtype" (example : "application/postscript") and some
 * parameters (attribute/value pairs). This routine returns the mime type 
 * in a gchar object. THIS OBJECT MUST BE FREED BY THE CALLER.
 * 
 * Return value: the mime type in the form "type/subtype" or NULL if not defined.
 **/
gchar * 
gmime_content_field_get_mime_type (GMimeContentField *content_field)
{
	gchar *mime_type;

	if (!content_field->content_type->type) return NULL;

	if (content_field->content_type->subtype) 
		mime_type = g_strdup_printf ("%s/%s", content_field->content_type->type, content_field->content_type->subtype);
	else 
		mime_type = g_strdup (content_field->content_type->type);
	return mime_type;
}

/**
 * gmime_content_field_get_parameter: return the value of a mime type parameter
 * @content_field: content field object
 * @name: name of the parameter
 * 
 * Returns the value of a parameter contained in the content field 
 * object. The content type is formed of a mime type, a mime subtype,
 * and a parameter list. Each parameter is a name/value pair. This 
 * routine returns the value assiciated to a given name. 
 * When the parameter does not exist, NULL is returned. 
 * 
 * Return value: parameter value, or NULL if not found.
 **/
const gchar *
gmime_content_field_get_parameter (GMimeContentField *content_field, const gchar *name)
{
	g_assert (content_field);

	g_assert (name);
	return header_content_type_param(content_field->content_type, name);
}




/**
 * gmime_content_field_construct_from_string: construct a ContentType object by parsing a string.
 *
 * @content_field: content type object to construct 
 * @string: string containing the content type field 
 * 
 * Parse a string containing a content type field as defined in
 * RFC 2045, and construct the corresponding ContentType object.
 * The string is not modified and not used in the ContentType 
 * object. It can and must be freed by the calling part.
 **/
void
gmime_content_field_construct_from_string (GMimeContentField *content_field, const gchar *string)
{
	struct _header_content_type *new;

	g_assert (string);
	g_assert (content_field);

	new = header_content_type_decode(string);
	if (content_field->content_type)
		header_content_type_unref(content_field->content_type);

	if (new == NULL) {
		new = header_content_type_new(NULL, NULL);
		g_warning("Cannot parse content-type string: %s", string);
	}
	content_field->content_type = new;
	content_field->type = new->type;
	content_field->subtype = new->subtype;
}

/**
 * gmime_content_field_is_type:
 * @content_field: An initialised GMimeContentField.
 * @type: MIME Major type name.
 * @subtype: MIME subtype.
 * 
 * Returns true if the content_field is of the type @type and subtype @subtype.
 * If @subtype is the special wildcard "*", then it will match any type.
 *
 * If the @content_field is empty, then it will match "text/plain", or "text/ *".
 * 
 * Return value: 
 **/
int
gmime_content_field_is_type (GMimeContentField *content_field, const char *type, const char *subtype)
{
	return header_content_type_is(content_field->content_type, type, subtype);
}
