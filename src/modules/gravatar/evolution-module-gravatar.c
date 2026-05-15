/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gmodule.h>
#include <glib-object.h>

#include "e-gravatar-photo-source.h"
#include "e-photo-cache-gravatar-loader.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_gravatar_photo_source_type_register (type_module);
	e_photo_cache_gravatar_loader_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

