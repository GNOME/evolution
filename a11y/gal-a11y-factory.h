/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Gilbert Fang <gilbert.fang@sun.com>
 *
 * This file is mainly from the gailfactory.h of GAIL.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _GAL_A11Y_FACTORY_H__
#define _GAL_A11Y_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobject.h>
#include <atk/atkobjectfactory.h>

#define GAL_A11Y_FACTORY(type, type_as_function, opt_create_accessible)	\
										\
static GType									\
type_as_function ## _factory_get_accessible_type (void)				\
{										\
  return type;									\
}										\
										\
static AtkObject*								\
type_as_function ## _factory_create_accessible (GObject *obj)			\
{										\
  GtkWidget *widget;								\
  AtkObject *accessible;							\
										\
  g_return_val_if_fail (GTK_IS_WIDGET (obj), NULL);				\
										\
  widget = GTK_WIDGET (obj);							\
										\
  accessible = opt_create_accessible (widget);					\
										\
  return accessible;								\
}										\
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
    gchar *name;									\
    static const GTypeInfo tinfo =						\
    {										\
      sizeof (AtkObjectFactoryClass),					\
      NULL, NULL, (GClassInitFunc) type_as_function ## _factory_class_init,			\
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

#endif /* _GAL_A11Y_FACTORY_H__ */
