/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __GAL_A11Y_E_CELL_TEXT_H__
#define __GAL_A11Y_E_CELL_TEXT_H__

#include <glib-object.h>
#include <e-util/e-table-item.h>
#include <e-util/e-cell-text.h>
#include <e-util/gal-a11y-e-cell.h>

#define GAL_A11Y_TYPE_E_CELL_TEXT            (gal_a11y_e_cell_text_get_type ())
#define GAL_A11Y_E_CELL_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL_TEXT, GalA11yECellText))
#define GAL_A11Y_E_CELL_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_CELL_TEXT, GalA11yECellTextClass))
#define GAL_A11Y_IS_E_CELL_TEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL_TEXT))
#define GAL_A11Y_IS_E_CELL_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL_TEXT))

typedef struct _GalA11yECellText GalA11yECellText;
typedef struct _GalA11yECellTextClass GalA11yECellTextClass;
typedef struct _GalA11yECellTextPrivate GalA11yECellTextPrivate;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yECellTextPrivate comes right after the parent class structure.
 **/
struct _GalA11yECellText {
	GalA11yECell object;
	gint inserted_id;
	gint deleted_id;
};

struct _GalA11yECellTextClass {
	GalA11yECellClass parent_class;
};

/* Standard Glib function */
GType      gal_a11y_e_cell_text_get_type   (void);
AtkObject *gal_a11y_e_cell_text_new        (ETableItem *item,
					    ECellView  *cell_view,
					    AtkObject  *parent,
					    gint         model_col,
					    gint         view_col,
					    gint         row);

#endif /* __GAL_A11Y_E_CELL_TEXT_H__ */
