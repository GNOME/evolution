/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mime-content_field.h : mime content type field utilities  */

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


#ifndef GMIME_CONTENT_FIELD_H
#define GMIME_CONTENT_FIELD_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include <stdio.h>
#include "camel-stream.h"

typedef struct {

	GString *type;
	GString *subtype;
	GHashTable *parameters;

} GMimeContentField;

GMimeContentField *gmime_content_field_new (GString *type, GString *subtype);
void gmime_content_field_set_parameter(GMimeContentField *content_field, GString *attribute, GString *value);
void gmime_content_field_write_to_stream (GMimeContentField *content_field, CamelStream *stream);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GMIME_CONTENT_FIELD_H */
