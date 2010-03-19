/*
 * e-extensible.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include "e-extensible.h"

#include <e-util/e-util.h>
#include <e-util/e-extension.h>

static GQuark extensible_quark;

static GPtrArray *
extensible_get_extensions (EExtensible *extensible)
{
	return g_object_get_qdata (G_OBJECT (extensible), extensible_quark);
}

static void
extensible_load_extension (GType extension_type,
                           EExtensible *extensible)
{
	EExtensionClass *extension_class;
	GType extensible_type;
	GPtrArray *extensions;
	EExtension *extension;

	extensible_type = G_OBJECT_TYPE (extensible);
	extension_class = g_type_class_ref (extension_type);

	/* Only load extensions that extend the given extensible object. */
	if (!g_type_is_a (extensible_type, extension_class->extensible_type))
		goto exit;

	extension = g_object_new (
		extension_type, "extensible", extensible, NULL);

	extensions = extensible_get_extensions (extensible);
	g_ptr_array_add (extensions, extension);

exit:
	g_type_class_unref (extension_class);
}

static void
extensible_interface_init (EExtensibleInterface *interface)
{
	extensible_quark = g_quark_from_static_string ("e-extensible-quark");
}

GType
e_extensible_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EExtensibleInterface),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) extensible_interface_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			0,     /* instance_size */
			0,     /* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_INTERFACE, "EExtensible", &type_info, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

void
e_extensible_load_extensions (EExtensible *extensible)
{
	GPtrArray *extensions;

	g_return_if_fail (E_IS_EXTENSIBLE (extensible));

	if (extensible_get_extensions (extensible) != NULL)
		return;

	extensions = g_ptr_array_new_with_free_func (
		(GDestroyNotify) g_object_unref);

	g_object_set_qdata_full (
		G_OBJECT (extensible), extensible_quark,
		extensions, (GDestroyNotify) g_ptr_array_unref);

	e_type_traverse (
		E_TYPE_EXTENSION, (ETypeFunc)
		extensible_load_extension, extensible);
}
