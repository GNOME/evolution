/*
 * e-contacts-selector.c
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

#include "evolution-config.h"

#include <libedataserver/libedataserver.h>

#include "e-contacts-selector.h"

G_DEFINE_DYNAMIC_TYPE (
	EContactsSelector,
	e_contacts_selector,
	E_TYPE_SOURCE_SELECTOR)

static gboolean
contacts_selector_get_source_selected (ESourceSelector *selector,
                                       ESource *source)
{
	ESourceContacts *extension;
	const gchar *extension_name;

	extension_name = e_source_selector_get_extension_name (selector);

	/* Make sure this source is an address book. */
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_CONTACTS_BACKEND;
	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_CONTACTS (extension), FALSE);

	return e_source_contacts_get_include_me (extension);
}

static gboolean
contacts_selector_set_source_selected (ESourceSelector *selector,
                                       ESource *source,
                                       gboolean selected)
{
	ESourceContacts *extension;
	const gchar *extension_name;

	/* Make sure this source is an address book. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_CONTACTS_BACKEND;
	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_CONTACTS (extension), FALSE);

	if (selected != e_source_contacts_get_include_me (extension)) {
		e_source_contacts_set_include_me (extension, selected);
		e_source_selector_queue_write (selector, source);

		return TRUE;
	}

	return FALSE;
}

static void
e_contacts_selector_class_init (EContactsSelectorClass *class)
{
	ESourceSelectorClass *source_selector_class;

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->get_source_selected =
				contacts_selector_get_source_selected;
	source_selector_class->set_source_selected =
				contacts_selector_set_source_selected;
}

static void
e_contacts_selector_class_finalize (EContactsSelectorClass *class)
{
}

static void
e_contacts_selector_init (EContactsSelector *selector)
{
	e_source_selector_set_show_colors (
		E_SOURCE_SELECTOR (selector), FALSE);
	e_source_selector_set_show_icons (
		E_SOURCE_SELECTOR (selector), TRUE);
}

void
e_contacts_selector_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_contacts_selector_register_type (type_module);
}

GtkWidget *
e_contacts_selector_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_CONTACTS_SELECTOR,
		"extension-name", E_SOURCE_EXTENSION_ADDRESS_BOOK,
		"registry", registry, NULL);
}
