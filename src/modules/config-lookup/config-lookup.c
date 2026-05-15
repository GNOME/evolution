/*
 * SPDX-FileCopyrightText: (C) 2017 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gmodule.h>
#include <glib-object.h>

#include "e-gnome-config-lookup.h"
#include "e-srv-config-lookup.h"
#include "e-webdav-config-lookup.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_gnome_config_lookup_type_register (type_module);
	e_srv_config_lookup_type_register (type_module);
	e_webdav_config_lookup_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
