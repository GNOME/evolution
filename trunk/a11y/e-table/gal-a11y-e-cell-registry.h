/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2001 Chris Lahey
 */

#ifndef __GAL_A11Y_E_CELL_REGISTRY_H__
#define __GAL_A11Y_E_CELL_REGISTRY_H__

#include <glib-object.h>
#include <atk/atkobject.h>
#include <table/e-table-item.h>
#include <table/e-cell.h>

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
						int         model_col,
						int         view_col,
						int         row);

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
						    int                       model_col,
						    int                       view_col,
						    int                       row);
void       gal_a11y_e_cell_registry_add_cell_type  (GalA11yECellRegistry     *registry,
						    GType                     type,
						    GalA11yECellRegistryFunc  func);

#endif /* ! __GAL_A11Y_E_CELL_REGISTRY_H__ */
