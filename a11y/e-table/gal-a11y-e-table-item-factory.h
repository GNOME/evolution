/*
 * Authors: *   Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 2003 Ximian, Inc.
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

#endif /* ! __GAL_A11Y_E_TABLE_FACTORY_H__ */
