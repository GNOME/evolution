/*
 * e-mail-account-tree-view.c
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
 */

#include "e-mail-account-tree-view.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/e-source-mail-account.h>

#define E_MAIL_ACCOUNT_TREE_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_ACCOUNT_TREE_VIEW, EMailAccountTreeViewPrivate))

#define SOURCE_IS_MAIL_ACCOUNT(source) \
	(e_source_has_extension ((source), E_SOURCE_EXTENSION_MAIL_ACCOUNT))

struct _EMailAccountTreeViewPrivate {
	ESourceRegistry *registry;
	guint refresh_idle_id;
};

enum {
	PROP_0,
	PROP_REGISTRY
};

enum {
	ENABLE_SELECTED,
	DISABLE_SELECTED,
	LAST_SIGNAL
};

enum {
	COLUMN_DISPLAY_NAME,
	COLUMN_BACKEND_NAME,
	COLUMN_DEFAULT,
	COLUMN_ENABLED,
	COLUMN_UID
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EMailAccountTreeView,
	e_mail_account_tree_view,
	GTK_TYPE_TREE_VIEW)

static gboolean
mail_account_tree_view_refresh_idle_cb (EMailAccountTreeView *tree_view)
{
	/* The refresh function will clear the idle ID. */
	e_mail_account_tree_view_refresh (tree_view);

	return FALSE;
}

static void
mail_account_tree_view_registry_changed (ESourceRegistry *registry,
                                         ESource *source,
                                         EMailAccountTreeView *tree_view)
{
	/* If the ESource in question has a "Mail Account" extension,
	 * schedule a refresh of the tree model.  Otherwise ignore it.
	 * We use an idle callback to limit how frequently we refresh
	 * the tree model, in case the registry is emitting lots of
	 * signals at once. */

	if (!SOURCE_IS_MAIL_ACCOUNT (source))
		return;

	if (tree_view->priv->refresh_idle_id > 0)
		return;

	tree_view->priv->refresh_idle_id = gdk_threads_add_idle (
		(GSourceFunc) mail_account_tree_view_refresh_idle_cb,
		tree_view);
}

static void
mail_account_tree_view_enabled_toggled_cb (GtkCellRendererToggle *cell_renderer,
                                           const gchar *path_string,
                                           EMailAccountTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;

	/* Chain the selection first so we enable or disable the
	 * correct account. */
	path = gtk_tree_path_new_from_string (path_string);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	if (gtk_cell_renderer_toggle_get_active (cell_renderer))
		e_mail_account_tree_view_disable_selected (tree_view);
	else
		e_mail_account_tree_view_enable_selected (tree_view);
}

static void
mail_account_tree_view_set_registry (EMailAccountTreeView *tree_view,
                                     ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (tree_view->priv->registry == NULL);

	tree_view->priv->registry = g_object_ref (registry);

	g_signal_connect (
		registry, "source-added",
		G_CALLBACK (mail_account_tree_view_registry_changed),
		tree_view);

	g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (mail_account_tree_view_registry_changed),
		tree_view);

	g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (mail_account_tree_view_registry_changed),
		tree_view);
}

static void
mail_account_tree_view_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			mail_account_tree_view_set_registry (
				E_MAIL_ACCOUNT_TREE_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_tree_view_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_account_tree_view_get_registry (
				E_MAIL_ACCOUNT_TREE_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_tree_view_dispose (GObject *object)
{
	EMailAccountTreeViewPrivate *priv;

	priv = E_MAIL_ACCOUNT_TREE_VIEW_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->registry, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->refresh_idle_id > 0) {
		g_source_remove (priv->refresh_idle_id);
		priv->refresh_idle_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_account_tree_view_parent_class)->
		dispose (object);
}

static void
mail_account_tree_view_constructed (GObject *object)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_account_tree_view_parent_class)->
		constructed (object);

	tree_view = GTK_TREE_VIEW (object);
	gtk_tree_view_set_headers_visible (tree_view, TRUE);

	/* Column: Enabled */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Enabled"));

	cell_renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	g_signal_connect (
		cell_renderer, "toggled",
		G_CALLBACK (mail_account_tree_view_enabled_toggled_cb),
		tree_view);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "active", COLUMN_ENABLED);

	gtk_tree_view_append_column (tree_view, column);

	/* Column: Account Name */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Account Name"));

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "text", COLUMN_DISPLAY_NAME);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "text", _("Default"), NULL);
	gtk_tree_view_column_pack_end (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "visible", COLUMN_DISPLAY_NAME);

	cell_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (
		cell_renderer, "icon-name", "emblem-default",
		"stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_column_pack_end (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "visible", COLUMN_DISPLAY_NAME);

	gtk_tree_view_append_column (tree_view, column);

	/* Column: Type */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Type"));

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "text", COLUMN_BACKEND_NAME);

	gtk_tree_view_append_column (tree_view, column);

	e_mail_account_tree_view_refresh (E_MAIL_ACCOUNT_TREE_VIEW (object));
}

static void
mail_account_tree_view_enable_selected (EMailAccountTreeView *tree_view)
{
	ESource *source;
	ESourceMailAccount *mail_account;
	const gchar *extension_name;
	GError *error = NULL;

	source = e_mail_account_tree_view_get_selected_source (tree_view);

	if (source == NULL)
		return;

	/* The source should already be a mail account. */
	g_return_if_fail (SOURCE_IS_MAIL_ACCOUNT (source));

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	mail_account = e_source_get_extension (source, extension_name);

	/* Avoid unnecessary signal emissions and disk writes. */
	if (e_source_mail_account_get_enabled (mail_account))
		return;

	e_source_mail_account_set_enabled (mail_account, TRUE);

	if (!e_source_sync (source, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
mail_account_tree_view_disable_selected (EMailAccountTreeView *tree_view)
{
	ESource *source;
	ESourceMailAccount *mail_account;
	const gchar *extension_name;
	GError *error = NULL;

	source = e_mail_account_tree_view_get_selected_source (tree_view);

	if (source == NULL)
		return;

	/* The source should already be a mail account. */
	g_return_if_fail (SOURCE_IS_MAIL_ACCOUNT (source));

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	mail_account = e_source_get_extension (source, extension_name);

	/* Avoid unnecessary signal emissions and disk writes. */
	if (!e_source_mail_account_get_enabled (mail_account))
		return;

	e_source_mail_account_set_enabled (mail_account, FALSE);

	if (!e_source_sync (source, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
e_mail_account_tree_view_class_init (EMailAccountTreeViewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailAccountTreeViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_account_tree_view_set_property;
	object_class->get_property = mail_account_tree_view_get_property;
	object_class->dispose = mail_account_tree_view_dispose;
	object_class->constructed = mail_account_tree_view_constructed;

	class->enable_selected = mail_account_tree_view_enable_selected;
	class->disable_selected = mail_account_tree_view_disable_selected;

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[ENABLE_SELECTED] = g_signal_new (
		"enable-selected",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountTreeViewClass, enable_selected),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[DISABLE_SELECTED] = g_signal_new (
		"disable-selected",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountTreeViewClass, disable_selected),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_account_tree_view_init (EMailAccountTreeView *tree_view)
{
	tree_view->priv = E_MAIL_ACCOUNT_TREE_VIEW_GET_PRIVATE (tree_view);
}

GtkWidget *
e_mail_account_tree_view_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_ACCOUNT_TREE_VIEW,
		"registry", registry, NULL);
}

void
e_mail_account_tree_view_refresh (EMailAccountTreeView *tree_view)
{
	ESourceRegistry *registry;
	GtkTreeModel *tree_model;
	ESource *default_source;
	ESource *source;
	GList *list, *link;
	const gchar *extension_name;
	gchar *saved_uid = NULL;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_TREE_VIEW (tree_view));

	if (tree_view->priv->refresh_idle_id > 0) {
		g_source_remove (tree_view->priv->refresh_idle_id);
		tree_view->priv->refresh_idle_id = 0;
	}

	registry = e_mail_account_tree_view_get_registry (tree_view);
	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

	source = e_mail_account_tree_view_get_selected_source (tree_view);
	if (source != NULL)
		saved_uid = g_strdup (e_source_get_uid (source));

	default_source = e_source_registry_get_default_mail_account (registry);

	gtk_list_store_clear (GTK_LIST_STORE (tree_model));

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESourceMailAccount *mail_account;
		GtkTreeIter iter;
		const gchar *backend_name;
		const gchar *display_name;
		const gchar *uid;
		gboolean is_default;
		gboolean is_enabled;

		source = E_SOURCE (link->data);
		mail_account = e_source_get_extension (source, extension_name);

		display_name = e_source_get_display_name (source);
		backend_name = e_source_get_backend_name (source);
		is_default = e_source_equal (source, default_source);
		is_enabled = e_source_mail_account_get_enabled (mail_account);
		uid = e_source_get_uid (source);

		gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			COLUMN_DISPLAY_NAME, display_name,
			COLUMN_BACKEND_NAME, backend_name,
			COLUMN_DEFAULT, is_default,
			COLUMN_ENABLED, is_enabled,
			COLUMN_UID, uid, -1);
	}

	g_list_free (list);

	/* Try and restore the previous selected source,
	 * or else just pick the default mail account. */

	source = NULL;

	if (saved_uid != NULL) {
		source = e_source_registry_lookup_by_uid (registry, saved_uid);
		g_free (saved_uid);
	}

	if (source == NULL)
		source = default_source;

	if (source != NULL)
		e_mail_account_tree_view_set_selected_source (
			tree_view, source);
}

void
e_mail_account_tree_view_enable_selected (EMailAccountTreeView *tree_view)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_TREE_VIEW (tree_view));

	g_signal_emit (tree_view, signals[ENABLE_SELECTED], 0);
}

void
e_mail_account_tree_view_disable_selected (EMailAccountTreeView *tree_view)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_TREE_VIEW (tree_view));

	g_signal_emit (tree_view, signals[DISABLE_SELECTED], 0);
}

ESourceRegistry *
e_mail_account_tree_view_get_registry (EMailAccountTreeView *tree_view)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_TREE_VIEW (tree_view), NULL);

	return tree_view->priv->registry;
}

ESource *
e_mail_account_tree_view_get_selected_source (EMailAccountTreeView *tree_view)
{
	ESourceRegistry *registry;
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	ESource *source;
	gchar *uid;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_TREE_VIEW (tree_view), NULL);

	registry = e_mail_account_tree_view_get_registry (tree_view);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	if (!gtk_tree_selection_get_selected (selection, &tree_model, &iter))
		return NULL;

	gtk_tree_model_get (tree_model, &iter, COLUMN_UID, &uid, -1);
	source = e_source_registry_lookup_by_uid (registry, uid);
	g_free (uid);

	return source;
}

void
e_mail_account_tree_view_set_selected_source (EMailAccountTreeView *tree_view,
                                              ESource *source)
{
	ESourceRegistry *registry;
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean valid;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_TREE_VIEW (tree_view));
	g_return_if_fail (E_IS_SOURCE (source));

	/* It is a programming error to pass an ESource that has no
	 * "Mail Account" extension. */
	g_return_if_fail (SOURCE_IS_MAIL_ACCOUNT (source));

	registry = e_mail_account_tree_view_get_registry (tree_view);
	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (valid) {
		ESource *candidate;
		gchar *uid;

		gtk_tree_model_get (tree_model, &iter, COLUMN_UID, &uid, -1);
		candidate = e_source_registry_lookup_by_uid (registry, uid);
		g_free (uid);

		if (candidate != NULL && e_source_equal (source, candidate)) {
			gtk_tree_selection_select_iter (selection, &iter);
			break;
		}

		valid = gtk_tree_model_iter_next (tree_model, &iter);
	}
}
