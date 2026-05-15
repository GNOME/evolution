/*
 * SPDX-FileCopyrightText: (C) 2017 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gmodule.h>
#include <glib-object.h>

#include "e-accounts-window-editors.h"
#include "e-collection-wizard-page.h"
#include "e-webdav-browser-page.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_accounts_window_editors_type_register (type_module);
	e_collection_wizard_page_type_register (type_module);
	e_webdav_browser_page_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
