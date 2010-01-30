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
 *		Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __GAL_A11Y_E_TABLE_ITEM_FACTORY_H__
#define __GAL_A11Y_E_TABLE_ITEM_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobjectfactory.h>

#define GAL_A11Y_TYPE_E_TABLE_ITEM_FACTORY            (gal_a11y_e_table_item_factory_get_type ())
#define GAL_A11Y_E_TABLE_ITEM_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE_ITEM_FACTORY, GalA11yETableItemFactory))
#define GAL_A11Y_E_TABLE_ITEM_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE_ITEM_FACTORY, GalA11yETableItemFactoryClass))
#define GAL_A11Y_IS_E_TABLE_ITEM_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE_ITEM_FACTORY))
#define GAL_A11Y_IS_E_TABLE_ITEM_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE_ITEM_FACTORY))

typedef struct _GalA11yETableItemFactory GalA11yETableItemFactory;
typedef struct _GalA11yETableItemFactoryClass GalA11yETableItemFactoryClass;

struct _GalA11yETableItemFactory {
	AtkObject object;
};

struct _GalA11yETableItemFactoryClass {
	AtkObjectClass parent_class;
};

/* Standard Glib function */
GType              gal_a11y_e_table_item_factory_get_type         (void);

#endif /* __GAL_A11Y_E_TABLE_FACTORY_H__ */
