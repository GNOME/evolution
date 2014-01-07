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
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __GAL_A11Y_E_TABLE_ITEM_H__
#define __GAL_A11Y_E_TABLE_ITEM_H__

#include <atk/atkgobjectaccessible.h>

#include <e-util/e-table-item.h>

#define GAL_A11Y_TYPE_E_TABLE_ITEM            (gal_a11y_e_table_item_get_type ())
#define GAL_A11Y_E_TABLE_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE_ITEM, GalA11yETableItem))
#define GAL_A11Y_E_TABLE_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE_ITEM, GalA11yETableItemClass))
#define GAL_A11Y_IS_E_TABLE_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE_ITEM))
#define GAL_A11Y_IS_E_TABLE_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE_ITEM))

typedef struct _GalA11yETableItem GalA11yETableItem;
typedef struct _GalA11yETableItemClass GalA11yETableItemClass;
typedef struct _GalA11yETableItemPrivate GalA11yETableItemPrivate;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yETableItemPrivate comes right after the parent class structure.
 **/
struct _GalA11yETableItem {
	AtkGObjectAccessible parent;
};

struct _GalA11yETableItemClass {
	AtkGObjectAccessibleClass parent_class;
};

/* Standard Glib function */
GType      gal_a11y_e_table_item_get_type  (void);
AtkObject *gal_a11y_e_table_item_new       (ETableItem *item);

void gal_a11y_e_table_item_init (void);

#endif /* __GAL_A11Y_E_TABLE_ITEM_H__ */
