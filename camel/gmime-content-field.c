/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mime-content_field.c : mime content type field utilities  */

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
#include "gmime-content-field.h"
#include "string-utils.h"
#include "camel-log.h"
#include <string.h>


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
	ctf->type = g_strdup (type);
	ctf->subtype = g_strdup (subtype);
	ctf->parameters =  g_hash_table_new (g_str_hash, g_str_equal);
	ctf->ref = 1;

	return ctf;
} 


static void
_free_parameter (gpointer name, gpointer value, gpointer user_data)
{
	g_free (name);
	g_free (value);
}

/**
 * gmime_content_field_free: free a GMimeContentField object
 * @content_field: GMimeContentField object
 * 
 * This method destroys the object and should be used very carefully.
 * Use gmime_content_field_unref instead.
 *
 **/
void 
gmime_content_field_free (GMimeContentField *content_field)
{
	if (!content_field) return;

	g_hash_table_foreach (content_field->parameters, _free_parameter, NULL);
	g_free (content_field->type);
	g_free (content_field->subtype);
	g_hash_table_destroy (content_field->parameters);
	g_free (content_field);
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
	if (content_field->ref <= 0)
		gmime_content_field_free (content_field);
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
	gboolean attribute_exists;
	gchar *old_attribute;
	gchar *old_value;
	CAMEL_LOG_FULL_DEBUG ("GMimeContentField:: Entering set_parameter\n");
	CAMEL_LOG_FULL_DEBUG ("GMimeContentField:: set_parameter content_field=%p name=%s, value=%s\n", content_field, attribute, value);
	attribute_exists = g_hash_table_lookup_extended (content_field->parameters, 
							 attribute, 
							 (gpointer *) &old_attribute,
							 (gpointer *) &old_value);
	/** CHECK THAT : is normal to free pointers before insertion ? **/
	if (attribute_exists) {
		g_free (old_value);
		g_free (old_attribute);
	} 
		
	g_hash_table_insert (content_field->parameters, g_strdup (attribute), g_strdup (value));
	CAMEL_LOG_FULL_DEBUG ("GMimeContentField:: Leaving set_parameter\n");
}


/**
 * _print_parameter: print a parameter/value pair to a stream as described in RFC 2045
 * @name: name of the parameter
 * @value: value of the parameter
 * @user_data: CamelStream object to write the text to.
 * 
 * 
 **/
static void
_print_parameter (gpointer name, gpointer value, gpointer user_data)
{
	CamelStream *stream = (CamelStream *)user_data;
	
	camel_stream_write_strings (stream, 
				    "; \n    ", 
				    (gchar *)name, 
				    "=", 
				    (gchar *)value,
				    NULL);
	
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
	if (!content_field) return;

	g_assert (stream);
	if (content_field->type) {
		camel_stream_write_strings (stream, "Content-Type: ", content_field->type, NULL);
		if (content_field->subtype) {
			camel_stream_write_strings (stream, "/", content_field->subtype, NULL);
		}
		/* print all parameters */
		g_hash_table_foreach (content_field->parameters, _print_parameter, stream);
		camel_stream_write_string (stream, "\n");
	} else CAMEL_LOG_FULL_DEBUG ("GMimeContentField::write_to_stream no mime type found\n");
}

/**
 * gmime_content_field_get_mime_type: return the mime type of the content field object
 * @content_field: content field object
 * 
 * A RFC 2045 content type field contains the mime type in the
 * form "type/subtype" (example : "application/postscript") and some
 * parameters (attribute/value pairs). This routine returns the mime type 
 * in a gchar object. 
 * 
 * Return value: the mime type in the form "type/subtype" or NULL if not defined.
 **/
gchar * 
gmime_content_field_get_mime_type (GMimeContentField *content_field)
{
	gchar *mime_type;

	if (!content_field->type) return NULL;

	if (content_field->subtype) 
		mime_type = g_strdup_printf ("%s/%s", content_field->type, content_field->subtype);
	else 
		mime_type = g_strdup (content_field->type);
	return mime_type;
}

static void
___debug_print_parameter (gpointer name, gpointer value, gpointer user_data)
{
	
	printf ("****** parameter \"%s\"=\"%s\"\n", (gchar *)name, (gchar *)value);
	
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
	const gchar *parameter;
	const gchar *old_name;
	gboolean parameter_exists;

	CAMEL_LOG_FULL_DEBUG ("Entering GMimeContentField::get_parameter content_field =%p\n", content_field);
	g_assert (content_field->parameters);
	g_assert (name);
	CAMEL_LOG_FULL_DEBUG ("GMimeContentField::get_parameter looking for parameter \"%s\"\n", name);
	/* parameter = (const gchar *)g_hash_table_lookup (content_field->parameters, name); */
	parameter_exists = g_hash_table_lookup_extended (content_field->parameters, 
							 name, 
							 (gpointer *) &old_name,
							 (gpointer *) &parameter);
	if (!parameter_exists) {
		CAMEL_LOG_FULL_DEBUG ("GMimeContentField::get_parameter, parameter not found\n");
		g_hash_table_foreach (content_field->parameters, ___debug_print_parameter, NULL);
		return NULL;
	}
	CAMEL_LOG_FULL_DEBUG ("Leaving GMimeContentField::get_parameter\n");
	return parameter;
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
	gint first, len;
	gchar *str;
	gint i=0;
	gchar *type, *subtype;
	gchar *param_name, *param_value;
	gboolean param_end;
	
	CAMEL_LOG_TRACE ( "GMimeContentField::construct_from_string, entering\n");
	g_assert (string);
	g_assert (content_field);
 
	g_free (content_field->type);	
	g_free (content_field->subtype);
	
	
	first = 0;
	len = strlen (string);
	if (!len) return;
	CAMEL_LOG_FULL_DEBUG ("GMimeContentField::construct_from_string, All checks done\n");

	/* find the type */
	while ( (i<len) && (!strchr ("/;", string[i])) ) i++;
	
	if (i == 0) return;
	
	type = g_strndup (string, i);
	string_trim (type, " \t", STRING_TRIM_STRIP_TRAILING | STRING_TRIM_STRIP_LEADING);
	content_field->type = type;
	CAMEL_LOG_TRACE ( "GMimeContentField::construct_from_string, Found mime type : \"%s\"\n", type); 
	if (i >= len-1) {
		content_field->subtype = NULL;
		return;
	}
	
	first = i+1;
	/* find the subtype, if any */
	if (string[i++] == '/') {
		while ( (i<len) && (string[i] != ';') ) i++;
		if (i != first) {
			subtype = g_strndup (string+first, i-first);
			string_trim (subtype, " \t", STRING_TRIM_STRIP_TRAILING | STRING_TRIM_STRIP_LEADING);
			content_field->subtype = subtype;
			CAMEL_LOG_TRACE ( "GMimeContentField::construct_from_string, Found mime subtype: \"%s\"\n", subtype);
			if (i >= len-1) return;
		}
 	}
	first = i+1;

	/* parse parameters list */
	param_end = FALSE;
	do {
		while ( (i<len) && (string[i] != '=') ) i++;
		if ((i == len) || (i==first)) param_end = TRUE;
		else {
			/* we have found parameter name */
			param_name = g_strndup (string+first, i-first);
			string_trim (param_name, " ", STRING_TRIM_STRIP_TRAILING | STRING_TRIM_STRIP_LEADING);
			i++;
			first = i;
			/* Let's find parameter value */
			while ( (i<len) && (string[i] != ';') ) i++;
			if (i != first) param_value = g_strndup (string+first, i-first);
			else param_value = g_strdup ("");
			CAMEL_LOG_TRACE ( "GMimeContentField::construct_from_string, Found mime parameter \"%s\"=\"%s\"\n", param_name, param_value);
			string_trim (param_value, " \t", STRING_TRIM_STRIP_TRAILING | STRING_TRIM_STRIP_LEADING);
			gmime_content_field_set_parameter (content_field, param_name, param_value);
			g_free (param_name);
			g_free (param_value);
			i++;
			first = i;
		}
	} while ((!param_end) && (first < len));


}

