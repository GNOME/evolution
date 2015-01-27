/*
 * e-client-selector.c
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
 * SECTION: e-client-selector
 * @include: e-util/e-util.h
 * @short_description: Tree view of #EClient instances
 *
 * #EClientSelector extends the functionality of #ESourceSelector by
 * utilizing an #EClientCache to display status information about the
 * backends associated with the displayed data sources.
 **/

#include "e-client-selector.h"

#define E_CLIENT_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CLIENT_SELECTOR, EClientSelectorPrivate))

typedef struct _AsyncContext AsyncContext;

struct _EClientSelectorPrivate {
	EClientCache *client_cache;
	gulong backend_died_handler_id;
	gulong client_created_handler_id;
	gulong client_notify_online_handler_id;
};

struct _AsyncContext {
	EClientSelector *selector;
	ESource *source;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE
};

G_DEFINE_TYPE (
	EClientSelector,
	e_client_selector,
	E_TYPE_SOURCE_SELECTOR)

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->selector);
	g_clear_object (&async_context->source);

	g_slice_free (AsyncContext, async_context);
}

static void
client_selector_update_status_icon_cb (GtkTreeViewColumn *column,
                                       GtkCellRenderer *renderer,
                                       GtkTreeModel *model,
                                       GtkTreeIter *iter,
                                       gpointer user_data)
{
	GtkWidget *tree_view;
	EClient *client;
	const gchar *icon_name = NULL;

	tree_view = gtk_tree_view_column_get_tree_view (column);

	client = e_client_selector_ref_cached_client_by_iter (
		E_CLIENT_SELECTOR (tree_view), iter);

	if (client != NULL) {
		if (e_client_is_online (client))
			icon_name = "network-idle-symbolic";
		else
			icon_name = "network-offline-symbolic";

		g_object_unref (client);

	} else {
		ESource *source;

		/* No client... did the backend die? */
		source = e_source_selector_ref_source_by_iter (
			E_SOURCE_SELECTOR (tree_view), iter);

		if (source != NULL) {
			gboolean dead_backend;

			dead_backend = e_client_selector_is_backend_dead (
				E_CLIENT_SELECTOR (tree_view), source);
			if (dead_backend) {
				icon_name = "network-error-symbolic";
			} else {
				gpointer data;

				/* See client_selector_can_reach_cb() */
				data = g_object_get_data (
					G_OBJECT (source),
					"initial-icon-name");
				icon_name = (const gchar *) data;
			}

			g_object_unref (source);
		}
	}

	if (icon_name != NULL) {
		GIcon *icon;

		/* Use fallbacks if symbolic icons are not available. */
		icon = g_themed_icon_new_with_default_fallbacks (icon_name);
		g_object_set (renderer, "gicon", icon, NULL);
		g_object_unref (icon);
	} else {
		g_object_set (renderer, "gicon", NULL, NULL);
	}
}

static void
client_selector_update_row (EClientSelector *selector,
                            EClient *client)
{
	ESource *source;

	source = e_client_get_source (client);
	e_source_selector_update_row (E_SOURCE_SELECTOR (selector), source);
}

static void
client_selector_backend_died_cb (EClientCache *client_cache,
                                 EClient *client,
                                 EAlert *alert,
                                 EClientSelector *selector)
{
	client_selector_update_row (selector, client);
}

static void
client_selector_client_created_cb (EClientCache *client_cache,
                                   EClient *client,
                                   EClientSelector *selector)
{
	client_selector_update_row (selector, client);
}

static void
client_selector_client_notify_cb (EClientCache *client_cache,
                                  EClient *client,
                                  GParamSpec *pspec,
                                  EClientSelector *selector)
{
	client_selector_update_row (selector, client);
}

static void
client_selector_can_reach_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	EClient *client;
	AsyncContext *async_context;
	gboolean reachable;

	async_context = (AsyncContext *) user_data;

	/* We don't care about errors here.  This is just to show some
	 * initial icon next to the ESource before creating an EClient. */
	reachable = g_network_monitor_can_reach_finish (
		G_NETWORK_MONITOR (source_object), result, NULL);

	client = e_client_selector_ref_cached_client (
		async_context->selector, async_context->source);

	/* EClient's online state is authoritative.
	 * Defer to it if an instance already exists. */
	if (client == NULL) {
		const gchar *icon_name;

		if (reachable)
			icon_name = "network-idle-symbolic";
		else
			icon_name = "network-offline-symbolic";

		/* XXX Hackish way to stash the initial icon name. */
		g_object_set_data (
			G_OBJECT (async_context->source),
			"initial-icon-name", (gpointer) icon_name);

		e_source_selector_update_row (
			E_SOURCE_SELECTOR (async_context->selector),
			async_context->source);
	}

	g_clear_object (&client);

	async_context_free (async_context);
}

static void
client_selector_set_client_cache (EClientSelector *selector,
                                  EClientCache *client_cache)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (selector->priv->client_cache == NULL);

	selector->priv->client_cache = g_object_ref (client_cache);
}

static void
client_selector_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			client_selector_set_client_cache (
				E_CLIENT_SELECTOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
client_selector_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_take_object (
				value,
				e_client_selector_ref_client_cache (
				E_CLIENT_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
client_selector_dispose (GObject *object)
{
	EClientSelectorPrivate *priv;

	priv = E_CLIENT_SELECTOR_GET_PRIVATE (object);

	if (priv->backend_died_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->backend_died_handler_id);
		priv->backend_died_handler_id = 0;
	}

	if (priv->client_created_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->client_created_handler_id);
		priv->client_created_handler_id = 0;
	}

	if (priv->client_notify_online_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->client_notify_online_handler_id);
		priv->client_notify_online_handler_id = 0;
	}

	g_clear_object (&priv->client_cache);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_client_selector_parent_class)->dispose (object);
}

static void
client_selector_constructed (GObject *object)
{
	EClientSelector *selector;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GNetworkMonitor *network_monitor;
	const gchar *extension_name;
	GList *list, *link;
	gulong handler_id;

	selector = E_CLIENT_SELECTOR (object);
	client_cache = e_client_selector_ref_client_cache (selector);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_client_selector_parent_class)->constructed (object);

	/* Append an icon to hint at backend status. */

	tree_view = GTK_TREE_VIEW (object);
	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (tree_view, column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_set_cell_data_func (
		column, renderer,
		client_selector_update_status_icon_cb,
		NULL, (GDestroyNotify) NULL);

	/* Listen for signals that may change the icon. */

	handler_id = g_signal_connect (
		client_cache, "backend-died",
		G_CALLBACK (client_selector_backend_died_cb),
		selector);
	selector->priv->backend_died_handler_id = handler_id;

	handler_id = g_signal_connect (
		client_cache, "client-created",
		G_CALLBACK (client_selector_client_created_cb),
		selector);
	selector->priv->client_created_handler_id = handler_id;

	handler_id = g_signal_connect (
		client_cache, "client-notify::online",
		G_CALLBACK (client_selector_client_notify_cb),
		selector);
	selector->priv->client_notify_online_handler_id = handler_id;

	g_object_unref (client_cache);

	/* Have GNetworkMonitor make an initial guess at the online
	 * state of backends by evaluating the reachability of their
	 * host name.  This will show an initial status icon for all
	 * displayed ESources without actually opening a connection,
	 * since some backends are expensive to start unnecessarily.
	 *
	 * XXX It occurred to me after writing this that it would be
	 *     better for ESourceSelector to evaluate reachability of
	 *     ESource host names, and keep it up-to-date in response
	 *     to network changes.  It could automatically trigger a
	 *     GtkTreeModel::row-changed signal when it has a new host
	 *     reachability result, and provide that result via some
	 *     e_source_selector_get_host_reachable() for us to fall
	 *     back on if no EClient instance is available.
	 *
	 *     But the approach below is good enough for now.
	 */

	network_monitor = g_network_monitor_get_default ();

	registry = e_source_selector_get_registry (
		E_SOURCE_SELECTOR (selector));
	extension_name = e_source_selector_get_extension_name (
		E_SOURCE_SELECTOR (selector));

	list = e_source_registry_list_sources (registry, extension_name);

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESource *auth_source;
		ESourceAuthentication *auth_extension;
		GSocketConnectable *socket_connectable;
		const gchar *host;
		guint16 port;

		auth_source = e_source_registry_find_extension (
			registry, source, extension_name);

		if (auth_source == NULL)
			continue;

		auth_extension = e_source_get_extension (
			auth_source, extension_name);

		host = e_source_authentication_get_host (auth_extension);
		port = e_source_authentication_get_port (auth_extension);

		socket_connectable = g_network_address_new (host, port);

		/* XXX GNetworkAddress will happily take a NULL host
		 *     but then crash while enumerating the address,
		 *     so watch out for that. */
		if (host == NULL || !*host || g_ascii_strcasecmp (host, "localhost") == 0)
			g_clear_object (&socket_connectable);

		if (socket_connectable != NULL) {
			AsyncContext *async_context;

			async_context = g_slice_new0 (AsyncContext);
			async_context->selector = g_object_ref (selector);
			async_context->source = g_object_ref (source);

			g_network_monitor_can_reach_async (
				network_monitor, socket_connectable, NULL,
				client_selector_can_reach_cb, async_context);

			g_object_unref (socket_connectable);
		}

		g_object_unref (auth_source);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
e_client_selector_class_init (EClientSelectorClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EClientSelectorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = client_selector_set_property;
	object_class->get_property = client_selector_get_property;
	object_class->dispose = client_selector_dispose;
	object_class->constructed = client_selector_constructed;

	/**
	 * EClientSelector:client-cache:
	 *
	 * Cache of shared #EClient instances.
	 **/
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
e_client_selector_init (EClientSelector *selector)
{
	selector->priv = E_CLIENT_SELECTOR_GET_PRIVATE (selector);
}

/**
 * e_client_selector_new:
 * @client_cache: an #EClientCache
 * @extension_name: the name of an #ESource extension
 *
 * Displays a list of sources having an extension named @extension_name,
 * along with status information about the backends associated with them.
 * The sources are grouped by backend or groupware account, which are
 * described by the parent source.
 *
 * Returns: a new #EClientSelector
 **/
GtkWidget *
e_client_selector_new (EClientCache *client_cache,
                       const gchar *extension_name)
{
	ESourceRegistry *registry;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	registry = e_client_cache_ref_registry (client_cache);

	widget = g_object_new (
		E_TYPE_CLIENT_SELECTOR,
		"client-cache", client_cache,
		"extension-name", extension_name,
		"registry", registry, NULL);

	g_object_unref (registry);

	return widget;
}

/**
 * e_client_selector_ref_client_cache:
 * @selector: an #EClientSelector
 *
 * Returns the #EClientCache passed to e_client_selector_new().
 *
 * The returned #EClientCache is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClientCache
 **/
EClientCache *
e_client_selector_ref_client_cache (EClientSelector *selector)
{
	g_return_val_if_fail (E_IS_CLIENT_SELECTOR (selector), NULL);

	return g_object_ref (selector->priv->client_cache);
}

/**
 * e_client_selector_get_client_sync:
 * @selector: an #ESourceSelector
 * @source: an #ESource
 * @call_allow_auth_prompt: whether call allow-auth-prompt on the source first
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Obtains a shared #EClient instance for @source, or else creates a new
 * #EClient instance to be shared.
 *
 * The #ESourceSelector:extension-name property determines the type of
 * #EClient to obtain.  See e_client_cache_get_client_sync() for a list
 * of valid extension names.
 *
 * If a request for the same @source and #ESourceSelector:extension-name
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
e_client_selector_get_client_sync (EClientSelector *selector,
                                   ESource *source,
				   gboolean call_allow_auth_prompt,
                                   GCancellable *cancellable,
                                   GError **error)
{
	EClientCache *client_cache;
	const gchar *extension_name;
	EClient *client;

	g_return_val_if_fail (E_IS_CLIENT_SELECTOR (selector), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	if (call_allow_auth_prompt) {
		if (!e_source_allow_auth_prompt_sync (source, cancellable, error))
			return NULL;
	}

	extension_name = e_source_selector_get_extension_name (E_SOURCE_SELECTOR (selector));

	client_cache = e_client_selector_ref_client_cache (selector);

	client = e_client_cache_get_client_sync (
		client_cache, source,
		extension_name, cancellable, error);

	g_object_unref (client_cache);

	return client;
}

/* Helper for e_client_selector_get_client() */
static void
client_selector_get_client_done_cb (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	EClient *client;
	GSimpleAsyncResult *simple;
	GError *error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (client != NULL) {
		g_simple_async_result_set_op_res_gpointer (
			simple, g_object_ref (client),
			(GDestroyNotify) g_object_unref);
		g_object_unref (client);
	}

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);

	g_simple_async_result_complete (simple);

	g_object_unref (simple);
}

typedef struct _AllowAuthPromptData
{
	EClientSelector *selector;
	GSimpleAsyncResult *simple;
	GCancellable *cancellable;
} AllowAuthPromptData;

static void
client_selector_allow_auth_prompt_done_cb (GObject *source_object,
					   GAsyncResult *result,
					   gpointer user_data)
{
	AllowAuthPromptData *data;
	ESource *source;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));
	g_return_if_fail (user_data != NULL);

	data = user_data;
	source = E_SOURCE (source_object);

	e_source_allow_auth_prompt_finish (source, result, &local_error);

	if (local_error) {
		g_simple_async_result_take_error (data->simple, local_error);
		g_simple_async_result_complete (data->simple);
		local_error = NULL;
	} else {
		EClientCache *client_cache;
		const gchar *extension_name;

		extension_name = e_source_selector_get_extension_name (
			E_SOURCE_SELECTOR (data->selector));

		client_cache = e_client_selector_ref_client_cache (data->selector);

		e_client_cache_get_client (
			client_cache, source,
			extension_name, data->cancellable,
			client_selector_get_client_done_cb,
			g_object_ref (data->simple));

		g_object_unref (client_cache);
	}

	g_clear_object (&data->selector);
	g_clear_object (&data->simple);
	g_clear_object (&data->cancellable);
	g_free (data);
}

/**
 * e_client_selector_get_client:
 * @selector: an #ESourceSelector
 * @source: an #ESource
 * @call_allow_auth_prompt: whether call allow-auth-prompt on the source first
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously obtains a shared #EClient instance for @source, or else
 * creates a new #EClient instance to be shared.
 *
 * The #ESourceSelector:extension-name property determines the type of
 * #EClient to obtain.  See e_client_cache_get_client_sync() for a list
 * of valid extension names.
 *
 * If a request for the same @source and #ESourceSelector:extension-name
 * is already in progress when this function is called, this request will
 * "piggyback" on the in-progress request such that they will both succeed
 * or fail simultaneously.
 *
 * When the operation is finished, @callback will be called.  You can
 * then call e_client_selector_get_client_finish() to get the result of
 * the operation.
 **/
void
e_client_selector_get_client (EClientSelector *selector,
                              ESource *source,
			      gboolean call_allow_auth_prompt,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	GSimpleAsyncResult *simple;

	g_return_if_fail (E_IS_CLIENT_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	simple = g_simple_async_result_new (
		G_OBJECT (selector), callback,
		user_data, e_client_selector_get_client);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	if (call_allow_auth_prompt) {
		AllowAuthPromptData *data;

		data = g_new0 (AllowAuthPromptData, 1);
		data->selector = g_object_ref (selector);
		data->simple = g_object_ref (simple);
		data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

		e_source_allow_auth_prompt (source, cancellable,
			client_selector_allow_auth_prompt_done_cb, data);
	} else {
		EClientCache *client_cache;
		const gchar *extension_name;

		extension_name = e_source_selector_get_extension_name (
			E_SOURCE_SELECTOR (selector));

		client_cache = e_client_selector_ref_client_cache (selector);

		e_client_cache_get_client (
			client_cache, source,
			extension_name, cancellable,
			client_selector_get_client_done_cb,
			g_object_ref (simple));

		g_object_unref (client_cache);
	}

	g_object_unref (simple);
}

/**
 * e_client_selector_get_client_finish:
 * @selector: an #EClientSelector
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finishes the operation started with e_client_selector_get_client().
 *
 * Unreference the returned #EClient with g_object_unref() when finished
 * with it.  If an error occurred, the function will set @error and return
 * %NULL.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_selector_get_client_finish (EClientSelector *selector,
                                     GAsyncResult *result,
                                     GError **error)
{
	GSimpleAsyncResult *simple;
	EClient *client;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (selector),
		e_client_selector_get_client), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	client = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	g_return_val_if_fail (client != NULL, NULL);

	return g_object_ref (client);
}

/**
 * e_client_selector_ref_cached_client:
 * @selector: an #EClientSelector
 * @source: an #ESource
 *
 * Returns a shared #EClient instance for @source and the value of
 * #ESourceSelector:extension-name if such an instance is already cached,
 * or else %NULL.  This function does not create a new #EClient instance,
 * and therefore does not block.
 *
 * The returned #EClient is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_selector_ref_cached_client (EClientSelector *selector,
                                     ESource *source)
{
	EClient *client;
	EClientCache *client_cache;
	const gchar *extension_name;

	g_return_val_if_fail (E_IS_CLIENT_SELECTOR (selector), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	extension_name = e_source_selector_get_extension_name (
		E_SOURCE_SELECTOR (selector));

	client_cache = e_client_selector_ref_client_cache (selector);

	client = e_client_cache_ref_cached_client (
		client_cache, source, extension_name);

	g_object_unref (client_cache);

	return client;
}

/**
 * e_client_selector_ref_cached_client_by_iter:
 * @selector: an #EClientSelector
 * @iter: a #GtkTreeIter
 *
 * Returns a shared #EClient instance for the #ESource in the tree model
 * row pointed to by @iter and the value of #ESourceSelector:extension-name
 * if such an instance is already cached, or else %NULL.  This function does
 * not create a new #EClient instance, and therefore does not block.
 *
 * The returned #EClient is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_selector_ref_cached_client_by_iter (EClientSelector *selector,
                                             GtkTreeIter *iter)
{
	EClient *client = NULL;
	ESource *source;

	g_return_val_if_fail (E_IS_CLIENT_SELECTOR (selector), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	source = e_source_selector_ref_source_by_iter (
		E_SOURCE_SELECTOR (selector), iter);

	if (source != NULL) {
		client = e_client_selector_ref_cached_client (
			selector, source);
		g_object_unref (source);
	}

	return client;
}

/**
 * e_client_selector_is_backend_dead:
 * @selector: an #EClientSelector
 * @source: an #ESource
 *
 * Returns %TRUE if an #EClient instance for @source and the value of
 * #ESourceSelector:extension-name was recently discarded after having
 * emitted a #EClient:backend-died signal, and a replacement #EClient
 * instance has not yet been created.
 *
 * Returns: whether the backend for @source died
 **/
gboolean
e_client_selector_is_backend_dead (EClientSelector *selector,
                                   ESource *source)
{
	EClientCache *client_cache;
	const gchar *extension_name;
	gboolean dead_backend;

	g_return_val_if_fail (E_IS_CLIENT_SELECTOR (selector), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	extension_name = e_source_selector_get_extension_name (
		E_SOURCE_SELECTOR (selector));

	client_cache = e_client_selector_ref_client_cache (selector);

	dead_backend = e_client_cache_is_backend_dead (
		client_cache, source, extension_name);

	g_object_unref (client_cache);

	return dead_backend;
}

