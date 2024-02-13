/*
 * e-mail-config-service-backend.c
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

#include "e-mail-config-service-backend.h"

#include <mail/e-mail-config-receiving-page.h>
#include <mail/e-mail-config-sending-page.h>

struct _EMailConfigServiceBackendPrivate {
	ESource *source;
	ESource *collection;
};

enum {
	PROP_0,
	PROP_COLLECTION,
	PROP_SELECTABLE,
	PROP_SOURCE
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EMailConfigServiceBackend, e_mail_config_service_backend, E_TYPE_EXTENSION)

static void
mail_config_service_backend_init_collection (EMailConfigServiceBackend *backend)
{
	EMailConfigServiceBackendClass *class;

	/* Use the new_collection() method to initialize the "collection"
	 * property.  This assumes we're editing a new account.  If we're
	 * editing an existing account, the initial "collection" property
	 * value should be overridden with the existing collection source. */

	g_return_if_fail (backend->priv->collection == NULL);

	class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->new_collection != NULL);

	backend->priv->collection = class->new_collection (backend);
}

static void
mail_config_service_backend_set_property (GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COLLECTION:
			e_mail_config_service_backend_set_collection (
				E_MAIL_CONFIG_SERVICE_BACKEND (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			e_mail_config_service_backend_set_source (
				E_MAIL_CONFIG_SERVICE_BACKEND (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_service_backend_get_property (GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COLLECTION:
			g_value_set_object (
				value,
				e_mail_config_service_backend_get_collection (
				E_MAIL_CONFIG_SERVICE_BACKEND (object)));
			return;

		case PROP_SELECTABLE:
			g_value_set_boolean (
				value,
				e_mail_config_service_backend_get_selectable (
				E_MAIL_CONFIG_SERVICE_BACKEND (object)));
			return;

		case PROP_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_service_backend_get_source (
				E_MAIL_CONFIG_SERVICE_BACKEND (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_service_backend_dispose (GObject *object)
{
	EMailConfigServiceBackend *self = E_MAIL_CONFIG_SERVICE_BACKEND (object);

	g_clear_object (&self->priv->source);
	g_clear_object (&self->priv->collection);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_service_backend_parent_class)->dispose (object);
}

static void
mail_config_service_backend_constructed (GObject *object)
{
	EMailConfigServiceBackend *backend;

	backend = E_MAIL_CONFIG_SERVICE_BACKEND (object);
	mail_config_service_backend_init_collection (backend);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_service_backend_parent_class)->constructed (object);
}

static gboolean
mail_config_service_backend_get_selectable (EMailConfigServiceBackend *backend)
{
	EMailConfigServicePage *page;
	CamelProvider *provider;
	gboolean selectable = TRUE;

	page = e_mail_config_service_backend_get_page (backend);
	provider = e_mail_config_service_backend_get_provider (backend);

	if (CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider))
		selectable = E_IS_MAIL_CONFIG_RECEIVING_PAGE (page);

	return selectable;
}

static ESource *
mail_config_service_backend_new_collection (EMailConfigServiceBackend *backend)
{
	/* This is typically only used for groupware backends. */
	return NULL;
}

static void
mail_config_service_backend_insert_widgets (EMailConfigServiceBackend *backend,
                                            GtkBox *parent)
{
	/* does nothing */
}

static void
mail_config_service_backend_setup_defaults (EMailConfigServiceBackend *backend)
{
	/* does nothing */
}

static gboolean
mail_config_service_backend_auto_configure (EMailConfigServiceBackend *backend,
					    EConfigLookup *config_lookup,
					    gint *out_priority,
					    gboolean *out_is_complete)
{
	return FALSE;
}

static gboolean
mail_config_service_backend_check_complete (EMailConfigServiceBackend *backend)
{
	return TRUE;
}

static void
mail_config_service_backend_commit_changes (EMailConfigServiceBackend *backend)
{
	/* does nothing */
}

static void
e_mail_config_service_backend_class_init (EMailConfigServiceBackendClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_service_backend_set_property;
	object_class->get_property = mail_config_service_backend_get_property;
	object_class->dispose = mail_config_service_backend_dispose;
	object_class->constructed = mail_config_service_backend_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_CONFIG_SERVICE_PAGE;

	class->get_selectable = mail_config_service_backend_get_selectable;
	class->new_collection = mail_config_service_backend_new_collection;
	class->insert_widgets = mail_config_service_backend_insert_widgets;
	class->setup_defaults = mail_config_service_backend_setup_defaults;
	class->auto_configure = mail_config_service_backend_auto_configure;
	class->check_complete = mail_config_service_backend_check_complete;
	class->commit_changes = mail_config_service_backend_commit_changes;

	g_object_class_install_property (
		object_class,
		PROP_COLLECTION,
		g_param_spec_object (
			"collection",
			"Collection",
			"Optional collection ESource",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SELECTABLE,
		g_param_spec_boolean (
			"selectable",
			"Selectable",
			"Whether the backend is user selectable",
			TRUE,  /* not applied */
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
			"The ESource being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_service_backend_init (EMailConfigServiceBackend *backend)
{
	backend->priv = e_mail_config_service_backend_get_instance_private (backend);
}

EMailConfigServicePage *
e_mail_config_service_backend_get_page (EMailConfigServiceBackend *backend)
{
	EExtensible *extensible;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), NULL);

	extensible = e_extension_get_extensible (E_EXTENSION (backend));

	return E_MAIL_CONFIG_SERVICE_PAGE (extensible);
}

ESource *
e_mail_config_service_backend_get_source (EMailConfigServiceBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), NULL);

	return backend->priv->source;
}

void
e_mail_config_service_backend_set_source (EMailConfigServiceBackend *backend,
                                          ESource *source)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));

	if (backend->priv->source == source)
		return;

	if (source != NULL) {
		g_return_if_fail (E_IS_SOURCE (source));
		g_object_ref (source);
	}

	if (backend->priv->source != NULL)
		g_object_unref (backend->priv->source);

	backend->priv->source = source;

	g_object_notify (G_OBJECT (backend), "source");
}

ESource *
e_mail_config_service_backend_get_collection (EMailConfigServiceBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), NULL);

	return backend->priv->collection;
}

void
e_mail_config_service_backend_set_collection (EMailConfigServiceBackend *backend,
                                              ESource *collection)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));

	if (backend->priv->collection == collection)
		return;

	if (collection != NULL) {
		g_return_if_fail (E_IS_SOURCE (collection));
		g_object_ref (collection);
	}

	if (backend->priv->collection != NULL)
		g_object_unref (backend->priv->collection);

	backend->priv->collection = collection;

	g_object_notify (G_OBJECT (backend), "collection");
}

CamelProvider *
e_mail_config_service_backend_get_provider (EMailConfigServiceBackend *backend)
{
	EMailConfigServiceBackendClass *class;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), NULL);

	class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->backend_name != NULL, NULL);

	return camel_provider_get (class->backend_name, NULL);
}

CamelSettings *
e_mail_config_service_backend_get_settings (EMailConfigServiceBackend *backend)
{
	ESource *source;
	ESourceCamel *camel_extension = NULL;
	EMailConfigServicePage *page;
	EMailConfigServicePageClass *page_class;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), NULL);

	page = e_mail_config_service_backend_get_page (backend);
	page_class = E_MAIL_CONFIG_SERVICE_PAGE_GET_CLASS (page);

	/* Which ESource do we pull the CamelSettings from?  This is a
	 * little tricky because we have to handle the following cases:
	 *
	 *   1) A stand-alone mail account.
	 *
	 *   2) A collection with a specialized backend (e.g. ews).
	 *
	 *   3) A collection that uses standard backends (e.g. yahoo).
	 *
	 * So the semantics are as follows.  They work for now but may
	 * need further tweaking as we support more collection types.
	 *
	 *   1) If the service backend defines a collection source,
	 *      assume the CamelSettings will be pulled from there.
	 *
	 *   2) If we have a collection source, try extracting the
	 *      ESourceCamel extension for the collection source's
	 *      backend name.
	 *
	 *   3) If steps 1 or 2 fail, pull the CamelSettings from
	 *      the service backend's own scratch source.
	 */

	source = e_mail_config_service_backend_get_collection (backend);
	if (source != NULL) {
		ESourceBackend *backend_extension;
		const gchar *backend_name;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_COLLECTION;
		backend_extension =
			e_source_get_extension (source, extension_name);
		backend_name =
			e_source_backend_get_backend_name (backend_extension);

		/* XXX ESourceCollection's default backend name is "none".
		 *     Unfortunately so is CamelNullStore's provider name.
		 *     Make sure these two misfits don't get paired up! */
		if (g_strcmp0 (backend_name, "none") != 0) {
			extension_name =
				e_source_camel_get_extension_name (backend_name);
			camel_extension =
				e_source_get_extension (source, extension_name);
		}
	}

	if (camel_extension == NULL) {
		ESourceBackend *backend_extension;
		const gchar *backend_name;
		const gchar *extension_name;

		source = e_mail_config_service_backend_get_source (backend);

		extension_name = page_class->extension_name;
		backend_extension =
			e_source_get_extension (source, extension_name);
		backend_name =
			e_source_backend_get_backend_name (backend_extension);

		extension_name =
			e_source_camel_get_extension_name (backend_name);
		camel_extension =
			e_source_get_extension (source, extension_name);
	}

	return e_source_camel_get_settings (camel_extension);
}

gboolean
e_mail_config_service_backend_get_selectable (EMailConfigServiceBackend *backend)
{
	EMailConfigServiceBackendClass *class;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), FALSE);

	class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->get_selectable != NULL, FALSE);

	return class->get_selectable (backend);
}

void
e_mail_config_service_backend_insert_widgets (EMailConfigServiceBackend *backend,
                                              GtkBox *parent)
{
	EMailConfigServiceBackendClass *class;

	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));
	g_return_if_fail (GTK_IS_BOX (parent));

	class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->insert_widgets != NULL);

	class->insert_widgets (backend, parent);
}

void
e_mail_config_service_backend_setup_defaults (EMailConfigServiceBackend *backend)
{
	EMailConfigServiceBackendClass *class;

	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));

	class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->setup_defaults != NULL);

	return class->setup_defaults (backend);
}

gboolean
e_mail_config_service_backend_auto_configure (EMailConfigServiceBackend *backend,
					      EConfigLookup *config_lookup,
					      gint *out_priority,
					      gboolean *out_is_complete)
{
	EMailConfigServiceBackendClass *class;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), FALSE);
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), FALSE);

	class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->auto_configure != NULL, FALSE);

	return class->auto_configure (backend, config_lookup, out_priority, out_is_complete);
}

gboolean
e_mail_config_service_backend_check_complete (EMailConfigServiceBackend *backend)
{
	EMailConfigServiceBackendClass *class;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), FALSE);

	class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->check_complete != NULL, FALSE);

	return class->check_complete (backend);
}

void
e_mail_config_service_backend_commit_changes (EMailConfigServiceBackend *backend)
{
	EMailConfigServiceBackendClass *class;

	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));

	class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->commit_changes != NULL);

	class->commit_changes (backend);
}

/*
 * e_mail_config_service_backend_auto_configure_for_kind:
 * @backend: an #EMailConfigServiceBackend
 * @config_lookup: an #EConfigLookup
 * @kind: an #EConfigLookupResultKind
 * @protocol: (nullable): optional protocol name, or %NULL
 * @source: (nullable): optional #ESource to configure, or %NULL
 * @out_priority: (out) (nullable): priority of the chosen lookup result
 * @out_is_complete: (out) (nullable): whether the config is complete
 *
 * Finds a config lookup result for the given @kind and @protocol and
 * configures the @source with it. The @out_priority is set to the priority
 * of that lookup result.
 *
 * If no @protocol is given, then the backend name of the @backend is used.
 * If no @source is given, then gets it with e_mail_config_service_backend_get_source().
 *
 * Returns: whether applied any changes
 *
 * Since: 3.26
 */
gboolean
e_mail_config_service_backend_auto_configure_for_kind (EMailConfigServiceBackend *backend,
						       EConfigLookup *config_lookup,
						       EConfigLookupResultKind kind,
						       const gchar *protocol,
						       ESource *source,
						       gint *out_priority,
						       gboolean *out_is_complete)
{
	EMailConfigServiceBackendClass *klass;
	GSList *results;
	gboolean changed = FALSE;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), FALSE);
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), FALSE);
	g_return_val_if_fail (kind != E_CONFIG_LOOKUP_RESULT_UNKNOWN, FALSE);

	klass = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (klass != NULL, FALSE);
	g_return_val_if_fail (klass->backend_name != NULL, FALSE);

	if (!source)
		source = e_mail_config_service_backend_get_source (backend);
	if (!protocol)
		protocol = klass->backend_name;

	results = e_config_lookup_dup_results (config_lookup, kind, protocol);
	results = g_slist_sort (results, e_config_lookup_result_compare);

	if (results && results->data) {
		EConfigLookupResult *lookup_result = results->data;

		changed = e_config_lookup_result_configure_source (lookup_result, config_lookup, source);

		if (changed) {
			if (out_priority)
				*out_priority = e_config_lookup_result_get_priority (lookup_result);

			if (out_is_complete)
				*out_is_complete = e_config_lookup_result_get_is_complete (lookup_result);
		}
	}

	g_slist_free_full (results, g_object_unref);

	return changed;
}
