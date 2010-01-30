/*
 *
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
 *		Li Yuan <li.yuan@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __GAL_A11Y_E_TABLE_COLUMN_HEADER_H__
#define __GAL_A11Y_E_TABLE_COLUMN_HEADER_H__

#include <glib-object.h>
#include <atk/atkgobjectaccessible.h>

#define GAL_A11Y_TYPE_E_TABLE_COLUMN_HEADER            (gal_a11y_e_table_column_header_get_type ())
#define GAL_A11Y_E_TABLE_COLUMN_HEADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE_COLUMN_HEADER, GalA11yETableColumnHeader))
#define GAL_A11Y_E_TABLE_COLUMN_HEADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE_COLUMN_HEADER, GalA11yETableColumnHeaderClass))
#define GAL_A11Y_IS_E_TABLE_COLUMN_HEADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE_COLUMN_HEADER))
#define GAL_A11Y_IS_E_TABLE_COLUMN_HEADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE_COLUMN_HEADER))

typedef struct _GalA11yETableColumnHeader GalA11yETableColumnHeader;
typedef struct _GalA11yETableColumnHeaderClass GalA11yETableColumnHeaderClass;
typedef struct _GalA11yETableColumnHeaderPrivate GalA11yETableColumnHeaderPrivate;

struct _GalA11yETableColumnHeader {
	AtkGObjectAccessible parent;
};

struct _GalA11yETableColumnHeaderClass {
	AtkGObjectAccessibleClass parent_class;
};

/* Standard Glib function */
GType      gal_a11y_e_table_column_header_get_type  (void);
AtkObject *gal_a11y_e_table_column_header_new       (ETableCol *etc, ETableItem *item);
void gal_a11y_e_table_column_header_init (void);

#endif /* __GAL_A11Y_E_TABLE_COLUMN_HEADER_H__ */
