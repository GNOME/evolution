/*
 * e-source-conflict-search.c
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

#include "e-source-conflict-search.h"

struct _ESourceConflictSearchPrivate {
	gboolean include_me;
};

enum {
	PROP_0,
	PROP_INCLUDE_ME
};

G_DEFINE_TYPE_WITH_PRIVATE (ESourceConflictSearch, e_source_conflict_search, E_TYPE_SOURCE_EXTENSION)

static void
source_conflict_search_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_INCLUDE_ME:
			e_source_conflict_search_set_include_me (
				E_SOURCE_CONFLICT_SEARCH (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_conflict_search_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_INCLUDE_ME:
			g_value_set_boolean (
				value,
				e_source_conflict_search_get_include_me (
				E_SOURCE_CONFLICT_SEARCH (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_source_conflict_search_class_init (ESourceConflictSearchClass *class)
{
	GObjectClass *object_class;
	ESourceExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = source_conflict_search_set_property;
	object_class->get_property = source_conflict_search_get_property;

	extension_class = E_SOURCE_EXTENSION_CLASS (class);
	extension_class->name = E_SOURCE_EXTENSION_CONFLICT_SEARCH;

	g_object_class_install_property (
		object_class,
		PROP_INCLUDE_ME,
		g_param_spec_boolean (
			"include-me",
			"IncludeMe",
			"Include this calendar in when "
			"searching for scheduling conflicts",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS |
			E_SOURCE_PARAM_SETTING));
}

static void
e_source_conflict_search_init (ESourceConflictSearch *extension)
{
	extension->priv = e_source_conflict_search_get_instance_private (extension);
}

/**
 * e_source_conflict_search_get_include_me:
 * @extension: an #ESourceConflictSearch
 *
 * Returns whether the calendar described by the #ESource to which
 * @extension belongs should be queried for scheduling conflicts when
 * negotiating a meeting invitation.
 *
 * Returns: whether to search for scheduling conflicts
 *
 * Since: 3.6
 **/
gboolean
e_source_conflict_search_get_include_me (ESourceConflictSearch *extension)
{
	g_return_val_if_fail (E_IS_SOURCE_CONFLICT_SEARCH (extension), FALSE);

	return extension->priv->include_me;
}

/**
 * e_source_conflict_search_set_include_me:
 * @extension: an #ESourceConflictSearch
 * @include_me: whether to search for scheduling conflicts
 *
 * Sets whether the calendar described by the #ESource to which @extension
 * belongs should be queried for scheduling conflicts when negotiating a
 * meeting invitation.
 *
 * Since: 3.6
 **/
void
e_source_conflict_search_set_include_me (ESourceConflictSearch *extension,
                                         gboolean include_me)
{
	g_return_if_fail (E_IS_SOURCE_CONFLICT_SEARCH (extension));

	if (extension->priv->include_me == include_me)
		return;

	extension->priv->include_me = include_me;

	g_object_notify (G_OBJECT (extension), "include-me");
}

