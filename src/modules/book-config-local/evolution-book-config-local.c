/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

typedef ESourceConfigBackend EBookConfigLocal;
typedef ESourceConfigBackendClass EBookConfigLocalClass;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_book_config_local_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EBookConfigLocal,
	e_book_config_local,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static void
e_book_config_local_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_BOOK_SOURCE_CONFIG;

	class->parent_uid = "local-stub";
	class->backend_name = "local";
}

static void
e_book_config_local_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_book_config_local_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_book_config_local_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
