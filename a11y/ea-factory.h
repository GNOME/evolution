/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-factory.h
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
 *
 */

/* Evolution Accessibility
*/

#ifndef _EA_FACTORY_H__
#define _EA_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobject.h>

#define EA_FACTORY_PARTA_GOBJECT(type, type_as_function, opt_create_accessible)	\
static AtkObject*								\
type_as_function ## _factory_create_accessible (GObject *obj)			\
{                                                                               \
  AtkObject *accessible;							\
  g_return_val_if_fail (G_IS_OBJECT (obj), NULL);				\
  accessible = opt_create_accessible (G_OBJECT (obj));	                 	\
  return accessible;	                                                        \
}

#define EA_FACTORY_PARTA(type, type_as_function, opt_create_accessible)	        \
static AtkObject*								\
type_as_function ## _factory_create_accessible (GObject *obj)			\
{                                                                               \
  GtkWidget *widget;								\
  AtkObject *accessible;							\
                                                                                \
  g_return_val_if_fail (GTK_IS_WIDGET (obj), NULL);				\
										\
  widget = GTK_WIDGET (obj);							\
										\
  accessible = opt_create_accessible (widget);					\
  return accessible;	                                                        \
}

#define EA_FACTORY_PARTB(type, type_as_function, opt_create_accessible)	        \
										\
static GType									\
type_as_function ## _factory_get_accessible_type (void)				\
{										\
  return type;									\
}										\
										\
										\
static void									\
type_as_function ## _factory_class_init (AtkObjectFactoryClass *klass)		\
{										\
  klass->create_accessible   = type_as_function ## _factory_create_accessible;	\
  klass->get_accessible_type = type_as_function ## _factory_get_accessible_type;\
}										\
										\
static GType									\
type_as_function ## _factory_get_type (void)					\
{										\
  static GType t = 0;								\
										\
  if (!t)									\
  {										\
    char *name;									\
    static const GTypeInfo tinfo =						\
    {										\
      sizeof (AtkObjectFactoryClass),				         	\
      NULL, NULL, (GClassInitFunc) type_as_function ## _factory_class_init,	\
      NULL, NULL, sizeof (AtkObjectFactory), 0, NULL, NULL			\
    };										\
										\
    name = g_strconcat (g_type_name (type), "Factory", NULL);			\
    t = g_type_register_static (						\
	    ATK_TYPE_OBJECT_FACTORY, name, &tinfo, 0);				\
    g_free (name);								\
  }										\
										\
  return t;									\
}

#define EA_FACTORY(type, type_as_function, opt_create_accessible)	        \
        EA_FACTORY_PARTA(type, type_as_function, opt_create_accessible)	        \
        EA_FACTORY_PARTB(type, type_as_function, opt_create_accessible)

#define EA_FACTORY_GOBJECT(type, type_as_function, opt_create_accessible)	\
        EA_FACTORY_PARTA_GOBJECT(type, type_as_function, opt_create_accessible)	\
        EA_FACTORY_PARTB(type, type_as_function, opt_create_accessible)

#define EA_SET_FACTORY(obj_type, type_as_function)		         	\
	atk_registry_set_factory_type (atk_get_default_registry (),		\
				       obj_type,				\
				       type_as_function ## _factory_get_type ())

#endif /* _EA_FACTORY_H__ */
