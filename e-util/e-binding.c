/*
 * e-binding.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-binding.h"

static gboolean
e_binding_transform_negate (const GValue *src_value,
                            GValue *dst_value)
{
	if (!g_value_transform (src_value, dst_value))
		return FALSE;

	g_value_set_boolean (dst_value, !g_value_get_boolean (dst_value));

	return TRUE;
}

static void
e_bind_properties_transfer (GObject *src_object,
                            GParamSpec *src_pspec,
                            GObject *dst_object,
                            GParamSpec *dst_pspec,
                            EBindingTransform  transform,
                            gpointer user_data)
{
	const gchar *src_name;
	const gchar *dst_name;
	gboolean result;
	GValue src_value = { 0, };
	GValue dst_value = { 0, };

	src_name = g_param_spec_get_name (src_pspec);
	dst_name = g_param_spec_get_name (dst_pspec);

	g_value_init (&src_value, G_PARAM_SPEC_VALUE_TYPE (src_pspec));
	g_object_get_property (src_object, src_name, &src_value);

	g_value_init (&dst_value, G_PARAM_SPEC_VALUE_TYPE (dst_pspec));
	result = (*transform) (&src_value, &dst_value, user_data);

	g_value_unset (&src_value);

	g_return_if_fail (result);

	g_param_value_validate (dst_pspec, &dst_value);
	g_object_set_property (dst_object, dst_name, &dst_value);
	g_value_unset (&dst_value);
}

static void
e_bind_properties_notify (GObject *src_object,
                          GParamSpec *src_pspec,
                          gpointer data)
{
	EBindingLink *link = data;

	/* Block the destination handler for mutual bindings,
	 * so we don't recurse here. */
	if (link->dst_handler != 0)
		g_signal_handler_block (link->dst_object, link->dst_handler);

	e_bind_properties_transfer (
		src_object, src_pspec,
		link->dst_object, link->dst_pspec,
		link->transform, link->user_data);

	/* Unblock destination handler. */
	if (link->dst_handler != 0)
		g_signal_handler_unblock (link->dst_object, link->dst_handler);
}

static void
e_binding_on_dst_object_destroy (gpointer  data,
                                 GObject  *object)
{
	EBinding *binding = data;

	binding->link.dst_object = NULL;

	/* Calls e_binding_on_disconnect() */
	g_signal_handler_disconnect (
		binding->src_object, binding->link.handler);
}

static void
e_binding_on_disconnect (gpointer data,
                         GClosure *closure)
{
	EBindingLink *link = data;
	EBinding *binding;

	binding = (EBinding *)
		(((gchar *) link) - G_STRUCT_OFFSET (EBinding, link));

	if (binding->base.destroy != NULL)
		binding->base.destroy (link->user_data);

	if (link->dst_object != NULL)
		g_object_weak_unref (
			link->dst_object,
			e_binding_on_dst_object_destroy, binding);

	g_slice_free (EBinding, binding);
}

/* Recursively calls e_mutual_binding_on_disconnect_object2() */
static void
e_mutual_binding_on_disconnect_object1 (gpointer data,
                                        GClosure *closure)
{
	EMutualBinding *binding;
	EBindingLink *link = data;
	GObject *object2;

	binding = (EMutualBinding *)
		(((gchar *) link) - G_STRUCT_OFFSET (EMutualBinding, direct));
	binding->reverse.dst_object = NULL;

	object2 = binding->direct.dst_object;
	if (object2 != NULL) {
		if (binding->base.destroy != NULL)
			binding->base.destroy (binding->direct.user_data);
		binding->direct.dst_object = NULL;
		g_signal_handler_disconnect (object2, binding->reverse.handler);
		g_slice_free (EMutualBinding, binding);
	}
}

/* Recursively calls e_mutual_binding_on_disconnect_object1() */
static void
e_mutual_binding_on_disconnect_object2 (gpointer data,
                                        GClosure *closure)
{
	EMutualBinding *binding;
	EBindingLink *link = data;
	GObject *object1;

	binding = (EMutualBinding *)
		(((gchar *) link) - G_STRUCT_OFFSET (EMutualBinding, reverse));
	binding->direct.dst_object = NULL;

	object1 = binding->reverse.dst_object;
	if (object1 != NULL) {
		binding->reverse.dst_object = NULL;
		g_signal_handler_disconnect (object1, binding->direct.handler);
	}
}

static void
e_binding_link_init (EBindingLink *link,
                     GObject *src_object,
                     const gchar *src_property,
                     GObject *dst_object,
                     GParamSpec *dst_pspec,
                     EBindingTransform transform,
                     GClosureNotify destroy_notify,
                     gpointer user_data)
{
	gchar *signal_name;

	link->dst_object = dst_object;
	link->dst_pspec = dst_pspec;
	link->dst_handler = 0;
	link->transform = transform;
	link->user_data = user_data;

	signal_name = g_strconcat ("notify::", src_property, NULL);
	link->handler = g_signal_connect_data (
		src_object, signal_name,
		G_CALLBACK (e_bind_properties_notify),
		link, destroy_notify, 0);
	g_free (signal_name);
}

/**
 * e_binding_new:
 * @src_object: The source #GObject.
 * @src_property: The name of the property to bind from.
 * @dst_object: The destination #GObject.
 * @dst_property: The name of the property to bind to.
 *
 * One-way binds @src_property in @src_object to @dst_property
 * in @dst_object.
 *
 * Before binding the value of @dst_property is set to the
 * value of @src_property.
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
EBinding *
e_binding_new (GObject *src_object,
               const gchar *src_property,
               GObject *dst_object,
               const gchar *dst_property)
{
	return e_binding_new_full (
		src_object, src_property,
		dst_object, dst_property,
		NULL, NULL, NULL);
}

/**
 * e_binding_new_full:
 * @src_object: The source #GObject.
 * @src_property: The name of the property to bind from.
 * @dst_object: The destination #GObject.
 * @dst_property: The name of the property to bind to.
 * @transform: Transformation function or %NULL.
 * @destroy_notify: Callback function that is called on
 *                  disconnection with @user_data or %NULL.
 * @user_data: User data associated with the binding.
 *
 * One-way binds @src_property in @src_object to @dst_property
 * in @dst_object.
 *
 * Before binding the value of @dst_property is set to the
 * value of @src_property.
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
EBinding *
e_binding_new_full (GObject *src_object,
                    const gchar *src_property,
                    GObject *dst_object,
                    const gchar *dst_property,
                    EBindingTransform transform,
                    GDestroyNotify destroy_notify,
                    gpointer user_data)
{
	EBinding *binding;
	GParamSpec *src_pspec;
	GParamSpec *dst_pspec;

	g_return_val_if_fail (G_IS_OBJECT (src_object), NULL);
	g_return_val_if_fail (G_IS_OBJECT (dst_object), NULL);

	src_pspec = g_object_class_find_property (
		G_OBJECT_GET_CLASS (src_object), src_property);
	dst_pspec = g_object_class_find_property (
		G_OBJECT_GET_CLASS (dst_object), dst_property);

	if (transform == NULL)
		transform = (EBindingTransform) g_value_transform;

	e_bind_properties_transfer (
		src_object, src_pspec,
		dst_object, dst_pspec,
		transform, user_data);

	binding = g_slice_new (EBinding);
	binding->src_object = src_object;
	binding->base.destroy = destroy_notify;

	e_binding_link_init (
		&binding->link, src_object, src_property, dst_object,
		dst_pspec, transform, e_binding_on_disconnect, user_data);

	g_object_weak_ref (
		dst_object, e_binding_on_dst_object_destroy, binding);

	return binding;
}

/**
 * e_binding_new_with_negation:
 * @src_object: The source #GObject.
 * @src_property: The name of the property to bind from.
 * @dst_object: The destination #GObject.
 * @dst_property: The name of the property to bind to.
 *
 * Convenience function for binding with boolean negation of value.
 *
 * Return: The descriptor of the binding. It is automatically
 *         removed if one of the objects is finalized.
 **/
EBinding *
e_binding_new_with_negation (GObject *src_object,
                             const gchar *src_property,
                             GObject *dst_object,
                             const gchar *dst_property)
{
	EBindingTransform transform;

	transform = (EBindingTransform) e_binding_transform_negate;

	return e_binding_new_full (
		src_object, src_property,
		dst_object, dst_property,
		transform, NULL, NULL);
}

/**
 * e_binding_unbind:
 * @binding: An #EBinding to unbind.
 *
 * Disconnects the binding between two properties. Should be
 * rarely used by applications.
 *
 * This functions also calls the @destroy_notify function that
 * was specified when @binding was created.
 **/
void
e_binding_unbind (EBinding *binding)
{
	g_signal_handler_disconnect (
		binding->src_object, binding->link.handler);
}

/**
 * e_mutual_binding_new:
 * @object1 : The first #GObject.
 * @property1: The first property to bind.
 * @object2 : The second #GObject.
 * @property2: The second property to bind.
 *
 * Mutually binds values of two properties.
 *
 * Before binding the value of @property2 is set to the value
 * of @property1.
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
EMutualBinding *
e_mutual_binding_new (GObject *object1,
                      const gchar *property1,
                      GObject *object2,
                      const gchar *property2)
{
	return e_mutual_binding_new_full (
		object1, property1,
		object2, property2,
		NULL, NULL, NULL, NULL);
}

/**
 * e_mutual_binding_new_full:
 * @object1: The first #GObject.
 * @property1: The first property to bind.
 * @object2: The second #GObject.
 * @property2: The second property to bind.
 * @transform: Transformation function or %NULL.
 * @reverse_transform: The inverse transformation function or %NULL.
 * @destroy_notify: Callback function called on disconnection with
 *                  @user_data as argument or %NULL.
 * @user_data: User data associated with the binding.
 *
 * Mutually binds values of two properties.
 *
 * Before binding the value of @property2 is set to the value of
 * @property1.
 *
 * Both @transform and @reverse_transform should simultaneously be
 * %NULL or non-%NULL. If they are non-%NULL, they should be reverse
 * in each other.
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
EMutualBinding *
e_mutual_binding_new_full (GObject *object1,
                           const gchar *property1,
                           GObject *object2,
                           const gchar *property2,
                           EBindingTransform transform,
                           EBindingTransform reverse_transform,
                           GDestroyNotify destroy_notify,
                           gpointer user_data)
{
	EMutualBinding *binding;
	GParamSpec *pspec1;
	GParamSpec *pspec2;

	g_return_val_if_fail (G_IS_OBJECT (object1), NULL);
	g_return_val_if_fail (G_IS_OBJECT (object2), NULL);

	pspec1 = g_object_class_find_property (
		G_OBJECT_GET_CLASS (object1), property1);
	pspec2 = g_object_class_find_property (
		G_OBJECT_GET_CLASS (object2), property2);

	if (transform == NULL)
		transform = (EBindingTransform) g_value_transform;

	if (reverse_transform == NULL)
		reverse_transform = (EBindingTransform) g_value_transform;

	e_bind_properties_transfer (
		object1, pspec1, object2,
		pspec2, transform, user_data);

	binding = g_slice_new (EMutualBinding);
	binding->base.destroy = destroy_notify;

	e_binding_link_init (
		&binding->direct,
		object1, property1, object2, pspec2, transform,
		e_mutual_binding_on_disconnect_object1, user_data);

	e_binding_link_init (
		&binding->reverse,
		object2, property2, object1, pspec1, reverse_transform,
		e_mutual_binding_on_disconnect_object2, user_data);

	/* Tell each link about the reverse link for mutual bindings,
	 * to make sure that we do not ever recurse in notify (yeah,
	 * the GObject notify dispatching is really weird!). */
	binding->direct.dst_handler = binding->reverse.handler;
	binding->reverse.dst_handler = binding->direct.handler;

	return binding;
}

/**
 * e_mutual_binding_new_with_negation:
 * @object1: The first #GObject.
 * @property1: The first property to bind.
 * @object2: The second #GObject.
 * @property2: The second property to bind.
 *
 * Convenience function for binding with boolean negation of value.
 *
 * Returns: The descriptor of the binding. It is automatically removed
 *          if one of the objects if finalized.
 **/
EMutualBinding*
e_mutual_binding_new_with_negation (GObject *object1,
                                    const gchar *property1,
                                    GObject *object2,
                                    const gchar *property2)
{
	EBindingTransform transform;

	transform = (EBindingTransform) e_binding_transform_negate;

	return e_mutual_binding_new_full (
		object1, property1,
		object2, property2,
		transform, transform,
		NULL, NULL);
}

/**
 * e_mutual_binding_unbind:
 * @binding: An #EMutualBinding to unbind.
 *
 * Disconnects the binding between two properties. Should be
 * rarely used by applications.
 *
 * This functions also calls the @destroy_notify function that
 * was specified when @binding was created.
 **/
void
e_mutual_binding_unbind (EMutualBinding *binding)
{
	g_signal_handler_disconnect (
		binding->reverse.dst_object, binding->direct.handler);
}

/**
 * e_binding_transform_color_to_string:
 * @src_value: a #GValue of type #GDK_TYPE_COLOR
 * @dst_value: a #GValue of type #G_TYPE_STRING
 * @user_data: not used
 *
 * Transforms a #GdkColor value to a color string specification.
 *
 * Returns: %TRUE always
 **/
gboolean
e_binding_transform_color_to_string (const GValue *src_value,
                                     GValue *dst_value,
                                     gpointer user_data)
{
	const GdkColor *color;
	gchar *string;

	color = g_value_get_boxed (src_value);
	string = gdk_color_to_string (color);
	g_value_set_string (dst_value, string);
	g_free (string);

	return TRUE;
}

/**
 * e_binding_transform_string_to_color:
 * @src_value: a #GValue of type #G_TYPE_STRING
 * @dst_value: a #GValue of type #GDK_TYPE_COLOR
 * @user_data: not used
 *
 * Transforms a color string specification to a #GdkColor.
 *
 * Returns: %TRUE if color string specification was valid
 **/
gboolean
e_binding_transform_string_to_color (const GValue *src_value,
                                     GValue *dst_value,
                                     gpointer user_data)
{
	GdkColor color;
	const gchar *string;
	gboolean success = FALSE;

	string = g_value_get_string (src_value);
	if (gdk_color_parse (string, &color)) {
		g_value_set_boxed (dst_value, &color);
		success = TRUE;
	}

	return success;
}
