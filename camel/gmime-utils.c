/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mime-utils.c : misc utilities for mime  */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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



#include "gmime-utils.h"
#include "gstring-util.h"
#include "camel-log.h"
#include "camel-stream.h"

void
gmime_write_header_pair_to_stream (CamelStream *stream, gchar* name, GString *value)
{

	GString *strtmp;
	guint len;

	g_assert(name);

	if (!value || !(value->str)) return;
	len = strlen (name) + strlen (value->str) +3;
	/* 3 is for ": " and "\n" */
	strtmp = g_string_sized_new (len);
	
	sprintf(strtmp->str, "%s: %s\n", name, value->str);
	camel_stream_write (stream, strtmp->str, len);
	CAMEL_LOG (FULL_DEBUG, "gmime_write_header_pair_to_stream:\n  writing %s\n", strtmp->str);
	g_string_free (strtmp, FALSE);
}


static void
_write_one_header_to_stream (gpointer key, gpointer value, gpointer user_data)
{
	GString *header_name = (GString *)key;
	GString *header_value = (GString *)value;
	CamelStream *stream = (CamelStream *)user_data;

	if ( (header_name) && (header_name->str) && 
	     (header_value) && (header_value->str) )
		gmime_write_header_pair_to_stream (stream, header_name->str, header_value);		
}

void 
write_header_table_to_stream (CamelStream *stream, GHashTable *header_table)
{
	g_hash_table_foreach (header_table, 
			      _write_one_header_to_stream, 
			      (gpointer)stream);
}


void 
write_header_with_glist_to_stream (CamelStream *stream, gchar *header_name, GList *header_values)
{
	
	GString *current;
	
	if ( (header_name) && (header_values) )
		{
			gboolean first;
			
			camel_stream_write (stream, header_name, strlen (header_name) );
			camel_stream_write (stream, ": ", 2);
			first = TRUE;
			while (header_values) {
				current = (GString *)header_values->data;
				if ( (current) && (current->str) ) {
					if (!first) camel_stream_write (stream, ", ", 2);
					else first = FALSE;
					camel_stream_write (stream, current->str, strlen (current->str));
				}
				header_values = g_list_next(header_values);
			}
			camel_stream_write (stream, "\n", 1);
		}
	
}	





/* * * * * * * * * * * */
/* scanning functions  */

static void
_store_header_pair_from_gstring (GHashTable *header_table, GString *header_line)
{
	gchar dich_result;
	GString *header_name, *header_value;
	
	g_assert (header_table);
	if ( (header_line) && (header_line->str) ) {
		dich_result = g_string_dichotomy(header_line, ':', &header_name, &header_value, DICHOTOMY_NONE);
		if (dich_result != 'o')
			camel_log(WARNING, 
				  "store_header_pair_from_gstring : dichotomy result is %c"
				  "header line is :\n--\n%s\n--\n");
		
		else {
			g_string_trim (header_value, " \t", TRIM_STRIP_LEADING | TRIM_STRIP_TRAILING);
			g_hash_table_insert (header_table, header_name, header_value);
		}
	}
		
}


	
		
GHashTable *
get_header_table_from_stream (CamelStream *stream)
{
	gchar next_char;

	gboolean crlf = FALSE;
	gboolean end_of_header_line = FALSE;
	gboolean end_of_headers = FALSE;
	gboolean end_of_file = FALSE;
	GString *header_line=NULL;
	GHashTable *header_table;

	header_table = g_hash_table_new (g_string_hash, g_string_equal_for_hash);
	camel_stream_read (stream, &next_char, 1);
	do {
		header_line = g_string_new("");
		end_of_header_line = FALSE;
		crlf = FALSE;
		
		/* read a whole header line */
		do {
			switch (next_char) {
			case -1:
				end_of_file=TRUE;
				end_of_header_line = TRUE;
				break;
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
			/* if we have read a whole header line, we have also read
			   the first character of the next line to be sure the 
			   crlf was not followed by a space or a tab char */
			if (!end_of_header_line) camel_stream_read (stream, &next_char, 1);

		} while ( !end_of_header_line );
		if ( strlen(header_line->str) ) 
			_store_header_pair_from_gstring (header_table, header_line);
		g_string_free (header_line, FALSE);

	} while ( (!end_of_headers) && (!end_of_file) );

	return header_table;
}
		
		
