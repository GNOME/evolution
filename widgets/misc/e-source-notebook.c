/*
 * e-source-notebook.c
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

#include "e-source-notebook.h"

#define E_SOURCE_NOTEBOOK_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SOURCE_NOTEBOOK, ESourceNotebookPrivate))

#define CHILD_SOURCE_KEY_FORMAT  "__e_source_notebook_%p_child_source__"

struct _ESourceNotebookPrivate {
	ESource *active_source;
	gchar *child_source_key;
};

enum {
	PROP_0,
	PROP_ACTIVE_SOURCE
};

enum {
	PROP_CHILD_0,
	PROP_CHILD_SOURCE
};

G_DEFINE_TYPE (
	ESourceNotebook,
	e_source_notebook,
	GTK_TYPE_NOTEBOOK)

static void
source_notebook_set_child_source (ESourceNotebook *notebook,
                                  GtkWidget *child,
                                  ESource *source)
{
	const gchar *key;

	key = notebook->priv->child_source_key;

	if (E_IS_SOURCE (source))
		g_object_set_data_full (
			G_OBJECT (child), key,
			g_object_ref (source),
			(GDestroyNotify) g_object_unref);
	else
		g_object_set_data (G_OBJECT (child), key, NULL);
}

static ESource *
source_notebook_get_child_source (ESourceNotebook *notebook,
                                  GtkWidget *child)
{
	const gchar *key;

	key = notebook->priv->child_source_key;

	return g_object_get_data (G_OBJECT (child), key);
}

static gboolean
source_notebook_page_num_to_source (GBinding *binding,
                                    const GValue *source_value,
                                    GValue *target_value,
                                    gpointer user_data)
{
	GtkNotebook *notebook;
	GtkWidget *child;
	ESource *source;
	gint page_num;

	/* The binding's source and target are the same instance. */
	notebook = GTK_NOTEBOOK (g_binding_get_source (binding));

	page_num = g_value_get_int (source_value);
	child = gtk_notebook_get_nth_page (notebook, page_num);

	if (child != NULL)
		source = source_notebook_get_child_source (
			E_SOURCE_NOTEBOOK (notebook), child);
	else
		source = NULL;

	g_value_set_object (target_value, source);

	return TRUE;
}

static gboolean
source_notebook_source_to_page_num (GBinding *binding,
                                    const GValue *source_value,
                                    GValue *target_value,
                                    gpointer user_data)
{
	GtkNotebook *notebook;
	ESource *source;
	gint n_pages, ii;

	/* The binding's source and target are the same instance. */
	notebook = GTK_NOTEBOOK (g_binding_get_source (binding));

	source = g_value_get_object (source_value);
	n_pages = gtk_notebook_get_n_pages (notebook);

	for (ii = 0; ii < n_pages; ii++) {
		GtkWidget *child;
		ESource *candidate;

		child = gtk_notebook_get_nth_page (notebook, ii);
		candidate = source_notebook_get_child_source (
			E_SOURCE_NOTEBOOK (notebook), child);

		if (e_source_equal (source, candidate)) {
			g_value_set_int (target_value, ii);
			return TRUE;
		}
	}

	return FALSE;
}

static void
source_notebook_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_SOURCE:
			e_source_notebook_set_active_source (
				E_SOURCE_NOTEBOOK (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_notebook_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_SOURCE:
			g_value_set_object (
				value,
				e_source_notebook_get_active_source (
				E_SOURCE_NOTEBOOK (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_notebook_dispose (GObject *object)
{
	ESourceNotebookPrivate *priv;

	priv = E_SOURCE_NOTEBOOK_GET_PRIVATE (object);

	if (priv->active_source != NULL) {
		g_object_unref (priv->active_source);
		priv->active_source = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_source_notebook_parent_class)->dispose (object);
}

static void
source_notebook_finalize (GObject *object)
{
	ESourceNotebookPrivate *priv;

	priv = E_SOURCE_NOTEBOOK_GET_PRIVATE (object);

	g_free (priv->child_source_key);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_source_notebook_parent_class)->finalize (object);
}

static void
source_notebook_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_source_notebook_parent_class)->constructed (object);

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (object), FALSE);

	/* Current page is still -1 so skip G_BINDING_SYNC_CREATE. */
	g_object_bind_property_full (
		object, "page",
		object, "active-source",
		G_BINDING_BIDIRECTIONAL,
		source_notebook_page_num_to_source,
		source_notebook_source_to_page_num,
		NULL, (GDestroyNotify) NULL);
}

static void
source_notebook_set_child_property (GtkContainer *container,
                                    GtkWidget *child,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHILD_SOURCE:
			source_notebook_set_child_source (
				E_SOURCE_NOTEBOOK (container),
				child, g_value_get_object (value));
			return;
	}

	GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (
		container, property_id, pspec);
}

static void
source_notebook_get_child_property (GtkContainer *container,
                                    GtkWidget *child,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHILD_SOURCE:
			g_value_set_object (
				value,
				source_notebook_get_child_source (
				E_SOURCE_NOTEBOOK (container), child));
			return;
	}

	GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (
		container, property_id, pspec);
}

static void
e_source_notebook_class_init (ESourceNotebookClass *class)
{
	GObjectClass *object_class;
	GtkContainerClass *container_class;

	g_type_class_add_private (class, sizeof (ESourceNotebookPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = source_notebook_set_property;
	object_class->get_property = source_notebook_get_property;
	object_class->dispose = source_notebook_dispose;
	object_class->finalize = source_notebook_finalize;
	object_class->constructed = source_notebook_constructed;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->set_child_property = source_notebook_set_child_property;
	container_class->get_child_property = source_notebook_get_child_property;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_SOURCE,
		g_param_spec_object (
			"active-source",
			"Active Source",
			"The data source for the current page",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/* Child property for notebook pages. */
	gtk_container_class_install_child_property (
		container_class,
		PROP_CHILD_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
			"The data source for this page",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_source_notebook_init (ESourceNotebook *notebook)
{
	gchar *key;

	notebook->priv = E_SOURCE_NOTEBOOK_GET_PRIVATE (notebook);

	key = g_strdup_printf (CHILD_SOURCE_KEY_FORMAT, notebook);
	notebook->priv->child_source_key = key;
}

GtkWidget *
e_source_notebook_new (void)
{
	return g_object_new (E_TYPE_SOURCE_NOTEBOOK, NULL);
}

gint
e_source_notebook_add_page (ESourceNotebook *notebook,
                            ESource *source,
                            GtkWidget *child)
{
	g_return_val_if_fail (E_IS_SOURCE_NOTEBOOK (notebook), -1);
	g_return_val_if_fail (E_IS_SOURCE (source), -1);
	g_return_val_if_fail (GTK_IS_WIDGET (child), -1);

	gtk_widget_show (child);
	source_notebook_set_child_source (notebook, child, source);

	return gtk_notebook_append_page (GTK_NOTEBOOK (notebook), child, NULL);
}

ESource *
e_source_notebook_get_active_source (ESourceNotebook *notebook)
{
	g_return_val_if_fail (E_IS_SOURCE_NOTEBOOK (notebook), NULL);

	return notebook->priv->active_source;
}

void
e_source_notebook_set_active_source (ESourceNotebook *notebook,
                                     ESource *source)
{
	g_return_if_fail (E_IS_SOURCE_NOTEBOOK (notebook));

	if (source != NULL) {
		g_return_if_fail (E_IS_SOURCE (source));
		g_object_ref (source);
	}

	if (notebook->priv->active_source != NULL)
		g_object_unref (notebook->priv->active_source);

	notebook->priv->active_source = source;

	g_object_notify (G_OBJECT (notebook), "active-source");
}

