/*
 * e-conflict-search-selector.c
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

#include "e-conflict-search-selector.h"

#include "e-source-conflict-search.h"

G_DEFINE_TYPE (
	EConflictSearchSelector,
	e_conflict_search_selector,
	E_TYPE_SOURCE_SELECTOR)

static gboolean
conflict_search_selector_get_source_selected (ESourceSelector *selector,
                                           ESource *source)
{
	ESourceConflictSearch *extension;
	const gchar *extension_name;

	/* Make sure this source is a calendar. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_CONFLICT_SEARCH;
	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_CONFLICT_SEARCH (extension), FALSE);

	return e_source_conflict_search_get_include_me (extension);
}

static gboolean
conflict_search_selector_set_source_selected (ESourceSelector *selector,
                                           ESource *source,
                                           gboolean selected)
{
	ESourceConflictSearch *extension;
	const gchar *extension_name;

	/* Make sure this source is a calendar. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_CONFLICT_SEARCH;
	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_CONFLICT_SEARCH (extension), FALSE);

	if (selected != e_source_conflict_search_get_include_me (extension)) {
		e_source_conflict_search_set_include_me (extension, selected);
		e_source_selector_queue_write (selector, source);

		return TRUE;
	}

	return FALSE;
}

static gboolean
conflict_search_selector_filter_source_cb (ESourceSelector *selector,
					   ESource *source,
					   gpointer user_data)
{
	gboolean hidden = FALSE;

	if (E_IS_SOURCE (source) && (
	    g_strcmp0 (e_source_get_uid (source), "contacts-stub") == 0 ||
	    g_strcmp0 (e_source_get_uid (source), "birthdays") == 0)) {
		hidden = TRUE;
	}

	return hidden;
}

static void
e_conflict_search_selector_class_init (EConflictSearchSelectorClass *class)
{
	ESourceSelectorClass *source_selector_class;

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->get_source_selected =
				conflict_search_selector_get_source_selected;
	source_selector_class->set_source_selected =
				conflict_search_selector_set_source_selected;

	g_type_ensure (E_TYPE_SOURCE_CONFLICT_SEARCH);
}

static void
e_conflict_search_selector_init (EConflictSearchSelector *selector)
{
	g_signal_connect (selector, "filter-source",
		G_CALLBACK (conflict_search_selector_filter_source_cb), NULL);
}

GtkWidget *
e_conflict_search_selector_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_CONFLICT_SEARCH_SELECTOR,
		"extension-name", E_SOURCE_EXTENSION_CALENDAR,
		"registry", registry, NULL);
}
