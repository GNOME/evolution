/*
 * e-mail-config-notebook.c
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

#include <libebackend/libebackend.h>

#include "e-mail-config-composing-page.h"
#include "e-mail-config-defaults-page.h"
#include "e-mail-config-identity-page.h"
#include "e-mail-config-provider-page.h"
#include "e-mail-config-receiving-page.h"
#include "e-mail-config-security-page.h"
#include "e-mail-config-sending-page.h"

#include "e-mail-config-notebook.h"

typedef struct _AsyncContext AsyncContext;

struct _EMailConfigNotebookPrivate {
	EMailSession *session;
	ESource *original_source;
	ESource *account_source;
	ESource *identity_source;
	ESource *transport_source;
	ESource *collection_source;
};

struct _AsyncContext {
	GQueue *page_queue;
	GQueue *source_queue;
};

enum {
	PROP_0,
	PROP_ACCOUNT_SOURCE,
	PROP_COLLECTION_SOURCE,
	PROP_COMPLETE,
	PROP_IDENTITY_SOURCE,
	PROP_ORIGINAL_SOURCE,
	PROP_SESSION,
	PROP_TRANSPORT_SOURCE
};

G_DEFINE_TYPE_WITH_CODE (EMailConfigNotebook, e_mail_config_notebook, GTK_TYPE_NOTEBOOK,
	G_ADD_PRIVATE (EMailConfigNotebook)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
async_context_free (AsyncContext *async_context)
{
	g_queue_free_full (
		async_context->page_queue,
		(GDestroyNotify) g_object_unref);

	g_queue_free_full (
		async_context->source_queue,
		(GDestroyNotify) g_object_unref);

	g_free (async_context);
}

static void
mail_config_notebook_sort_pages (EMailConfigNotebook *notebook)
{
	GList *list, *link;
	gint ii = 0;

	list = g_list_sort (
		gtk_container_get_children (GTK_CONTAINER (notebook)),
		(GCompareFunc) e_mail_config_page_compare);

	for (link = list; link != NULL; link = g_list_next (link))
		gtk_notebook_reorder_child (
			GTK_NOTEBOOK (notebook),
			GTK_WIDGET (link->data), ii++);

	g_list_free (list);
}

static void
mail_config_notebook_page_changed (EMailConfigPage *page,
                                   EMailConfigNotebook *notebook)
{
	g_object_notify (G_OBJECT (notebook), "complete");
}

static void
mail_config_notebook_set_account_source (EMailConfigNotebook *notebook,
                                         ESource *account_source)
{
	g_return_if_fail (E_IS_SOURCE (account_source));
	g_return_if_fail (notebook->priv->account_source == NULL);

	notebook->priv->account_source = g_object_ref (account_source);
}

static void
mail_config_notebook_set_collection_source (EMailConfigNotebook *notebook,
                                            ESource *collection_source)
{
	g_return_if_fail (notebook->priv->collection_source == NULL);

	if (collection_source != NULL) {
		g_return_if_fail (E_IS_SOURCE (collection_source));
		g_object_ref (collection_source);
	}

	notebook->priv->collection_source = collection_source;
}

static void
mail_config_notebook_set_identity_source (EMailConfigNotebook *notebook,
                                          ESource *identity_source)
{
	g_return_if_fail (E_IS_SOURCE (identity_source));
	g_return_if_fail (notebook->priv->identity_source == NULL);

	notebook->priv->identity_source = g_object_ref (identity_source);
}

static void
mail_config_notebook_set_original_source (EMailConfigNotebook *notebook,
					  ESource *original_source)
{
	g_return_if_fail (notebook->priv->original_source == NULL);

	if (original_source != NULL) {
		g_return_if_fail (E_IS_SOURCE (original_source));
		g_object_ref (original_source);
	}

	notebook->priv->original_source = original_source;
}

static void
mail_config_notebook_set_session (EMailConfigNotebook *notebook,
                                  EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (notebook->priv->session == NULL);

	notebook->priv->session = g_object_ref (session);
}

static void
mail_config_notebook_set_transport_source (EMailConfigNotebook *notebook,
                                           ESource *transport_source)
{
	g_return_if_fail (E_IS_SOURCE (transport_source));
	g_return_if_fail (notebook->priv->transport_source == NULL);

	notebook->priv->transport_source = g_object_ref (transport_source);
}

static void
mail_config_notebook_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_SOURCE:
			mail_config_notebook_set_account_source (
				E_MAIL_CONFIG_NOTEBOOK (object),
				g_value_get_object (value));
			return;

		case PROP_COLLECTION_SOURCE:
			mail_config_notebook_set_collection_source (
				E_MAIL_CONFIG_NOTEBOOK (object),
				g_value_get_object (value));
			return;

		case PROP_IDENTITY_SOURCE:
			mail_config_notebook_set_identity_source (
				E_MAIL_CONFIG_NOTEBOOK (object),
				g_value_get_object (value));
			return;

		case PROP_ORIGINAL_SOURCE:
			mail_config_notebook_set_original_source (
				E_MAIL_CONFIG_NOTEBOOK (object),
				g_value_get_object (value));
			return;

		case PROP_SESSION:
			mail_config_notebook_set_session (
				E_MAIL_CONFIG_NOTEBOOK (object),
				g_value_get_object (value));
			return;

		case PROP_TRANSPORT_SOURCE:
			mail_config_notebook_set_transport_source (
				E_MAIL_CONFIG_NOTEBOOK (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_notebook_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_notebook_get_account_source (
				E_MAIL_CONFIG_NOTEBOOK (object)));
			return;

		case PROP_COLLECTION_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_notebook_get_collection_source (
				E_MAIL_CONFIG_NOTEBOOK (object)));
			return;

		case PROP_COMPLETE:
			g_value_set_boolean (
				value,
				e_mail_config_notebook_check_complete (
				E_MAIL_CONFIG_NOTEBOOK (object)));
			return;

		case PROP_IDENTITY_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_notebook_get_identity_source (
				E_MAIL_CONFIG_NOTEBOOK (object)));
			return;

		case PROP_ORIGINAL_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_notebook_get_original_source (
				E_MAIL_CONFIG_NOTEBOOK (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_config_notebook_get_session (
				E_MAIL_CONFIG_NOTEBOOK (object)));
			return;

		case PROP_TRANSPORT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_notebook_get_transport_source (
				E_MAIL_CONFIG_NOTEBOOK (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_notebook_dispose (GObject *object)
{
	EMailConfigNotebook *self = E_MAIL_CONFIG_NOTEBOOK (object);

	g_clear_object (&self->priv->session);
	g_clear_object (&self->priv->account_source);
	g_clear_object (&self->priv->identity_source);
	g_clear_object (&self->priv->transport_source);
	g_clear_object (&self->priv->collection_source);
	g_clear_object (&self->priv->original_source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_notebook_parent_class)->dispose (object);
}

static void
mail_config_notebook_constructed (GObject *object)
{
	EMailConfigNotebook *notebook;
	ESource *source;
	ESourceRegistry *registry;
	ESourceExtension *extension;
	ESourceMailIdentity *mail_identity_extension;
	EMailConfigServiceBackend *backend;
	CamelProvider *provider = NULL;
	EMailSession *session;
	EMailConfigPage *page;
	const gchar *extension_name;
	gboolean add_receiving_page = TRUE;
	gboolean add_sending_page = TRUE;
	gboolean add_transport_source;
	gboolean online_account = FALSE, goa_account = FALSE;

	notebook = E_MAIL_CONFIG_NOTEBOOK (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_notebook_parent_class)->constructed (object);

	session = e_mail_config_notebook_get_session (notebook);
	registry = e_mail_session_get_registry (session);

	source = notebook->priv->identity_source;
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	extension = e_source_get_extension (source, extension_name);
	mail_identity_extension = E_SOURCE_MAIL_IDENTITY (extension);

	/* If we have a collection source and the collection source
	 * has a [GNOME Online Accounts] or [Ubuntu Online Accounts]
	 * extension, skip the Receiving and Sending pages since GOA
	 * and UOA dictates those settings. */
	source = notebook->priv->collection_source;
	if (source != NULL) {
		extension_name = E_SOURCE_EXTENSION_GOA;
		if (e_source_has_extension (source, extension_name)) {
			online_account = TRUE;
			goa_account = TRUE;
			add_receiving_page = FALSE;
			add_sending_page = FALSE;
		}

		extension_name = E_SOURCE_EXTENSION_UOA;
		if (e_source_has_extension (source, extension_name)) {
			online_account = TRUE;
			add_receiving_page = FALSE;
			add_sending_page = FALSE;
		}
	}

	/* Keep all the display name properties synchronized.
	 * We consider the identity source's display name to
	 * be authoritative since technically that's the one
	 * shown on the Identity page. */

	e_binding_bind_property (
		notebook->priv->identity_source, "display-name",
		notebook->priv->account_source, "display-name",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		notebook->priv->identity_source, "display-name",
		notebook->priv->transport_source, "display-name",
		G_BINDING_SYNC_CREATE);

	if (notebook->priv->collection_source != NULL)
		e_binding_bind_property (
			notebook->priv->identity_source, "display-name",
			notebook->priv->collection_source, "display-name",
			G_BINDING_SYNC_CREATE);

	/*** Identity Page ***/

	page = e_mail_config_identity_page_new (
		registry, notebook->priv->identity_source);
	e_mail_config_identity_page_set_show_instructions (
		E_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);
	if (online_account) {
		e_mail_config_identity_page_set_show_account_info (
			E_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);
		e_mail_config_identity_page_set_show_email_address (
			E_MAIL_CONFIG_IDENTITY_PAGE (page), goa_account);
	}
	e_mail_config_notebook_add_page (notebook, page);

	/*** Receiving Page ***/

	page = e_mail_config_receiving_page_new (registry);
	backend = e_mail_config_service_page_add_scratch_source (
		E_MAIL_CONFIG_SERVICE_PAGE (page),
		notebook->priv->account_source,
		notebook->priv->collection_source);

	if (backend != NULL)
		provider = e_mail_config_service_backend_get_provider (backend);

	if (add_receiving_page)
		add_receiving_page = provider && g_strcmp0 (provider->protocol, "none") != 0;

	if (add_receiving_page) {
		e_mail_config_notebook_add_page (notebook, page);

		e_binding_bind_property (
			mail_identity_extension, "address",
			page, "email-address",
			G_BINDING_SYNC_CREATE);
	}

	/*** Receiving Options (conditional) ***/

	/* Note: We exclude this page if it has no options,
	 *       but we don't know that until we create it. */
	page = backend ? e_mail_config_provider_page_new (backend) : NULL;
	if (page && e_mail_config_provider_page_is_empty (
			E_MAIL_CONFIG_PROVIDER_PAGE (page))) {
		g_object_unref (g_object_ref_sink (page));
	} else if (page) {
		e_mail_config_notebook_add_page (notebook, page);
	}

	/*** Sending Page (conditional) ***/

	add_transport_source =
		provider != NULL &&
		!CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider);

	if ((add_transport_source || (provider && g_strcmp0 (provider->protocol, "none") == 0)) &&
	    notebook->priv->transport_source &&
	    e_source_has_extension (notebook->priv->transport_source, E_SOURCE_EXTENSION_MAIL_TRANSPORT)) {
		ESourceBackend *mail_transport;

		mail_transport = e_source_get_extension (notebook->priv->transport_source, E_SOURCE_EXTENSION_MAIL_TRANSPORT);

		e_source_extension_property_lock (E_SOURCE_EXTENSION (mail_transport));

		add_transport_source = g_strcmp0 (e_source_backend_get_backend_name (mail_transport), "none") != 0;

		e_source_extension_property_unlock (E_SOURCE_EXTENSION (mail_transport));
	}

	if (add_transport_source) {
		page = e_mail_config_sending_page_new (registry);
		e_mail_config_service_page_add_scratch_source (
			E_MAIL_CONFIG_SERVICE_PAGE (page),
			notebook->priv->transport_source,
			notebook->priv->collection_source);
		if (add_sending_page) {
			e_mail_config_notebook_add_page (notebook, page);

			e_binding_bind_property (
				mail_identity_extension, "address",
				page, "email-address",
				G_BINDING_SYNC_CREATE);
		}
	}

	/*** Defaults Page ***/

	page = e_mail_config_defaults_page_new (
		session,
		notebook->priv->original_source,
		notebook->priv->collection_source,
		notebook->priv->account_source,
		notebook->priv->identity_source,
		notebook->priv->transport_source);
	e_mail_config_notebook_add_page (notebook, page);

	/*** Composing Messages Page ***/

	page = e_mail_config_composing_page_new (notebook->priv->identity_source);
	e_mail_config_notebook_add_page (notebook, page);

	/*** Security Page ***/

	page = e_mail_config_security_page_new (
		notebook->priv->identity_source);
	e_mail_config_notebook_add_page (notebook, page);

	e_extensible_load_extensions (E_EXTENSIBLE (notebook));
}

static void
mail_config_notebook_page_removed (GtkNotebook *notebook,
                                   GtkWidget *child,
                                   guint page_num)
{
	/* Do not chain up.  GtkNotebook does not implement this method. */

	if (E_IS_MAIL_CONFIG_PAGE (child))
		g_signal_handlers_disconnect_by_func (
			child, mail_config_notebook_page_changed,
			E_MAIL_CONFIG_NOTEBOOK (notebook));
}

static void
mail_config_notebook_page_added (GtkNotebook *notebook,
                                 GtkWidget *child,
                                 guint page_num)
{
	/* Do not chain up.  GtkNotebook does not implement this method. */

	if (E_IS_MAIL_CONFIG_PAGE (child))
		g_signal_connect (
			child, "changed",
			G_CALLBACK (mail_config_notebook_page_changed),
			E_MAIL_CONFIG_NOTEBOOK (notebook));
}

static void
e_mail_config_notebook_class_init (EMailConfigNotebookClass *class)
{
	GObjectClass *object_class;
	GtkNotebookClass *notebook_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_notebook_set_property;
	object_class->get_property = mail_config_notebook_get_property;
	object_class->dispose = mail_config_notebook_dispose;
	object_class->constructed = mail_config_notebook_constructed;

	notebook_class = GTK_NOTEBOOK_CLASS (class);
	notebook_class->page_removed = mail_config_notebook_page_removed;
	notebook_class->page_added = mail_config_notebook_page_added;

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_SOURCE,
		g_param_spec_object (
			"account-source",
			"Account Source",
			"Mail account source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_COLLECTION_SOURCE,
		g_param_spec_object (
			"collection-source",
			"Collection Source",
			"Optional collection source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_COMPLETE,
		g_param_spec_boolean (
			"complete",
			"Complete",
			"Whether all required fields are complete",
			FALSE,  /* default is not used */
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_SOURCE,
		g_param_spec_object (
			"identity-source",
			"Identity Source",
			"Mail identity source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ORIGINAL_SOURCE,
		g_param_spec_object (
			"original-source",
			"Original Source",
			"Mail account original source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			"Session",
			"Mail session",
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_SOURCE,
		g_param_spec_object (
			"transport-source",
			"Transport Source",
			"Mail transport source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_notebook_init (EMailConfigNotebook *notebook)
{
	notebook->priv = e_mail_config_notebook_get_instance_private (notebook);
}

GtkWidget *
e_mail_config_notebook_new (EMailSession *session,
                            ESource *original_source,
                            ESource *account_source,
                            ESource *identity_source,
                            ESource *transport_source,
                            ESource *collection_source)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (E_IS_SOURCE (account_source), NULL);
	g_return_val_if_fail (E_IS_SOURCE (identity_source), NULL);
	g_return_val_if_fail (E_IS_SOURCE (transport_source), NULL);

	/* A collection source is optional. */
	if (collection_source != NULL)
		g_return_val_if_fail (E_IS_SOURCE (collection_source), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_NOTEBOOK,
		"session", session,
		"original-source", original_source,
		"account-source", account_source,
		"identity-source", identity_source,
		"transport-source", transport_source,
		"collection-source", collection_source,
		NULL);
}

EMailSession *
e_mail_config_notebook_get_session (EMailConfigNotebook *notebook)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook), NULL);

	return notebook->priv->session;
}

ESource *
e_mail_config_notebook_get_original_source (EMailConfigNotebook *notebook)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook), NULL);

	return notebook->priv->original_source;
}

ESource *
e_mail_config_notebook_get_account_source (EMailConfigNotebook *notebook)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook), NULL);

	return notebook->priv->account_source;
}

ESource *
e_mail_config_notebook_get_identity_source (EMailConfigNotebook *notebook)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook), NULL);

	return notebook->priv->identity_source;
}

ESource *
e_mail_config_notebook_get_transport_source (EMailConfigNotebook *notebook)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook), NULL);

	return notebook->priv->transport_source;
}

ESource *
e_mail_config_notebook_get_collection_source (EMailConfigNotebook *notebook)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook), NULL);

	return notebook->priv->collection_source;
}

void
e_mail_config_notebook_add_page (EMailConfigNotebook *notebook,
                                 EMailConfigPage *page)
{
	EMailConfigPageInterface *page_interface;
	GtkWidget *tab_label;

	g_return_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook));
	g_return_if_fail (E_IS_MAIL_CONFIG_PAGE (page));

	page_interface = E_MAIL_CONFIG_PAGE_GET_INTERFACE (page);
	tab_label = gtk_label_new (page_interface->title);

	gtk_widget_show (GTK_WIDGET (page));

	gtk_notebook_append_page (
		GTK_NOTEBOOK (notebook),
		GTK_WIDGET (page), tab_label);

	mail_config_notebook_sort_pages (notebook);
}

gboolean
e_mail_config_notebook_check_complete (EMailConfigNotebook *notebook)
{
	GList *list, *link;
	gboolean complete = TRUE;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook), FALSE);

	list = gtk_container_get_children (GTK_CONTAINER (notebook));

	for (link = list; link != NULL; link = g_list_next (link)) {
		if (E_IS_MAIL_CONFIG_PAGE (link->data)) {
			EMailConfigPage *page;
			page = E_MAIL_CONFIG_PAGE (link->data);
			complete = e_mail_config_page_check_complete (page);

			if (!complete)
				break;
		}
	}

	g_list_free (list);

	return complete;
}

/********************** e_mail_config_notebook_commit() **********************/

static void
mail_config_notebook_page_submit_cb (GObject *source_object,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
	GTask *task;
	AsyncContext *async_context;
	EMailConfigPage *next_page;
	GError *error = NULL;

	task = G_TASK (user_data);
	async_context = g_task_get_task_data (task);

	e_mail_config_page_submit_finish (
		E_MAIL_CONFIG_PAGE (source_object), result, &error);

	if (error != NULL) {
		g_task_return_error (task, g_steal_pointer (&error));
		g_object_unref (task);
		return;
	}

	next_page = g_queue_pop_head (async_context->page_queue);

	/* Submit the next EMailConfigPage. */
	if (next_page != NULL) {
		GCancellable *cancellable = g_task_get_cancellable (task);
		e_mail_config_page_submit (
			next_page, cancellable,
			mail_config_notebook_page_submit_cb, g_steal_pointer (&task));

		g_object_unref (next_page);

	/* All done! */
	} else {
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
	}
}

static void
mail_config_notebook_source_commit_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	ESourceRegistry *registry;
	GTask *task;
	GCancellable *cancellable;
	AsyncContext *async_context;
	ESource *next_source;
	GError *error = NULL;

	registry = E_SOURCE_REGISTRY (source_object);
	task = G_TASK (user_data);

	e_source_registry_commit_source_finish (registry, result, &error);

	if (error != NULL) {
		g_task_return_error (task, g_steal_pointer (&error));
		g_object_unref (task);
		return;
	}

	cancellable = g_task_get_cancellable (task);
	async_context = g_task_get_task_data (task);
	next_source = g_queue_pop_head (async_context->source_queue);

	/* Commit the next ESources. */
	if (next_source != NULL) {
		e_source_registry_commit_source (
			registry, next_source, cancellable,
			mail_config_notebook_source_commit_cb, g_steal_pointer (&task));

		g_object_unref (next_source);

	/* ESources done, start on the EMailConfigPages. */
	} else {
		EMailConfigPage *page;

		/* There should be at least one page,
		 * so we can skip the NULL check here. */
		page = g_queue_pop_head (async_context->page_queue);

		e_mail_config_page_submit (
			page, cancellable,
			mail_config_notebook_page_submit_cb, g_steal_pointer (&task));

		g_object_unref (page);
	}
}

void
e_mail_config_notebook_commit (EMailConfigNotebook *notebook,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GTask *task;
	AsyncContext *async_context;
	ESourceRegistry *registry;
	EMailSession *session;
	ESource *source;
	GList *list, *link;
	GQueue *page_queue;
	GQueue *source_queue;

	g_return_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook));

	session = e_mail_config_notebook_get_session (notebook);
	registry = e_mail_session_get_registry (session);

	page_queue = g_queue_new ();
	source_queue = g_queue_new ();

	/* Queue the collection data source if one is defined. */
	source = e_mail_config_notebook_get_collection_source (notebook);
	if (source != NULL && e_source_get_writable (source))
		g_queue_push_tail (source_queue, g_object_ref (source));

	/* Queue the mail-related data sources for the account. */
	source = e_mail_config_notebook_get_account_source (notebook);
	if (source != NULL && e_source_get_writable (source))
		g_queue_push_tail (source_queue, g_object_ref (source));
	source = e_mail_config_notebook_get_identity_source (notebook);
	if (source != NULL && e_source_get_writable (source))
		g_queue_push_tail (source_queue, g_object_ref (source));
	source = e_mail_config_notebook_get_transport_source (notebook);
	if (source != NULL && e_source_get_writable (source))
		g_queue_push_tail (source_queue, g_object_ref (source));

	list = gtk_container_get_children (GTK_CONTAINER (notebook));

	/* Tell all EMailConfigPages to commit their UI state to their
	 * scratch ESources and push any additional data sources on to
	 * the given source queue, such as calendars or address books
	 * to be bundled with the mail account. */
	for (link = list; link != NULL; link = g_list_next (link)) {
		if (E_IS_MAIL_CONFIG_PAGE (link->data)) {
			EMailConfigPage *page;
			page = E_MAIL_CONFIG_PAGE (link->data);
			g_queue_push_tail (page_queue, g_object_ref (page));
			e_mail_config_page_commit_changes (page, source_queue);
		}
	}

	g_list_free (list);

	async_context = g_new0 (AsyncContext, 1);
	async_context->page_queue = g_steal_pointer (&page_queue);
	async_context->source_queue = g_steal_pointer (&source_queue);

	task = g_task_new (notebook, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_config_notebook_commit);
	g_task_set_task_data (task, async_context, (GDestroyNotify) async_context_free);

	source = g_queue_pop_head (async_context->source_queue);
	g_return_if_fail (E_IS_SOURCE (source));

	e_source_registry_commit_source (
		registry, source, cancellable,
		mail_config_notebook_source_commit_cb, g_steal_pointer (&task));

	g_object_unref (source);
}

gboolean
e_mail_config_notebook_commit_finish (EMailConfigNotebook *notebook,
                                      GAsyncResult *result,
                                      GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, notebook), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_config_notebook_commit), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

