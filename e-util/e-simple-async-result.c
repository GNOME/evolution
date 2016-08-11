/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include "e-simple-async-result.h"

struct _ESimpleAsyncResultPrivate {
	GObject *source_object;
	GAsyncReadyCallback callback;
	gpointer callback_user_data;
	gpointer source_tag;

	gpointer user_data;
	GDestroyNotify destroy_user_data;

	gpointer op_pointer;
};

static void e_simple_async_result_iface_init (GAsyncResultIface *iface);

G_DEFINE_TYPE_WITH_CODE (ESimpleAsyncResult, e_simple_async_result, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, e_simple_async_result_iface_init))

static gpointer
e_simple_async_result_iface_get_user_data (GAsyncResult *result)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return eresult->priv->callback_user_data;
}

static GObject *
e_simple_async_result_iface_get_source_object (GAsyncResult *result)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return eresult->priv->source_object;
}

static gboolean
e_simple_async_result_iface_is_tagged (GAsyncResult *result,
				       gpointer source_tag)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return eresult && eresult->priv->source_tag == source_tag;
}

static void
e_simple_async_result_iface_init (GAsyncResultIface *iface)
{
	iface->get_user_data = e_simple_async_result_iface_get_user_data;
	iface->get_source_object = e_simple_async_result_iface_get_source_object;
	iface->is_tagged = e_simple_async_result_iface_is_tagged;
}

static void
e_simple_async_result_finalize (GObject *object)
{
	ESimpleAsyncResult *result = E_SIMPLE_ASYNC_RESULT (object);

	if (result->priv->user_data && result->priv->destroy_user_data)
		result->priv->destroy_user_data (result->priv->user_data);

	result->priv->destroy_user_data = NULL;
	result->priv->user_data = NULL;

	g_clear_object (&result->priv->source_object);

	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_simple_async_result_parent_class)->finalize (object);
}

static void
e_simple_async_result_class_init (ESimpleAsyncResultClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ESimpleAsyncResultPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_simple_async_result_finalize;
}

static void
e_simple_async_result_init (ESimpleAsyncResult *result)
{
	result->priv = G_TYPE_INSTANCE_GET_PRIVATE (result, E_TYPE_SIMPLE_ASYNC_RESULT, ESimpleAsyncResultPrivate);
}

ESimpleAsyncResult *
e_simple_async_result_new (GObject *source_object,
			   GAsyncReadyCallback callback,
			   gpointer user_data,
			   gpointer source_tag)
{
	ESimpleAsyncResult *result;

	g_return_val_if_fail (callback != NULL, NULL);
	if (source_object)
		g_return_val_if_fail (G_IS_OBJECT (source_object), NULL);

	result = g_object_new (E_TYPE_SIMPLE_ASYNC_RESULT, NULL);

	result->priv->source_object = source_object ? g_object_ref (source_object) : NULL;
	result->priv->callback = callback;
	result->priv->callback_user_data = user_data;
	result->priv->source_tag = source_tag;

	return result;
}

void
e_simple_async_result_set_user_data (ESimpleAsyncResult *result,
				     gpointer user_data,
				     GDestroyNotify destroy_user_data)
{
	ESimpleAsyncResult *eresult;

	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	if (eresult->priv->user_data == user_data)
		return;

	if (eresult->priv->user_data && eresult->priv->destroy_user_data)
		eresult->priv->destroy_user_data (eresult->priv->user_data);

	eresult->priv->user_data = user_data;
	eresult->priv->destroy_user_data = destroy_user_data;
}

gpointer
e_simple_async_result_get_user_data (ESimpleAsyncResult *result)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return eresult->priv->user_data;
}

gpointer
e_simple_async_result_steal_user_data (ESimpleAsyncResult *result)
{
	ESimpleAsyncResult *eresult;
	gpointer user_data;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	user_data = eresult->priv->user_data;

	eresult->priv->user_data = NULL;
	eresult->priv->destroy_user_data = NULL;

	return user_data;
}

void
e_simple_async_result_set_op_pointer (ESimpleAsyncResult *result,
				      gpointer ptr)
{
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	result->priv->op_pointer = ptr;
}

gpointer
e_simple_async_result_get_op_pointer (ESimpleAsyncResult *result)
{
	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	return result->priv->op_pointer;
}

void
e_simple_async_result_complete (ESimpleAsyncResult *result)
{
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	g_object_ref (result);

	result->priv->callback (result->priv->source_object, G_ASYNC_RESULT (result), result->priv->callback_user_data);

	g_object_unref (result);
}
