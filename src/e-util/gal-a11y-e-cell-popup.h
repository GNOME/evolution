/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Yang Wu <yang.wu@sun.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __GAL_A11Y_E_CELL_POPUP_H__
#define __GAL_A11Y_E_CELL_POPUP_H__

#include <atk/atkgobjectaccessible.h>

#include <e-util/e-table-item.h>
#include <e-util/gal-a11y-e-cell.h>

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
					    gint         model_col,
					    gint         view_col,
					    gint         row);

#endif /* __GAL_A11Y_E_CELL_POPUP_H__ */
