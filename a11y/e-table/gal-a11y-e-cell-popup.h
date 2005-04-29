/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: gal-a11y-e-cell-popup.h
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Yang Wu <yang.wu@sun.com> Sun Microsystem Inc., 2003
 *
 */

#ifndef __GAL_A11Y_E_CELL_POPUP_H__
#define __GAL_A11Y_E_CELL_POPUP_H__

#include <glib-object.h>
#include <gal/e-table/e-table-item.h>
#include <gal/a11y/e-table/gal-a11y-e-cell.h>
#include <atk/atkgobjectaccessible.h>

#define GAL_A11Y_TYPE_E_CELL_POPUP            (gal_a11y_e_cell_popup_get_type ())
#define GAL_A11Y_E_CELL_POPUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL_POPUP, GalA11yECellPopup))
#define GAL_A11Y_E_CELL_POPUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_CELL_POPUP, GalA11yECellPopupClass))
#define GAL_A11Y_IS_E_CELL_POPUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL_POPUP))
#define GAL_A11Y_IS_E_CELL_POPUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL_POPUP))

typedef struct _GalA11yECellPopup GalA11yECellPopup;
typedef struct _GalA11yECellPopupClass GalA11yECellPopupClass;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yECellPopupPrivate comes right after the parent class structure.
 **/
struct _GalA11yECellPopup {
	GalA11yECell object;
};

struct _GalA11yECellPopupClass {
	GalA11yECellClass parent_class;
};


/* Standard Glib function */
GType      gal_a11y_e_cell_popup_get_type   (void);
AtkObject *gal_a11y_e_cell_popup_new        (ETableItem *item,
					    ECellView  *cell_view,
					    AtkObject  *parent,
					    int         model_col,
					    int         view_col,
					    int         row);

#endif /* ! __GAL_A11Y_E_CELL_POPUP_H__ */
