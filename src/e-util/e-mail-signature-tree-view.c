/*
 * e-mail-signature-tree-view.c
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

#include "e-mail-signature-tree-view.h"

#define SOURCE_IS_MAIL_SIGNATURE(source) \
	(e_source_has_extension ((source), E_SOURCE_EXTENSION_MAIL_SIGNATURE))

struct _EMailSignatureTreeViewPrivate {
	ESourceRegistry *registry;
	guint refresh_idle_id;
};

enum {
	PROP_0,
	PROP_REGISTRY
};

enum {
	COLUMN_DISPLAY_NAME,
	COLUMN_UID,
	NUM_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailSignatureTreeView, e_mail_signature_tree_view, GTK_TYPE_TREE_VIEW)

static gboolean
mail_signature_tree_view_refresh_idle_cb (EMailSignatureTreeView *tree_view)
{
	/* The refresh function will clear the idle ID. */
	e_mail_signature_tree_view_refresh (tree_view);

	return FALSE;
}

static void
mail_signature_tree_view_registry_changed (ESourceRegistry *registry,
                                           ESource *source,
                                           EMailSignatureTreeView *tree_view)
{
	/* If the ESource in question has a "Mail Signature" extension,
	 * schedule a refresh of the tree model.  Otherwise ignore it.
	 * We use an idle callback to limit how frequently we refresh
	 * the tree model, in case the registry is emitting lots of
	 * signals at once. */

	if (!SOURCE_IS_MAIL_SIGNATURE (source))
		return;

	if (tree_view->priv->refresh_idle_id > 0)
		return;

	tree_view->priv->refresh_idle_id = g_idle_add (
		(GSourceFunc) mail_signature_tree_view_refresh_idle_cb,
		tree_view);
}

static void
mail_signature_tree_view_set_registry (EMailSignatureTreeView *tree_view,
                                       ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (tree_view->priv->registry == NULL);

	tree_view->priv->registry = g_object_ref (registry);

	g_signal_connect (
		registry, "source-added",
		G_CALLBACK (mail_signature_tree_view_registry_changed),
		tree_view);

	g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (mail_signature_tree_view_registry_changed),
		tree_view);

	g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (mail_signature_tree_view_registry_changed),
		tree_view);
}

static void
mail_signature_tree_view_set_property (GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			mail_signature_tree_view_set_registry (
				E_MAIL_SIGNATURE_TREE_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_tree_view_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_signature_tree_view_get_registry (
				E_MAIL_SIGNATURE_TREE_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_tree_view_dispose (GObject *object)
{
	EMailSignatureTreeView *self = E_MAIL_SIGNATURE_TREE_VIEW (object);

	if (self->priv->registry != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->registry, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->registry);
	}

	if (self->priv->refresh_idle_id > 0) {
		g_source_remove (self->priv->refresh_idle_id);
		self->priv->refresh_idle_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_signature_tree_view_parent_class)->dispose (object);
}

static void
mail_signature_tree_view_constructed (GObject *object)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;
	GtkListStore *list_store;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_signature_tree_view_parent_class)->constructed (object);

	list_store = gtk_list_store_new (
		NUM_COLUMNS,
		G_TYPE_STRING,		/* COLUMN_DISPLAY_NAME */
		G_TYPE_STRING);		/* COLUMN_UID */

	tree_view = GTK_TREE_VIEW (object);
	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	g_object_unref (list_store);

	/* Column: Signature Name */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "text", COLUMN_DISPLAY_NAME);

	gtk_tree_view_append_column (tree_view, column);

	e_mail_signature_tree_view_refresh (
		E_MAIL_SIGNATURE_TREE_VIEW (object));
}

static void
e_mail_signature_tree_view_class_init (EMailSignatureTreeViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_signature_tree_view_set_property;
	object_class->get_property = mail_signature_tree_view_get_property;
	object_class->dispose = mail_signature_tree_view_dispose;
	object_class->constructed = mail_signature_tree_view_constructed;

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
}

static void
e_mail_signature_tree_view_init (EMailSignatureTreeView *tree_view)
{
	tree_view->priv = e_mail_signature_tree_view_get_instance_private (tree_view);
}

GtkWidget *
e_mail_signature_tree_view_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_SIGNATURE_TREE_VIEW,
		"registry", registry, NULL);
}

void
e_mail_signature_tree_view_refresh (EMailSignatureTreeView *tree_view)
{
	ESourceRegistry *registry;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	ESource *source;
	GList *list, *link;
	const gchar *extension_name;
	gchar *saved_uid = NULL;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_TREE_VIEW (tree_view));

	if (tree_view->priv->refresh_idle_id > 0) {
		g_source_remove (tree_view->priv->refresh_idle_id);
		tree_view->priv->refresh_idle_id = 0;
	}

	registry = e_mail_signature_tree_view_get_registry (tree_view);
	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	source = e_mail_signature_tree_view_ref_selected_source (tree_view);
	if (source != NULL) {
		saved_uid = e_source_dup_uid (source);
		g_object_unref (source);
	}

	gtk_list_store_clear (GTK_LIST_STORE (tree_model));

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreeIter iter;
		const gchar *display_name;
		const gchar *uid;

		source = E_SOURCE (link->data);
		display_name = e_source_get_display_name (source);
		uid = e_source_get_uid (source);

		gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			COLUMN_DISPLAY_NAME, display_name,
			COLUMN_UID, uid, -1);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Try and restore the previous selected source. */

	source = NULL;

	if (saved_uid != NULL) {
		source = e_source_registry_ref_source (registry, saved_uid);
		g_free (saved_uid);
	}

	if (source != NULL) {
		e_mail_signature_tree_view_set_selected_source (
			tree_view, source);
		g_object_unref (source);
	}

	/* Hint to refresh a signature preview. */
	g_signal_emit_by_name (selection, "changed");
}

ESourceRegistry *
e_mail_signature_tree_view_get_registry (EMailSignatureTreeView *tree_view)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_TREE_VIEW (tree_view), NULL);

	return tree_view->priv->registry;
}

ESource *
e_mail_signature_tree_view_ref_selected_source (EMailSignatureTreeView *tree_view)
{
	ESourceRegistry *registry;
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	ESource *source;
	gchar *uid;

	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_TREE_VIEW (tree_view), NULL);

	registry = e_mail_signature_tree_view_get_registry (tree_view);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	if (!gtk_tree_selection_get_selected (selection, &tree_model, &iter))
		return NULL;

	gtk_tree_model_get (tree_model, &iter, COLUMN_UID, &uid, -1);
	source = e_source_registry_ref_source (registry, uid);
	g_free (uid);

	return source;
}

void
e_mail_signature_tree_view_set_selected_source (EMailSignatureTreeView *tree_view,
                                                ESource *source)
{
	ESourceRegistry *registry;
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean valid;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_TREE_VIEW (tree_view));
	g_return_if_fail (E_IS_SOURCE (source));

	/* It is a programming error to pass an ESource that has no
	 * "Mail Signature" extension. */
	g_return_if_fail (SOURCE_IS_MAIL_SIGNATURE (source));

	registry = e_mail_signature_tree_view_get_registry (tree_view);
	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (valid) {
		ESource *candidate;
		gchar *uid;

		gtk_tree_model_get (tree_model, &iter, COLUMN_UID, &uid, -1);
		candidate = e_source_registry_ref_source (registry, uid);
		g_free (uid);

		if (candidate != NULL && e_source_equal (source, candidate)) {
			gtk_tree_selection_select_iter (selection, &iter);
			g_object_unref (candidate);
			break;
		}

		if (candidate != NULL)
			g_object_unref (candidate);

		valid = gtk_tree_model_iter_next (tree_model, &iter);
	}
}
