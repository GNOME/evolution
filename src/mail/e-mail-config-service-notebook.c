/*
 * e-mail-config-service-notebook.c
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

#include "evolution-config.h"

#include "e-mail-config-service-notebook.h"

#define CHILD_BACKEND_KEY_FORMAT \
	"__e_mail_config_service_notebook_%p_child_backend__"

struct _EMailConfigServiceNotebookPrivate {
	EMailConfigServiceBackend *active_backend;
	gchar *child_backend_key;
};

enum {
	PROP_0,
	PROP_ACTIVE_BACKEND
};

enum {
	PROP_CHILD_0,
	PROP_CHILD_BACKEND
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailConfigServiceNotebook, e_mail_config_service_notebook, GTK_TYPE_NOTEBOOK)

static void
mail_config_service_notebook_set_child_backend (EMailConfigServiceNotebook *notebook,
                                                GtkWidget *child,
                                                EMailConfigServiceBackend *backend)
{
	const gchar *key;

	key = notebook->priv->child_backend_key;

	if (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend))
		g_object_set_data_full (
			G_OBJECT (child), key,
			g_object_ref (backend),
			(GDestroyNotify) g_object_unref);
}

static EMailConfigServiceBackend *
mail_config_service_notebook_get_child_backend (EMailConfigServiceNotebook *notebook,
                                                GtkWidget *child)
{
	const gchar *key;

	key = notebook->priv->child_backend_key;

	return g_object_get_data (G_OBJECT (child), key);
}

static gboolean
mail_config_service_notebook_page_num_to_backend (GBinding *binding,
                                                  const GValue *source_value,
                                                  GValue *target_value,
                                                  gpointer user_data)
{
	EMailConfigServiceBackend *backend;
	GtkNotebook *notebook;
	GtkWidget *child;
	gint page_num;

	/* The binding's source and target are the same instance. */
	notebook = GTK_NOTEBOOK (g_binding_dup_source (binding));

	page_num = g_value_get_int (source_value);
	child = gtk_notebook_get_nth_page (notebook, page_num);

	if (child != NULL)
		backend = mail_config_service_notebook_get_child_backend (
			E_MAIL_CONFIG_SERVICE_NOTEBOOK (notebook), child);
	else
		backend = NULL;

	g_value_set_object (target_value, backend);
	g_clear_object (&notebook);

	return TRUE;
}

static gboolean
mail_config_service_notebook_backend_to_page_num (GBinding *binding,
                                                  const GValue *source_value,
                                                  GValue *target_value,
                                                  gpointer user_data)
{
	EMailConfigServiceBackend *backend;
	GtkNotebook *notebook;
	gint n_pages, ii;

	/* The binding's source and target are the same instance. */
	notebook = GTK_NOTEBOOK (g_binding_dup_source (binding));

	backend = g_value_get_object (source_value);
	n_pages = gtk_notebook_get_n_pages (notebook);

	for (ii = 0; ii < n_pages; ii++) {
		GtkWidget *child;
		EMailConfigServiceBackend *candidate;

		child = gtk_notebook_get_nth_page (notebook, ii);
		candidate = mail_config_service_notebook_get_child_backend (
			E_MAIL_CONFIG_SERVICE_NOTEBOOK (notebook), child);

		if (backend == candidate) {
			g_value_set_int (target_value, ii);
			g_clear_object (&notebook);
			return TRUE;
		}
	}

	g_clear_object (&notebook);

	return FALSE;
}

static void
mail_config_service_notebook_set_property (GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_BACKEND:
			e_mail_config_service_notebook_set_active_backend (
				E_MAIL_CONFIG_SERVICE_NOTEBOOK (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_service_notebook_get_property (GObject *object,
                                           guint property_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_BACKEND:
			g_value_set_object (
				value,
				e_mail_config_service_notebook_get_active_backend (
				E_MAIL_CONFIG_SERVICE_NOTEBOOK (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_service_notebook_dispose (GObject *object)
{
	EMailConfigServiceNotebook *self = E_MAIL_CONFIG_SERVICE_NOTEBOOK (object);

	g_clear_object (&self->priv->active_backend);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_service_notebook_parent_class)->dispose (object);
}

static void
mail_config_service_notebook_finalize (GObject *object)
{
	EMailConfigServiceNotebook *self = E_MAIL_CONFIG_SERVICE_NOTEBOOK (object);

	g_free (self->priv->child_backend_key);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_config_service_notebook_parent_class)->finalize (object);
}

static void
mail_config_service_notebook_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_service_notebook_parent_class)->constructed (object);

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (object), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (object), FALSE);

	/* Current page is still -1 so skip G_BINDING_SYNC_CREATE. */
	e_binding_bind_property_full (
		object, "page",
		object, "active-backend",
		G_BINDING_BIDIRECTIONAL,
		mail_config_service_notebook_page_num_to_backend,
		mail_config_service_notebook_backend_to_page_num,
		NULL, (GDestroyNotify) NULL);
}

static void
mail_config_service_notebook_set_child_property (GtkContainer *container,
                                                 GtkWidget *child,
                                                 guint property_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHILD_BACKEND:
			mail_config_service_notebook_set_child_backend (
				E_MAIL_CONFIG_SERVICE_NOTEBOOK (container),
				child, g_value_get_object (value));
			return;
	}

	GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (
		container, property_id, pspec);
}

static void
mail_config_service_notebook_get_child_property (GtkContainer *container,
                                                 GtkWidget *child,
                                                 guint property_id,
                                                 GValue *value,
                                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHILD_BACKEND:
			g_value_set_object (
				value,
				mail_config_service_notebook_get_child_backend (
				E_MAIL_CONFIG_SERVICE_NOTEBOOK (container), child));
			return;
	}

	GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (
		container, property_id, pspec);
}

static void
e_mail_config_service_notebook_class_init (EMailConfigServiceNotebookClass *class)
{
	GObjectClass *object_class;
	GtkContainerClass *container_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_service_notebook_set_property;
	object_class->get_property = mail_config_service_notebook_get_property;
	object_class->dispose = mail_config_service_notebook_dispose;
	object_class->finalize = mail_config_service_notebook_finalize;
	object_class->constructed = mail_config_service_notebook_constructed;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->set_child_property = mail_config_service_notebook_set_child_property;
	container_class->get_child_property = mail_config_service_notebook_get_child_property;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_BACKEND,
		g_param_spec_object (
			"active-backend",
			"Active Backend",
			"The service backend for the current page",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/* Child property for notebook pages. */
	gtk_container_class_install_child_property (
		container_class,
		PROP_CHILD_BACKEND,
		g_param_spec_object (
			"backend",
			"Backend",
			"The service backend for this page",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_service_notebook_init (EMailConfigServiceNotebook *notebook)
{
	gchar *key;

	notebook->priv = e_mail_config_service_notebook_get_instance_private (notebook);

	key = g_strdup_printf (CHILD_BACKEND_KEY_FORMAT, notebook);
	notebook->priv->child_backend_key = key;
}

GtkWidget *
e_mail_config_service_notebook_new (void)
{
	return g_object_new (E_TYPE_MAIL_CONFIG_SERVICE_NOTEBOOK, NULL);
}

gint
e_mail_config_service_notebook_add_page (EMailConfigServiceNotebook *notebook,
                                         EMailConfigServiceBackend *backend,
                                         GtkWidget *child)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_NOTEBOOK (notebook), -1);
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), -1);
	g_return_val_if_fail (GTK_IS_WIDGET (child), -1);

	gtk_widget_show (child);

	mail_config_service_notebook_set_child_backend (
		notebook, child, backend);

	return gtk_notebook_append_page (GTK_NOTEBOOK (notebook), child, NULL);
}

EMailConfigServiceBackend *
e_mail_config_service_notebook_get_active_backend (EMailConfigServiceNotebook *notebook)
{
	g_return_val_if_fail (
		E_IS_MAIL_CONFIG_SERVICE_NOTEBOOK (notebook), NULL);

	return notebook->priv->active_backend;
}

void
e_mail_config_service_notebook_set_active_backend (EMailConfigServiceNotebook *notebook,
                                                   EMailConfigServiceBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_NOTEBOOK (notebook));

	if (notebook->priv->active_backend == backend)
		return;

	if (backend != NULL) {
		g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));
		g_object_ref (backend);
	}

	if (notebook->priv->active_backend != NULL)
		g_object_unref (notebook->priv->active_backend);

	notebook->priv->active_backend = backend;

	g_object_notify (G_OBJECT (notebook), "active-backend");
}

