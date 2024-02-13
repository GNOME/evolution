/*
 * e-client-combo-box.c
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
 */

/**
 * SECTION: e-client-combo-box
 * @include: e-util/e-util.h
 * @short_description: Combo box of #EClient instances
 *
 * #EClientComboBox extends the functionality of #ESourceComboBox by
 * providing convenient access to #EClient instances with #EClientCache.
 *
 * As a future enhancement, this widget may also display status information
 * about the backends associated with the displayed data sources, similar to
 * #EClientSelector.
 **/

#include "e-client-combo-box.h"

struct _EClientComboBoxPrivate {
	EClientCache *client_cache;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE
};

G_DEFINE_TYPE_WITH_PRIVATE (EClientComboBox, e_client_combo_box, E_TYPE_SOURCE_COMBO_BOX)

static void
client_combo_box_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			e_client_combo_box_set_client_cache (
				E_CLIENT_COMBO_BOX (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
client_combo_box_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_take_object (
				value,
				e_client_combo_box_ref_client_cache (
				E_CLIENT_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
client_combo_box_dispose (GObject *object)
{
	EClientComboBox *self = E_CLIENT_COMBO_BOX (object);

	g_clear_object (&self->priv->client_cache);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_client_combo_box_parent_class)->dispose (object);
}

static void
e_client_combo_box_class_init (EClientComboBoxClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = client_combo_box_set_property;
	object_class->get_property = client_combo_box_get_property;
	object_class->dispose = client_combo_box_dispose;

	/**
	 * EClientComboBox:client-cache:
	 *
	 * Cache of shared #EClient instances.
	 **/
	/* XXX Don't use G_PARAM_CONSTRUCT_ONLY here.  We need to allow
	 *     for this class to be instantiated by a GtkBuilder with no
	 *     special construct parameters, and then subsequently give
	 *     it an EClientCache. */
	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Cache of shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_client_combo_box_init (EClientComboBox *combo_box)
{
	combo_box->priv = e_client_combo_box_get_instance_private (combo_box);
}

/**
 * e_client_combo_box_new:
 * @client_cache: an #EClientCache
 * @extension_name: the name of an #ESource extension
 *
 * Creates a new #EClientComboBox widget that lets the user pick an #ESource
 * from the provided @client_cache.  The displayed sources are restricted to
 * those which have an @extension_name extension.
 *
 * Returns: a new #EClientComboBox
 **/
GtkWidget *
e_client_combo_box_new (EClientCache *client_cache,
                        const gchar *extension_name)
{
	ESourceRegistry *registry;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	registry = e_client_cache_ref_registry (client_cache);

	widget = g_object_new (
		E_TYPE_CLIENT_COMBO_BOX,
		"client-cache", client_cache,
		"extension-name", extension_name,
		"registry", registry, NULL);

	g_object_unref (registry);

	return widget;
}

/**
 * e_client_combo_box_ref_client_cache:
 * @combo_box: an #EClientComboBox
 *
 * Returns the #EClientCache passed to e_client_combo_box_new().
 *
 * The returned #EClientCache is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClientCache
 **/
EClientCache *
e_client_combo_box_ref_client_cache (EClientComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_CLIENT_COMBO_BOX (combo_box), NULL);

	return g_object_ref (combo_box->priv->client_cache);
}

/**
 * e_client_combo_box_set_client_cache:
 * @combo_box: an #EClientComboBox
 *
 * Simultaneously sets the #EClientComboBox:client-cache and
 * #ESourceComboBox:registry properties.
 *
 * This function is intended for cases where @combo_box is instantiated
 * by a #GtkBuilder and has to be given an #EClientCache after it is fully
 * constructed.
 **/
void
e_client_combo_box_set_client_cache (EClientComboBox *combo_box,
                                     EClientCache *client_cache)
{
	ESourceRegistry *registry = NULL;

	g_return_if_fail (E_IS_CLIENT_COMBO_BOX (combo_box));

	if (combo_box->priv->client_cache == client_cache)
		return;

	if (client_cache != NULL) {
		g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
		g_object_ref (client_cache);
	}

	if (combo_box->priv->client_cache != NULL)
		g_object_unref (combo_box->priv->client_cache);

	combo_box->priv->client_cache = client_cache;

	if (client_cache != NULL)
		registry = e_client_cache_ref_registry (client_cache);

	e_source_combo_box_set_registry (
		E_SOURCE_COMBO_BOX (combo_box), registry);

	g_clear_object (&registry);

	g_object_notify (G_OBJECT (combo_box), "client-cache");
}

/**
 * e_client_combo_box_get_client_sync:
 * @combo_box: an #EClientComboBox
 * @source: an #ESource
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Obtains a shared #EClient instance for @source, or else creates a new
 * #EClient instance to be shared.
 *
 * The #ESourceComboBox:extension-name property determines the type of
 * #EClient to obtain.  See e_client_cache_get_client_sync() for a list
 * of valid extension names.
 *
 * If a request for the same @source and #ESourceComboBox:extension-name
 * is already in progress when this function is called, this request will
 * "piggyback" on the in-progress request such that they will both succeed
 * or fail simultaneously.
 *
 * Unreference the returned #EClient with g_object_unref() when finished
 * with it.  If an error occurs, the function will set @error and return
 * %NULL.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_combo_box_get_client_sync (EClientComboBox *combo_box,
                                    ESource *source,
                                    GCancellable *cancellable,
                                    GError **error)
{
	EAsyncClosure *closure;
	GAsyncResult *result;
	EClient *client;

	g_return_val_if_fail (E_IS_CLIENT_COMBO_BOX (combo_box), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	closure = e_async_closure_new ();

	e_client_combo_box_get_client (
		combo_box, source, cancellable,
		e_async_closure_callback, closure);

	result = e_async_closure_wait (closure);

	client = e_client_combo_box_get_client_finish (
		combo_box, result, error);

	e_async_closure_free (closure);

	return client;
}

/* Helper for e_client_combo_box_get_client() */
static void
client_combo_box_get_client_done_cb (GObject *source_object,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
	EClient *client;
	GTask *task;
	GError *error = NULL;

	task = G_TASK (user_data);

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (client != NULL)
		g_task_return_pointer (task, g_steal_pointer (&client), g_object_unref);
	else
		g_task_return_error (task, g_steal_pointer (&error));

	g_object_unref (task);
}

/**
 * e_client_combo_box_get_client:
 * @combo_box: an #EClientComboBox
 * @source: an #ESource
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously obtains a shared #EClient instance for @source, or else
 * creates a new #EClient instance to be shared.
 *
 * The #ESourceComboBox:extension-name property determines the type of
 * #EClient to obtain.  See e_client_cache_get_client_sync() for a list
 * of valid extension names.
 *
 * If a request for the same @source and #ESourceComboBox:extension-name
 * is already in progress when this function is called, this request will
 * "piggyback" on the in-progress request such that they will both succeed
 * or fail simultaneously.
 *
 * When the operation is finished, @callback will be called.  You can
 * then call e_client_combo_box_get_client_finish() to get the result of
 * the operation.
 **/
void
e_client_combo_box_get_client (EClientComboBox *combo_box,
                               ESource *source,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	EClientCache *client_cache;
	GTask *task;
	const gchar *extension_name;

	g_return_if_fail (E_IS_CLIENT_COMBO_BOX (combo_box));
	g_return_if_fail (E_IS_SOURCE (source));

	task = g_task_new (combo_box, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_client_combo_box_get_client);

	extension_name = e_source_combo_box_get_extension_name (
		E_SOURCE_COMBO_BOX (combo_box));

	client_cache = e_client_combo_box_ref_client_cache (combo_box);

	e_client_cache_get_client (
		client_cache, source,
		extension_name, (guint32) -1, cancellable,
		client_combo_box_get_client_done_cb,
		g_steal_pointer (&task));

	g_object_unref (client_cache);
}

/**
 * e_client_combo_box_get_client_finish:
 * @combo_box: an #EClientComboBox
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finishes the operation started with e_client_combo_box_get_client().
 *
 * Unreference the returned #EClient with g_object_unref() when finished
 * with it.  If an error occurred, the function will set @error and return
 * %NULL.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_combo_box_get_client_finish (EClientComboBox *combo_box,
                                      GAsyncResult *result,
                                      GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, combo_box), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_client_combo_box_get_client), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * e_client_combo_box_ref_cached_client:
 * @combo_box: an #EClientComboBox
 * @source: an #ESource
 *
 * Returns a shared #EClient instance for @source and the value of
 * #ESourceComboBox:extension-name if such an instance is already cached,
 * or else %NULL.  This function does not create a new #EClient instance,
 * and therefore does not block.
 *
 * The returned #EClient is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_combo_box_ref_cached_client (EClientComboBox *combo_box,
                                      ESource *source)
{
	EClient *client;
	EClientCache *client_cache;
	const gchar *extension_name;

	g_return_val_if_fail (E_IS_CLIENT_COMBO_BOX (combo_box), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	extension_name = e_source_combo_box_get_extension_name (
		E_SOURCE_COMBO_BOX (combo_box));

	client_cache = e_client_combo_box_ref_client_cache (combo_box);

	client = e_client_cache_ref_cached_client (
		client_cache, source, extension_name);

	g_object_unref (client_cache);

	return client;
}

