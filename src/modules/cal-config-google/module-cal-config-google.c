/*
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
