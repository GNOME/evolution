/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-object.h: Base class for Camel */

/*
 * Author:
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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

#ifndef CAMEL_OBJECT_H
#define CAMEL_OBJECT_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include <camel/camel-types.h>

#define CAMEL_OBJECT_TYPE     (camel_object_get_type ())
#define CAMEL_OBJECT(obj)     (GTK_CHECK_CAST((obj), CAMEL_OBJECT_TYPE, CamelObject))
#define CAMEL_OBJECT_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_OBJECT_TYPE, CamelObjectClass))
#define CAMEL_IS_OBJECT(o)    (GTK_CHECK_TYPE((o), CAMEL_OBJECT_TYPE))


struct _CamelObject
{
	GtkObject parent_object;

};


typedef struct {
	GtkObjectClass parent_class;

} CamelObjectClass;


/* Standard Gtk function */
GtkType camel_object_get_type (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_OBJECT_H */
