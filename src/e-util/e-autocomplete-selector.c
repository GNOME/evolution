/*
 * e-autocomplete-selector.c
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

#include "e-autocomplete-selector.h"

G_DEFINE_TYPE (
	EAutocompleteSelector,
	e_autocomplete_selector,
	E_TYPE_SOURCE_SELECTOR)

static gboolean
autocomplete_selector_get_source_selected (ESourceSelector *selector,
                                           ESource *source)
{
	ESourceAutocomplete *extension;
	const gchar *extension_name;

	/* Make sure this source is an address book. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_AUTOCOMPLETE;
	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_AUTOCOMPLETE (extension), FALSE);

	return e_source_autocomplete_get_include_me (extension);
}

static gboolean
autocomplete_selector_set_source_selected (ESourceSelector *selector,
                                           ESource *source,
                                           gboolean selected)
{
	ESourceAutocomplete *extension;
	const gchar *extension_name;

	/* Make sure this source is an address book. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_AUTOCOMPLETE;
	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_AUTOCOMPLETE (extension), FALSE);

	if (selected != e_source_autocomplete_get_include_me (extension)) {
		e_source_autocomplete_set_include_me (extension, selected);
		e_source_selector_queue_write (selector, source);

		return TRUE;
	}

	return FALSE;
}

static void
e_autocomplete_selector_class_init (EAutocompleteSelectorClass *class)
{
	ESourceSelectorClass *source_selector_class;

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->get_source_selected =
				autocomplete_selector_get_source_selected;
	source_selector_class->set_source_selected =
				autocomplete_selector_set_source_selected;
}

static void
e_autocomplete_selector_init (EAutocompleteSelector *selector)
{
	e_source_selector_set_show_colors (
		E_SOURCE_SELECTOR (selector), FALSE);
	e_source_selector_set_show_icons (
		E_SOURCE_SELECTOR (selector), TRUE);
}

GtkWidget *
e_autocomplete_selector_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_AUTOCOMPLETE_SELECTOR,
		"extension-name", E_SOURCE_EXTENSION_ADDRESS_BOOK,
		"registry", registry,
		"show-icons", FALSE,
		NULL);
}
