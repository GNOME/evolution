/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mime-utils.c : misc utilities for mime  */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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
#include "gmime-utils.h"
#include "string-utils.h"
#include "camel-log.h"
#include "camel-stream.h"

void
gmime_write_header_pair_to_stream (CamelStream *stream, const gchar* name, const gchar *value)
{

	gchar *strtmp;

	CAMEL_LOG_FULL_DEBUG ( "gmime_write_header_pair_to_stream:: Entering\n");
	g_assert(name);

	if (!value) return; 
	strtmp = g_strdup_printf ("%s: %s\n", name, value);
	
	camel_stream_write_string (stream, strtmp);
	CAMEL_LOG_FULL_DEBUG ( "gmime_write_header_pair_to_stream:\n  writing %s\n", strtmp);
	
	g_free (strtmp);
	CAMEL_LOG_FULL_DEBUG ( "gmime_write_header_pair_to_stream:: Leaving\n");

}


static void
_write_one_header_to_stream (gpointer key, gpointer value, gpointer user_data)
{
	gchar *header_name = (gchar *)key;
	gchar *header_value = (gchar *)value;
	CamelStream *stream = (CamelStream *)user_data;

	CAMEL_LOG_FULL_DEBUG ( "_write_one_header_to_stream:: Entering\n");
	if ((header_name) && (header_value))
		gmime_write_header_pair_to_stream (stream, header_name, header_value);		
	CAMEL_LOG_FULL_DEBUG ( "_write_one_header_to_stream:: Leaving\n");
}

void 
gmime_write_header_table_to_stream (CamelStream *stream, GHashTable *header_table)
{
	CAMEL_LOG_FULL_DEBUG ( "write_header_table_to_stream:: Entering\n");
	g_hash_table_foreach (header_table, 
			      _write_one_header_to_stream, 
			      (gpointer)stream);
	CAMEL_LOG_FULL_DEBUG ( "write_header_table_to_stream:: Leaving\n");
}


void 
gmime_write_header_with_glist_to_stream (CamelStream *stream, 
					 const gchar *header_name, 
					 GList *header_values, 
					 const gchar *separator)
{
	
	gchar *current;

	CAMEL_LOG_FULL_DEBUG ( "write_header_with_glist_to_stream:: entering\n");
	if ( (header_name) && (header_values) )
		{
			gboolean first;
			
			camel_stream_write (stream, header_name, strlen (header_name) );
			camel_stream_write (stream, ": ", 2);
			first = TRUE;
			while (header_values) {
				current = (gchar *)header_values->data;
				if (current) {
					if (!first) camel_stream_write_string (stream, separator);
					else first = FALSE;
					camel_stream_write (stream, current, strlen (current));
				}
				header_values = g_list_next (header_values);
			}
			camel_stream_write (stream, "\n", 1);
		}
	CAMEL_LOG_FULL_DEBUG ("write_header_with_glist_to_stream:: leaving\n");
	
}	





/* * * * * * * * * * * */
/* scanning functions  */

static void
_store_header_pair_from_string (GArray *header_array, gchar *header_line)
{
	gchar dich_result;
	gchar *header_name, *header_value;
	Rfc822Header header;

	CAMEL_LOG_FULL_DEBUG ( "_store_header_pair_from_string:: Entering\n");

	g_assert (header_array);
	g_return_if_fail (header_line);


	if (header_line) {
		dich_result = string_dichotomy ( header_line, ':', 
						   &header_name, &header_value,
						   STRING_DICHOTOMY_NONE);
		if (dich_result != 'o') {
			CAMEL_LOG_WARNING (
					   "** WARNING **\n"
					   "store_header_pair_from_string : dichotomy result is '%c'\n"
					   "header line is :\n--\n%s\n--\n"
					   "** \n", dich_result, header_line);	
			if (header_name) 
				g_free (header_name);
			if (header_value) 
				g_free (header_value);
			
		} else {
			string_trim (header_value, " \t",
				     STRING_TRIM_STRIP_LEADING | STRING_TRIM_STRIP_TRAILING);

			header.name = header_name;
			header.value = header_value;
			g_array_append_val (header_array, header);
		}
	}

	CAMEL_LOG_FULL_DEBUG ( "_store_header_pair_from_string:: Leaving\n");
	
}


	
		
GArray *
get_header_array_from_stream (CamelStream *stream)
{
#warning Correct Lazy Implementation 
	/* should not use GString. */
	/* should read the header line by line */
	/* and not char by char */
	gchar next_char;
	gint nb_char_read;

	gboolean crlf = FALSE;
	gboolean end_of_header_line = FALSE;
	gboolean end_of_headers = FALSE;
	gboolean end_of_file = FALSE;

	GString *header_line=NULL;
	gchar *str_header_line;

	GArray *header_array;


	CAMEL_LOG_FULL_DEBUG ( "gmime-utils:: Entering get_header_table_from_stream\n");
	header_array = g_array_new (FALSE, FALSE, sizeof (Rfc822Header));

	nb_char_read = camel_stream_read (stream, &next_char, 1);
	do {
		header_line = g_string_new ("");
		end_of_header_line = FALSE;
		crlf = FALSE;
		
		/* read a whole header line */
		do {
			if (nb_char_read>0) {
				switch (next_char) {
					
				case '\n': /* a blank line means end of headers */
					if (crlf) {
						end_of_headers=TRUE;
						end_of_header_line = TRUE;
					}
					else crlf = TRUE;
					break;
				case ' ':
				case '\t':
					if (crlf) {
						crlf = FALSE; 
						next_char = ' ';
					}
					
				default:
					if (!crlf) header_line = g_string_append_c (header_line, next_char);					
					else end_of_header_line = TRUE;
				}
			} else {
				end_of_file=TRUE;
				end_of_header_line = TRUE;
			}
			/* if we have read a whole header line, we have also read
			   the first character of the next line to be sure the 
			   crlf was not followed by a space or a tab char */
			if (!end_of_header_line) nb_char_read = camel_stream_read (stream, &next_char, 1);

		} while ( !end_of_header_line );
		if ( strlen(header_line->str) ) {
			/*  str_header_line = g_strdup (header_line->str); */
			_store_header_pair_from_string (header_array, header_line->str);			
		}
		g_string_free (header_line, FALSE);

	} while ( (!end_of_headers) && (!end_of_file) );
	
	CAMEL_LOG_FULL_DEBUG ( "gmime-utils:: Leaving get_header_table_from_stream\n");
	return header_array;
}
		
		
gchar *
gmime_read_line_from_stream (CamelStream *stream)
{
	GString *new_line;
	gchar *result;
	gchar next_char;
	gboolean end_of_line = FALSE;
	gboolean end_of_stream = FALSE;
	gint nb_char_read;

	new_line = g_string_new ("");
	do {
		nb_char_read = camel_stream_read (stream, &next_char, 1);
		if (nb_char_read>0) {
			switch (next_char) {
			case '\n':				
				end_of_line = TRUE;
				/*  g_string_append_c (new_line, next_char); */
				break;
			default:
				g_string_append_c (new_line, next_char);
			}
		} else end_of_stream = TRUE;
	} while (!end_of_line && !end_of_stream);

	if (!end_of_stream)
		result = g_strdup (new_line->str);
	else result=NULL;
	g_string_free (new_line, TRUE);
	return result;
}
