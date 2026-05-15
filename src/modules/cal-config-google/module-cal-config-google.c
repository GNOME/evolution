/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <libebackend/libebackend.h>

#include "e-google-chooser-button.h"
#include "module-cal-config-google.h"

gboolean
e_module_cal_config_google_is_supported (ESourceConfigBackend *backend,
					 ESourceRegistry *registry)
{
	if (!backend && !registry)
		return FALSE;

	if (!registry)
		registry = e_source_config_get_registry (e_source_config_backend_get_config (backend));

	return registry && e_oauth2_services_is_oauth2_alias (e_source_registry_get_oauth2_services (registry), "Google");
}

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_google_chooser_button_type_register (type_module);
	e_cal_config_google_type_register (type_module);
	e_cal_config_gtasks_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
