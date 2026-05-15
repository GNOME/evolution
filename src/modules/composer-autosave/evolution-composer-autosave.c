/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gmodule.h>
#include <glib-object.h>

#include "e-composer-autosave.h"
#include "e-composer-registry.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_composer_autosave_type_register (type_module);
	e_composer_registry_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
