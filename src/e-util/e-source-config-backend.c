/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "e-source-config-backend.h"

G_DEFINE_TYPE (
	ESourceConfigBackend,
	e_source_config_backend,
	E_TYPE_EXTENSION)

static gboolean
source_config_backend_allow_creation (ESourceConfigBackend *backend)
{
	return TRUE;
}

static void
source_config_backend_insert_widgets (ESourceConfigBackend *backend,
                                      ESource *scratch_source)
{
	/* does nothing */
}

static gboolean
source_config_backend_check_complete (ESourceConfigBackend *backend,
                                      ESource *scratch_source)
{
	return TRUE;
}

static void
source_config_backend_commit_changes (ESourceConfigBackend *backend,
                                      ESource *scratch_source)
{
	/* does nothing */
}

static void
e_source_config_backend_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SOURCE_CONFIG;

	class->allow_creation = source_config_backend_allow_creation;
	class->insert_widgets = source_config_backend_insert_widgets;
	class->check_complete = source_config_backend_check_complete;
	class->commit_changes = source_config_backend_commit_changes;
}

static void
e_source_config_backend_init (ESourceConfigBackend *backend)
{
}

ESourceConfig *
e_source_config_backend_get_config (ESourceConfigBackend *backend)
{
	EExtensible *extensible;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG_BACKEND (backend), NULL);

	extensible = e_extension_get_extensible (E_EXTENSION (backend));

	return E_SOURCE_CONFIG (extensible);
}

gboolean
e_source_config_backend_allow_creation (ESourceConfigBackend *backend)
{
	ESourceConfigBackendClass *class;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG_BACKEND (backend), FALSE);

	class = E_SOURCE_CONFIG_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->allow_creation != NULL, FALSE);

	return class->allow_creation (backend);
}

void
e_source_config_backend_insert_widgets (ESourceConfigBackend *backend,
                                        ESource *scratch_source)
{
	ESourceConfigBackendClass *class;

	g_return_if_fail (E_IS_SOURCE_CONFIG_BACKEND (backend));
	g_return_if_fail (E_IS_SOURCE (scratch_source));

	class = E_SOURCE_CONFIG_BACKEND_GET_CLASS (backend);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->insert_widgets != NULL);

	class->insert_widgets (backend, scratch_source);
}

gboolean
e_source_config_backend_check_complete (ESourceConfigBackend *backend,
                                        ESource *scratch_source)
{
	ESourceConfigBackendClass *class;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG_BACKEND (backend), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (scratch_source), FALSE);

	class = E_SOURCE_CONFIG_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->check_complete != NULL, FALSE);

	return class->check_complete (backend, scratch_source);
}

void
e_source_config_backend_commit_changes (ESourceConfigBackend *backend,
                                        ESource *scratch_source)
{
	ESourceConfigBackendClass *class;

	g_return_if_fail (E_IS_SOURCE_CONFIG_BACKEND (backend));
	g_return_if_fail (E_IS_SOURCE (scratch_source));

	class = E_SOURCE_CONFIG_BACKEND_GET_CLASS (backend);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->commit_changes != NULL);

	class->commit_changes (backend, scratch_source);
}
