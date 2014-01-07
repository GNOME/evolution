/*
 * evolution-module-contact-photos.c
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

#include <gmodule.h>
#include <glib-object.h>

#include "e-contact-photo-source.h"
#include "e-photo-cache-contact-loader.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_contact_photo_source_type_register (type_module);
	e_photo_cache_contact_loader_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

