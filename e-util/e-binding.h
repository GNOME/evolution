/*
 * e-binding.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is a direct rip-off of Xfce's excellent ExoBinding API,
 * which binds two GObject properties together.  ExoBinding was
 * written by Benedikt Meurer <benny@xfce.org>. */

#ifndef E_BINDING_H
#define E_BINDING_H

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _EBinding EBinding;
typedef struct _EBindingBase EBindingBase;
typedef struct _EBindingLink EBindingLink;
typedef struct _EMutualBinding EMutualBinding;

typedef gboolean	(*EBindingTransform)	(const GValue *src_value,
						 GValue *dst_value,
						 gpointer user_data);

struct _EBindingBase {
	GDestroyNotify destroy;
};

struct _EBindingLink {
	GObject *dst_object;
	GParamSpec *dst_pspec;
	gulong dst_handler; /* only set for mutual bindings */
	gulong handler;
	EBindingTransform transform;
	gpointer user_data;
};

struct _EBinding {
	/*< private >*/
	GObject *src_object;
	EBindingBase base;
	EBindingLink link;
};

struct _EMutualBinding {
	/*< private >*/
	EBindingBase  base;
	EBindingLink  direct;
	EBindingLink  reverse;
};

EBinding *	e_binding_new			(GObject *src_object,
						 const gchar *src_property,
						 GObject *dst_object,
						 const gchar *dst_property);
EBinding *	e_binding_new_full		(GObject *src_object,
						 const gchar *src_property,
						 GObject *dst_object,
						 const gchar *dst_property,
						 EBindingTransform transform,
						 GDestroyNotify destroy_notify,
						 gpointer user_data);
EBinding *	e_binding_new_with_negation	(GObject *src_object,
						 const gchar *src_property,
						 GObject *dst_object,
						 const gchar *dst_property);
void		e_binding_unbind		(EBinding *binding);

EMutualBinding *e_mutual_binding_new		(GObject *object1,
						 const gchar *property1,
						 GObject *object2,
						 const gchar *property2);
EMutualBinding *e_mutual_binding_new_full	(GObject *object1,
						 const gchar *property1,
						 GObject *object2,
						 const gchar *property2,
						 EBindingTransform transform,
						 EBindingTransform reverse_transform,
						 GDestroyNotify destroy_notify,
						 gpointer user_data);
EMutualBinding *e_mutual_binding_new_with_negation
						(GObject *object1,
						 const gchar *property1,
						 GObject *object2,
						 const gchar *property2);
void		e_mutual_binding_unbind		(EMutualBinding *binding);

/* Useful transformation functions */
gboolean	e_binding_transform_color_to_string
						(const GValue *src_value,
						 GValue *dst_value,
						 gpointer user_data);
gboolean	e_binding_transform_string_to_color
						(const GValue *src_value,
						 GValue *dst_value,
						 gpointer user_data);

G_END_DECLS

#endif /* E_BINDING_H */
