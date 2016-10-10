/*
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
 *
 * Authors:
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-cell.h"
#include "gal-a11y-e-cell-registry.h"

static GObjectClass *parent_class;
static GalA11yECellRegistry *default_registry;
#define PARENT_TYPE (G_TYPE_OBJECT)

struct _GalA11yECellRegistryPrivate {
	GHashTable *table;
};

/* Static functions */

static void
gal_a11y_e_cell_registry_finalize (GObject *obj)
{
	GalA11yECellRegistry *registry = GAL_A11Y_E_CELL_REGISTRY (obj);

	g_hash_table_destroy (registry->priv->table);
	g_free (registry->priv);

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gal_a11y_e_cell_registry_class_init (GalA11yECellRegistryClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->finalize = gal_a11y_e_cell_registry_finalize;
}

static void
gal_a11y_e_cell_registry_init (GalA11yECellRegistry *registry)
{
	registry->priv = g_new (GalA11yECellRegistryPrivate, 1);
	registry->priv->table = g_hash_table_new (NULL, NULL);
}

/**
 * gal_a11y_e_cell_registry_get_type:
 * @void:
 *
 * Registers the &GalA11yECellRegistry class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &GalA11yECellRegistry class.
 **/
GType
gal_a11y_e_cell_registry_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yECellRegistryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gal_a11y_e_cell_registry_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yECellRegistry),
			0,
			(GInstanceInitFunc) gal_a11y_e_cell_registry_init,
			NULL /* value_cell */
		};

		type = g_type_register_static (
			PARENT_TYPE, "GalA11yECellRegistry", &info, 0);
	}

	return type;
}

static void
init_default_registry (void)
{
	if (default_registry == NULL) {
		default_registry = g_object_new (gal_a11y_e_cell_registry_get_type (), NULL);
	}
}

AtkObject *
gal_a11y_e_cell_registry_get_object (GalA11yECellRegistry *registry,
                                     ETableItem *item,
                                     ECellView *cell_view,
                                     AtkObject *parent,
                                     gint model_col,
                                     gint view_col,
                                     gint row)
{
	GalA11yECellRegistryFunc func = NULL;
	GType type;

	if (registry == NULL) {
		init_default_registry ();
		registry = default_registry;
	}

	type = G_OBJECT_TYPE (cell_view->ecell);
	while (func == NULL && type != 0) {
		func = g_hash_table_lookup (registry->priv->table, GINT_TO_POINTER (type));
		type = g_type_parent (type);
	}

	if (func == NULL)
		func = gal_a11y_e_cell_new;

	return func (item, cell_view, parent, model_col, view_col, row);
}

void
gal_a11y_e_cell_registry_add_cell_type (GalA11yECellRegistry *registry,
                                        GType type,
                                        GalA11yECellRegistryFunc func)
{
	if (registry == NULL) {
		init_default_registry ();
		registry = default_registry;
	}

	g_hash_table_insert (registry->priv->table, GINT_TO_POINTER (type), func);
}
