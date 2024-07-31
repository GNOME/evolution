/*
 * e-proxy-selector.c
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
 * SECTION: e-proxy-selector
 * @include: e-util/e-util.h
 * @short_description: Select and manage proxy profiles
 *
 * #EProxySelector displays a list of available proxy profiles, with inline
 * toolbar controls for adding and removing profiles.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-proxy-selector.h"

typedef struct _AsyncContext AsyncContext;

struct _EProxySelectorPrivate {
	ESourceRegistry *registry;
	gulong source_added_handler_id;
	gulong source_changed_handler_id;
	gulong source_removed_handler_id;

	GtkTreeSelection *selection;
	gulong selection_changed_handler_id;

	guint refresh_idle_id;
};

struct _AsyncContext {
	EProxySelector *selector;
	ESource *scratch_source;
};

enum {
	PROP_0,
	PROP_REGISTRY,
	PROP_SELECTED
};

enum {
	COLUMN_DISPLAY_NAME,
	COLUMN_SOURCE
};

G_DEFINE_TYPE_WITH_PRIVATE (EProxySelector, e_proxy_selector, E_TYPE_TREE_VIEW_FRAME)

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->selector);
	g_clear_object (&async_context->scratch_source);

	g_slice_free (AsyncContext, async_context);
}

static gchar *
proxy_selector_pick_display_name (EProxySelector *selector)
{
	ESourceRegistry *registry;
	GList *list, *link;
	const gchar *base_name = _("Custom Proxy");
	const gchar *extension_name;
	gchar *display_name;
	guint ii = 0;

	extension_name = E_SOURCE_EXTENSION_PROXY;
	registry = e_proxy_selector_get_registry (selector);
	list = e_source_registry_list_sources (registry, extension_name);

	/* Convert the list of ESources to a list of display names. */
	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		link->data = e_source_dup_display_name (source);
		g_object_unref (source);
	}

	display_name = g_strdup (base_name);

try_again:
	link = g_list_find_custom (
		list, display_name, (GCompareFunc) g_utf8_collate);

	if (link != NULL) {
		g_free (display_name);
		display_name = g_strdup_printf ("%s (%u)", base_name, ++ii);
		goto try_again;
	}

	g_list_free_full (list, (GDestroyNotify) g_free);

	return display_name;
}

static void
proxy_selector_commit_source_cb (GObject *object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	e_source_registry_commit_source_finish (
		E_SOURCE_REGISTRY (object), result, &local_error);

	if (local_error == NULL) {
		/* Refresh the tree model immediately. */
		e_proxy_selector_refresh (async_context->selector);

		/* Select the newly added proxy data source.  Note that
		 * e_proxy_selector_set_selected() uses e_source_equal()
		 * to match the input ESource to a tree model row, so it
		 * should match our scratch ESource to the newly-created
		 * ESource since they both have the same UID. */
		e_proxy_selector_set_selected (
			async_context->selector,
			async_context->scratch_source);
	} else {
		/* FIXME Hand the error off to an EAlertSink. */
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_error_free (local_error);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (async_context->selector), TRUE);

	async_context_free (async_context);
}

static void
proxy_selector_remove_source_cb (GObject *object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	EProxySelector *selector;
	GError *local_error = NULL;

	selector = E_PROXY_SELECTOR (user_data);

	e_source_remove_finish (E_SOURCE (object), result, &local_error);

	if (local_error != NULL) {
		/* FIXME Hand the error off to an EAlertSink. */
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_error_free (local_error);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (selector), TRUE);

	g_object_unref (selector);
}

static gboolean
proxy_selector_action_add_cb (EProxySelector *selector,
			      EUIAction *action)
{
	AsyncContext *async_context;
	ESourceRegistry *registry;
	ESourceProxy *extension;
	ESource *scratch_source;
	const gchar *extension_name;
	gchar *display_name;

	const gchar * const ignore_hosts[] = {
		"localhost",
		"127.0.0.0/8",
		"::1",
		NULL
	};

	scratch_source = e_source_new (NULL, NULL, NULL);

	display_name = proxy_selector_pick_display_name (selector);
	e_source_set_display_name (scratch_source, display_name);
	g_free (display_name);

	extension_name = E_SOURCE_EXTENSION_PROXY;
	extension = e_source_get_extension (scratch_source, extension_name);
	e_source_proxy_set_ignore_hosts (extension, ignore_hosts);

	registry = e_proxy_selector_get_registry (selector);

	/* Disable the selector until the commit operation completes. */
	gtk_widget_set_sensitive (GTK_WIDGET (selector), FALSE);

	async_context = g_slice_new0 (AsyncContext);
	async_context->selector = g_object_ref (selector);
	async_context->scratch_source = g_object_ref (scratch_source);

	e_source_registry_commit_source (
		registry, scratch_source, NULL,
		proxy_selector_commit_source_cb,
		async_context);

	g_object_unref (scratch_source);

	return TRUE;
}

static gboolean
proxy_selector_action_remove_cb (EProxySelector *selector,
				 EUIAction *action)
{
	ESource *selected_source;

	selected_source = e_proxy_selector_ref_selected (selector);
	g_return_val_if_fail (selected_source != NULL, FALSE);

	/* Disable the selector until the remove operation completes. */
	gtk_widget_set_sensitive (GTK_WIDGET (selector), FALSE);

	e_source_remove (
		selected_source, NULL,
		proxy_selector_remove_source_cb,
		g_object_ref (selector));

	g_object_unref (selected_source);

	return TRUE;
}

static gboolean
proxy_selector_refresh_idle_cb (gpointer user_data)
{
	EProxySelector *selector = user_data;

	/* The refresh function will clear the idle ID. */
	e_proxy_selector_refresh (selector);

	return FALSE;
}

static void
proxy_selector_schedule_refresh (EProxySelector *selector)
{
	/* Use an idle callback to limit how frequently we refresh
	 * the tree model in case the registry is emitting lots of
	 * signals at once. */

	if (selector->priv->refresh_idle_id == 0) {
		selector->priv->refresh_idle_id = g_idle_add (
			proxy_selector_refresh_idle_cb, selector);
	}
}

static void
proxy_selector_cell_edited_cb (GtkCellRendererText *renderer,
                               const gchar *path_string,
                               const gchar *new_name,
                               EProxySelector *selector)
{
	ETreeViewFrame *tree_view_frame;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	ESource *source;

	if (new_name == NULL || *new_name == '\0')
		return;

	tree_view_frame = E_TREE_VIEW_FRAME (selector);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);
	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);
	gtk_tree_path_free (path);

	/* EProxyPreferences will detect the change and commit it. */
	e_source_set_display_name (source, new_name);

	e_proxy_selector_refresh (selector);

	g_object_unref (source);
}

static void
proxy_selector_source_added_cb (ESourceRegistry *registry,
                                ESource *source,
                                EProxySelector *selector)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_PROXY))
		proxy_selector_schedule_refresh (selector);
}

static void
proxy_selector_source_changed_cb (ESourceRegistry *registry,
                                  ESource *source,
                                  EProxySelector *selector)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_PROXY))
		proxy_selector_schedule_refresh (selector);
}

static void
proxy_selector_source_removed_cb (ESourceRegistry *registry,
                                  ESource *source,
                                  EProxySelector *selector)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_PROXY))
		proxy_selector_schedule_refresh (selector);
}

static void
proxy_selector_selection_changed_cb (GtkTreeSelection *selection,
                                     EProxySelector *selector)
{
	g_object_notify (G_OBJECT (selector), "selected");
}

static void
proxy_selector_set_registry (EProxySelector *selector,
                             ESourceRegistry *registry)
{
	gulong handler_id;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (selector->priv->registry == NULL);

	selector->priv->registry = g_object_ref (registry);

	handler_id = g_signal_connect (
		registry, "source-added",
		G_CALLBACK (proxy_selector_source_added_cb), selector);
	selector->priv->source_added_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (proxy_selector_source_changed_cb), selector);
	selector->priv->source_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (proxy_selector_source_removed_cb), selector);
	selector->priv->source_removed_handler_id = handler_id;
}

static void
proxy_selector_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			proxy_selector_set_registry (
				E_PROXY_SELECTOR (object),
				g_value_get_object (value));
			return;

		case PROP_SELECTED:
			e_proxy_selector_set_selected (
				E_PROXY_SELECTOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_selector_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_proxy_selector_get_registry (
				E_PROXY_SELECTOR (object)));
			return;

		case PROP_SELECTED:
			g_value_take_object (
				value,
				e_proxy_selector_ref_selected (
				E_PROXY_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_selector_dispose (GObject *object)
{
	EProxySelector *self = E_PROXY_SELECTOR (object);

	if (self->priv->source_added_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_added_handler_id);
		self->priv->source_added_handler_id = 0;
	}

	if (self->priv->source_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_changed_handler_id);
		self->priv->source_changed_handler_id = 0;
	}

	if (self->priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_removed_handler_id);
		self->priv->source_removed_handler_id = 0;
	}

	if (self->priv->selection_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->selection,
			self->priv->selection_changed_handler_id);
		self->priv->selection_changed_handler_id = 0;
	}

	if (self->priv->refresh_idle_id > 0) {
		g_source_remove (self->priv->refresh_idle_id);
		self->priv->refresh_idle_id = 0;
	}

	g_clear_object (&self->priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_proxy_selector_parent_class)->dispose (object);
}

static void
proxy_selector_constructed (GObject *object)
{
	EProxySelector *selector;
	ETreeViewFrame *tree_view_frame;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkListStore *list_store;
	EUIAction *action;
	const gchar *tooltip;
	gulong handler_id;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_proxy_selector_parent_class)->constructed (object);

	selector = E_PROXY_SELECTOR (object);

	tree_view_frame = E_TREE_VIEW_FRAME (object);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	gtk_tree_view_set_reorderable (tree_view, FALSE);
	gtk_tree_view_set_headers_visible (tree_view, FALSE);

	/* Configure the toolbar actions. */

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_ADD);
	tooltip = _("Create a new proxy profile");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_REMOVE);
	tooltip = _("Delete the selected proxy profile");
	e_ui_action_set_tooltip (action, tooltip);

	/* Configure the tree view column. */

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (
		G_OBJECT (renderer),
		"editable", TRUE,
		"ellipsize", PANGO_ELLIPSIZE_END, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (proxy_selector_cell_edited_cb), selector);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COLUMN_DISPLAY_NAME);
	gtk_tree_view_append_column (tree_view, column);

	/* Listen for tree view selection changes. */

	selection = gtk_tree_view_get_selection (tree_view);
	selector->priv->selection = g_object_ref (selection);

	handler_id = g_signal_connect (
		selection, "changed",
		G_CALLBACK (proxy_selector_selection_changed_cb), selector);
	selector->priv->selection_changed_handler_id = handler_id;

	/* Create and populate the tree model. */

	list_store = gtk_list_store_new (2, G_TYPE_STRING, E_TYPE_SOURCE);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));
	g_object_unref (list_store);

	e_proxy_selector_refresh (E_PROXY_SELECTOR (object));
}

static void
proxy_selector_update_toolbar_actions (ETreeViewFrame *tree_view_frame)
{
	EProxySelector *selector;
	ESource *selected;
	EUIAction *action;
	gboolean sensitive;

	selector = E_PROXY_SELECTOR (tree_view_frame);
	selected = e_proxy_selector_ref_selected (selector);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_REMOVE);
	sensitive = e_source_get_removable (selected);
	e_ui_action_set_sensitive (action, sensitive);

	g_object_unref (selected);

	/* Chain up to parent's update_toolbar_actions() method. */
	E_TREE_VIEW_FRAME_CLASS (e_proxy_selector_parent_class)->
		update_toolbar_actions (tree_view_frame);
}

static void
e_proxy_selector_class_init (EProxySelectorClass *class)
{
	GObjectClass *object_class;
	ETreeViewFrameClass *tree_view_frame_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = proxy_selector_set_property;
	object_class->get_property = proxy_selector_get_property;
	object_class->dispose = proxy_selector_dispose;
	object_class->constructed = proxy_selector_constructed;

	tree_view_frame_class = E_TREE_VIEW_FRAME_CLASS (class);
	tree_view_frame_class->update_toolbar_actions =
				proxy_selector_update_toolbar_actions;

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SELECTED,
		g_param_spec_object (
			"selected",
			"Selected",
			"The selected data source",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_proxy_selector_init (EProxySelector *selector)
{
	selector->priv = e_proxy_selector_get_instance_private (selector);

	/* In this particular case, it's easier to connect handlers
	 * to detailed signal names than to override the class method. */

	g_signal_connect (
		selector,
		"toolbar-action-activate::"
		E_TREE_VIEW_FRAME_ACTION_ADD,
		G_CALLBACK (proxy_selector_action_add_cb), NULL);

	g_signal_connect (
		selector,
		"toolbar-action-activate::"
		E_TREE_VIEW_FRAME_ACTION_REMOVE,
		G_CALLBACK (proxy_selector_action_remove_cb), NULL);
}

/**
 * e_proxy_selector_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EProxySelector widget using #ESource instances in @registry.
 *
 * Returns: a new #EProxySelector
 **/
GtkWidget *
e_proxy_selector_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_PROXY_SELECTOR,
		"registry", registry, NULL);
}

/**
 * e_proxy_selector_refresh:
 * @selector: an #EProxySelector
 *
 * Rebuilds the @selector's list store with an updated list of #ESource
 * instances that describe a network proxy profile, without disrupting the
 * previously selected item (if possible).
 *
 * This funtion is called automatically in response to #ESourceRegistry
 * signals which are pertinent to the @selector.
 **/
void
e_proxy_selector_refresh (EProxySelector *selector)
{
	ETreeViewFrame *tree_view_frame;
	ESourceRegistry *registry;
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	ESource *builtin_source;
	ESource *selected;
	GList *list, *link;
	const gchar *extension_name;

	g_return_if_fail (E_IS_PROXY_SELECTOR (selector));

	if (selector->priv->refresh_idle_id > 0) {
		g_source_remove (selector->priv->refresh_idle_id);
		selector->priv->refresh_idle_id = 0;
	}

	tree_view_frame = E_TREE_VIEW_FRAME (selector);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);
	tree_model = gtk_tree_view_get_model (tree_view);

	selected = e_proxy_selector_ref_selected (selector);

	gtk_list_store_clear (GTK_LIST_STORE (tree_model));

	extension_name = E_SOURCE_EXTENSION_PROXY;
	registry = e_proxy_selector_get_registry (selector);
	list = e_source_registry_list_enabled (registry, extension_name);

	builtin_source = e_source_registry_ref_builtin_proxy (registry);
	g_warn_if_fail (builtin_source != NULL);

	/* Always list the built-in proxy profile first. */
	link = g_list_find (list, builtin_source);
	if (link != NULL && list != link) {
		list = g_list_remove_link (list, link);
		list = g_list_concat (link, list);
	}

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source;
		GtkTreeIter iter;
		const gchar *display_name;

		source = E_SOURCE (link->data);
		display_name = e_source_get_display_name (source);

		gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			COLUMN_DISPLAY_NAME, display_name,
			COLUMN_SOURCE, source, -1);
	}

	g_clear_object (&builtin_source);

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Try and restore the previous selected source or else pick
	 * the built-in proxy profile, which is always listed first. */

	e_proxy_selector_set_selected (selector, selected);

	g_clear_object (&selected);
}

/**
 * e_proxy_selector_get_registry:
 * @selector: an #EProxySelector
 *
 * Returns the #ESourceRegistry passed to e_proxy_selector_get_registry().
 *
 * Returns: an #ESourceRegistry
 **/
ESourceRegistry *
e_proxy_selector_get_registry (EProxySelector *selector)
{
	g_return_val_if_fail (E_IS_PROXY_SELECTOR (selector), NULL);

	return selector->priv->registry;
}

/**
 * e_proxy_selector_ref_selected:
 * @selector: an #EProxySelector
 *
 * Returns the selected #ESource in @selector.
 *
 * The function tries to ensure a valid #ESource is always returned,
 * falling back to e_source_registry_ref_builtin_proxy() if necessary.
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #ESource
 **/
ESource *
e_proxy_selector_ref_selected (EProxySelector *selector)
{
	ETreeViewFrame *tree_view_frame;
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	ESource *source = NULL;

	g_return_val_if_fail (E_IS_PROXY_SELECTOR (selector), NULL);

	tree_view_frame = E_TREE_VIEW_FRAME (selector);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_selection_get_selected (selection, &tree_model, &iter)) {
		gtk_tree_model_get (
			tree_model, &iter,
			COLUMN_SOURCE, &source, -1);
	}

	/* The built-in proxy profile is implicitly selected when
	 * no proxy profile is actually selected in the tree view. */
	if (source == NULL) {
		ESourceRegistry *registry;

		registry = e_proxy_selector_get_registry (selector);
		source = e_source_registry_ref_builtin_proxy (registry);
		g_return_val_if_fail (source != NULL, NULL);
	}

	return source;
}

/**
 * e_proxy_selector_set_selected:
 * @selector: an #EProxySelector
 * @source: an #ESource, or %NULL for the built-in proxy profile
 *
 * Finds the corresponding tree model row for @source, selects the row,
 * and returns %TRUE.  If no corresponding tree model row for @source is
 * found, the selection remains unchanged and the function returns %FALSE.
 *
 * Returns: whether @source was selected
 **/
gboolean
e_proxy_selector_set_selected (EProxySelector *selector,
                               ESource *source)
{
	ETreeViewFrame *tree_view_frame;
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_valid;

	g_return_val_if_fail (E_IS_PROXY_SELECTOR (selector), FALSE);
	g_return_val_if_fail (source == NULL || E_IS_SOURCE (source), FALSE);

	if (source == NULL) {
		ESourceRegistry *registry;

		registry = e_proxy_selector_get_registry (selector);
		source = e_source_registry_ref_builtin_proxy (registry);
		g_return_val_if_fail (source != NULL, FALSE);
	}

	tree_view_frame = E_TREE_VIEW_FRAME (selector);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);
	tree_model = gtk_tree_view_get_model (tree_view);

	iter_valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_valid) {
		ESource *candidate = NULL;
		gboolean match;

		gtk_tree_model_get (
			tree_model, &iter,
			COLUMN_SOURCE, &candidate, -1);

		match = e_source_equal (source, candidate);

		g_object_unref (candidate);

		if (match)
			break;

		iter_valid = gtk_tree_model_iter_next (tree_model, &iter);
	}

	if (iter_valid) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (tree_view);
		gtk_tree_selection_select_iter (selection, &iter);
	}

	return iter_valid;
}

