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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __GAL_A11Y_E_CELL_REGISTRY_H__
#define __GAL_A11Y_E_CELL_REGISTRY_H__

#include <atk/atkobject.h>

#include <e-util/e-table-item.h>
#include <e-util/e-cell.h>

#define GAL_A11Y_TYPE_E_CELL_REGISTRY            (gal_a11y_e_cell_registry_get_type ())
#define GAL_A11Y_E_CELL_REGISTRY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL_REGISTRY, GalA11yECellRegistry))
#define GAL_A11Y_E_CELL_REGISTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_CELL_REGISTRY, GalA11yECellRegistryClass))
#define GAL_A11Y_IS_E_CELL_REGISTRY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL_REGISTRY))
#define GAL_A11Y_IS_E_CELL_REGISTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL_REGISTRY))

typedef struct _GalA11yECellRegistry GalA11yECellRegistry;
typedef struct _GalA11yECellRegistryClass GalA11yECellRegistryClass;
typedef struct _GalA11yECellRegistryPrivate GalA11yECellRegistryPrivate;

typedef AtkObject *(*GalA11yECellRegistryFunc) (ETableItem *item,
						ECellView  *cell_view,
						AtkObject  *parent,
						gint         model_col,
						gint         view_col,
						gint         row);

struct _GalA11yECellRegistry {
	GObject object;

	GalA11yECellRegistryPrivate *priv;
};

struct _GalA11yECellRegistryClass {
	GObjectClass parent_class;
};

/* Standard Glib function */
GType      gal_a11y_e_cell_registry_get_type       (void);
AtkObject *gal_a11y_e_cell_registry_get_object     (GalA11yECellRegistry     *registry,
						    ETableItem               *item,
						    ECellView                *cell_view,
						    AtkObject                *parent,
						    gint                       model_col,
						    gint                       view_col,
						    gint                       row);
void       gal_a11y_e_cell_registry_add_cell_type  (GalA11yECellRegistry     *registry,
						    GType                     type,
						    GalA11yECellRegistryFunc  func);

#endif /* __GAL_A11Y_E_CELL_REGISTRY_H__ */
