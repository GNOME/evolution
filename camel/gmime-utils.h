/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mime-utils.h : misc utilities for mime  */

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


#ifndef GMIME_UTILS_H
#define GMIME_UTILS_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include <stdio.h>
#include <camel/camel-stream.h>

typedef struct 
{
	gchar *name;
	gchar *value;	

} Rfc822Header;


void gmime_write_header_pair_to_stream (CamelStream *stream, 
					const gchar* name, 
					const gchar *value);

void gmime_write_header_table_to_stream (CamelStream *stream, 
					 GHashTable *header_table);

void gmime_write_header_with_glist_to_stream (CamelStream *stream, 
					      const gchar *header_name, 
					      GList *header_values, 
					      const gchar *separator);

GArray *get_header_array_from_stream (CamelStream *stream);
gchar *gmime_read_line_from_stream (CamelStream *stream);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GMIME_UTILS_H */
