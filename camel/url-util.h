/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* url-util.h : utility functions to parse URLs */

/* 
 * This code is adapted form gzillaurl.h (http://www.gzilla.com)
 * Copyright (C) Raph Levien <raph@acm.org>
 *
 * Modifications by Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr>
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


#ifndef URL_UTIL_H
#define URL_UTIL_H 1

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


gboolean g_url_is_absolute (const char *url);
gboolean g_url_match_method (const char *url, const char *method);
gboolean g_url_relative (const char *base_url,
			  const char *relative_url,
			  char *new_url,
			  gint size_new_url);
char *g_url_parse (char *url,
			char *hostname,
			gint hostname_size,
			int *port);
void g_url_parse_hash (char **p_head, char **p_tail, const char *url);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* URL_UTIL_H */
