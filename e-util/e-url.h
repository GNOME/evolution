/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-url.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 *              Rodrigo Moya   <rodrigo@ximian.com>
 */

/*
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
 * USA.
 */

#ifndef __E_URL_H__
#define __E_URL_H__

#include <gtk/gtk.h>

char *e_url_shroud (const char *url);
gboolean e_url_equal (const char *url1, const char *url2);

typedef struct {
	char  *protocol;
	char  *user;
	char  *authmech;
	char  *passwd;
	char  *host;
	int    port;
	char  *path;
	GData *params;
	char  *query;
	char  *fragment;
} EUri;

EUri       *e_uri_new       (const char *uri_string);
void        e_uri_free      (EUri *uri);
const char *e_uri_get_param (EUri *uri, const char *name);
EUri       *e_uri_copy      (EUri *uri);
char       *e_uri_to_string (EUri *uri, gboolean show_password);

#endif /* __E_URL_H__ */

