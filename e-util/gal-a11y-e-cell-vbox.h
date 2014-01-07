/*
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
 *
 * Authors:
 *		Eric Zhao <eric.zhao@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2004 Sun Microsystem, Inc.
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __GAL_A11Y_E_CELL_VBOX_H__
#define __GAL_A11Y_E_CELL_VBOX_H__

#include "gal-a11y-e-cell.h"

G_BEGIN_DECLS

#define GAL_A11Y_TYPE_E_CELL_VBOX            (gal_a11y_e_cell_vbox_get_type ())
#define GAL_A11Y_E_CELL_VBOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL_VBOX, GalA11yECellVbox))
#define GAL_A11Y_E_CELL_VBOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_E_CELL_VBOX, GalA11yECellVboxClass))
#define GAL_A11Y_IS_E_CELL_VBOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL_VBOX))
#define GAL_A11Y_IS_E_CELL_VBOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL_VBOX))
#define GAL_A11Y_E_CELL_VBOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GAL_A11Y_TYPE_E_CELL_VBOX, GalA11yECellVboxClass))

typedef struct _GalA11yECellVbox	GalA11yECellVbox;
typedef struct _GalA11yECellVboxClass	GalA11yECellVboxClass;

struct _GalA11yECellVbox
{
	GalA11yECell	object;
	gint		a11y_subcell_count;
	gpointer       *a11y_subcells;
};

struct _GalA11yECellVboxClass
{
	GalA11yECellClass parent_class;
};

GType gal_a11y_e_cell_vbox_get_type	(void);
AtkObject *gal_a11y_e_cell_vbox_new	(ETableItem *item,
					 ECellView  *cell_view,
					 AtkObject  *parent,
					 gint         model_col,
					 gint         view_col,
					 gint         row);

G_END_DECLS
#endif /* __GAL_A11Y_E_CELL_VBOX_H__ */
