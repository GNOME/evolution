/*
 * Authors: *   Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 2003 Ximian, Inc.
 */

#ifndef __GAL_A11Y_E_TABLE_CLICK_TO_ADD_FACTORY_H__
#define __GAL_A11Y_E_TABLE_CLICK_TO_ADD_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobjectfactory.h>

#define GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD_FACTORY            (gal_a11y_e_table_item_factory_get_type ())
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
