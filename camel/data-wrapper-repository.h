/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org>
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


#ifndef DATA_WRAPPER_REPOSITORY_H
#define DATA_WRAPPER_REPOSITORY_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include "camel-data-wrapper.h"



typedef struct {
	GHashTable *mime_links;
} DataWrapperRepository;


gint data_wrapper_repository_init ();
void data_wrapper_repository_set_data_wrapper_type (const gchar *mime_type, GtkType object_type);
GtkType data_wrapper_repository_get_data_wrapper_type (const gchar *mime_type);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DATA_WRAPPER_REPOSITORY_H */
