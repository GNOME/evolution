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
 *		Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __GAL_A11Y_E_TABLE_CLICK_TO_ADD_FACTORY_H__
#define __GAL_A11Y_E_TABLE_CLICK_TO_ADD_FACTORY_H__

#include <atk/atkobjectfactory.h>

#define GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD_FACTORY            (gal_a11y_e_table_item_click_to_add_factory_get_type ())
#define GAL_A11Y_E_TABLE_CLICK_TO_ADD_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD_FACTORY, GalA11yETableClickToAddFactory))
#define GAL_A11Y_E_TABLE_CLICK_TO_ADD_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD_FACTORY, GalA11yETableClickToAddFactoryClass))
#define GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD_FACTORY))
#define GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD_FACTORY))

typedef struct _GalA11yETableClickToAddFactory GalA11yETableClickToAddFactory;
typedef struct _GalA11yETableClickToAddFactoryClass GalA11yETableClickToAddFactoryClass;

struct _GalA11yETableClickToAddFactory {
	AtkObject object;
};

struct _GalA11yETableClickToAddFactoryClass {
	AtkObjectClass parent_class;
};

/* Standard Glib function */
GType              gal_a11y_e_table_click_to_add_factory_get_type (void);

#endif
