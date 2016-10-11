/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

/* Evolution Accessibility
*/

#ifndef _EA_FACTORY_H__
#define _EA_FACTORY_H__

#include <atk/atkobject.h>

#define EA_FACTORY_PARTA_GOBJECT(type, type_as_function, opt_create_accessible) \
static AtkObject * \
type_as_function ## _factory_create_accessible (GObject *obj) \
{ \
  AtkObject *accessible; \
  g_return_val_if_fail (G_IS_OBJECT (obj), NULL); \
  accessible = opt_create_accessible (G_OBJECT (obj)); \
  return accessible; \
}

#define EA_FACTORY_PARTA(type, type_as_function, opt_create_accessible) \
static AtkObject * \
type_as_function ## _factory_create_accessible (GObject *obj) \
{ \
  GtkWidget *widget; \
  AtkObject *accessible; \
 \
  g_return_val_if_fail (GTK_IS_WIDGET (obj), NULL); \
 \
  widget = GTK_WIDGET (obj); \
 \
  accessible = opt_create_accessible (widget); \
  return accessible; \
}

#define EA_FACTORY_PARTB(type, type_as_function, opt_create_accessible) \
 \
static GType \
type_as_function ## _factory_get_accessible_type (void) \
{ \
  return type; \
} \
 \
 \
static void \
type_as_function ## _factory_class_init (AtkObjectFactoryClass *klass) \
{ \
  klass->create_accessible = type_as_function ## _factory_create_accessible; \
  klass->get_accessible_type = type_as_function ## _factory_get_accessible_type;\
} \
 \
static GType \
type_as_function ## _factory_get_type (void) \
{ \
  static GType t = 0; \
 \
  if (!t) \
  { \
    gchar *name; \
    static const GTypeInfo tinfo = \
    { \
      sizeof (AtkObjectFactoryClass), \
      NULL, NULL, (GClassInitFunc) type_as_function ## _factory_class_init, \
      NULL, NULL, sizeof (AtkObjectFactory), 0, NULL, NULL \
    }; \
 \
    name = g_strconcat (g_type_name (type), "Factory", NULL); \
    t = g_type_register_static ( \
	    ATK_TYPE_OBJECT_FACTORY, name, &tinfo, 0); \
    g_free (name); \
  } \
 \
  return t; \
}

#define EA_FACTORY(type, type_as_function, opt_create_accessible) \
        EA_FACTORY_PARTA (type, type_as_function, opt_create_accessible) \
        EA_FACTORY_PARTB (type, type_as_function, opt_create_accessible)

#define EA_FACTORY_GOBJECT(type, type_as_function, opt_create_accessible) \
        EA_FACTORY_PARTA_GOBJECT (type, type_as_function, opt_create_accessible) \
        EA_FACTORY_PARTB (type, type_as_function, opt_create_accessible)

#define EA_SET_FACTORY(obj_type, type_as_function) \
{ \
	if (atk_get_root ()) { \
		atk_registry_set_factory_type (atk_get_default_registry (), \
				      obj_type, \
				      type_as_function ## _factory_get_type ());\
	} \
}

#endif /* _EA_FACTORY_H__ */
