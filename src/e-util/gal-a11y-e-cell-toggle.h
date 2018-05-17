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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __GAL_A11Y_E_CELL_TOGGLE_H__
#define __GAL_A11Y_E_CELL_TOGGLE_H__

#include <atk/atk.h>
#include "gal-a11y-e-cell.h"

G_BEGIN_DECLS

#define GAL_A11Y_TYPE_E_CELL_TOGGLE            (gal_a11y_e_cell_toggle_get_type ())
#define GAL_A11Y_E_CELL_TOGGLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL_TOGGLE, GalA11yECellToggle))
#define GAL_A11Y_E_CELL_TOGGLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_E_CELL_TOGGLE, GalA11yECellToggleClass))
#define GAL_A11Y_IS_E_CELL_TOGGLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL_TOGGLE))
#define GAL_A11Y_IS_E_CELL_TOGGLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL_TOGGLE))
#define GAL_A11Y_E_CELL_TOGGLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GAL_A11Y_TYPE_E_CELL_TOGGLE, GalA11yECellToggleClass))

typedef struct _GalA11yECellToggle                  GalA11yECellToggle;
typedef struct _GalA11yECellToggleClass             GalA11yECellToggleClass;

struct _GalA11yECellToggle
{
  GalA11yECell parent;
  gint         model_id;
};

GType gal_a11y_e_cell_toggle_get_type (void);

struct _GalA11yECellToggleClass
{
  GalA11yECellClass parent_class;
};

AtkObject *gal_a11y_e_cell_toggle_new  (ETableItem *item,
                                        ECellView  *cell_view,
                                        AtkObject  *parent,
                                        gint         model_col,
                                        gint         view_col,
                                        gint         row);

G_END_DECLS

#endif /* __GAL_A11Y_E_CELL_TOGGLE_H__ */
