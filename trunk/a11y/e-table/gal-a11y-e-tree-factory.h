/*
 * Authors: Yuedong Du <yuedong.du@ximian.com>
 *
 * Copyright (C) 2003 Ximian, Inc.
 */

#ifndef __GAL_A11Y_E_TREE_FACTORY_H__
#define __GAL_A11Y_E_TREE_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobjectfactory.h>

#define GAL_A11Y_TYPE_E_TREE_FACTORY            (gal_a11y_e_table_factory_get_type ())
#define GAL_A11Y_E_TREE_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TREE_FACTORY, GalA11yETreeFactory))
#define GAL_A11Y_E_TREE_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TREE_FACTORY, GalA11yETreeFactoryClass))
#define GAL_A11Y_IS_E_TREE_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TREE_FACTORY))
#define GAL_A11Y_IS_E_TREE_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TREE_FACTORY))

typedef struct _GalA11yETreeFactory GalA11yETreeFactory;
typedef struct _GalA11yETreeFactoryClass GalA11yETreeFactoryClass;

struct _GalA11yETreeFactory {
	AtkObject object;
};

struct _GalA11yETreeFactoryClass {
	AtkObjectClass parent_class;
};


/* Standard Glib function */
GType              gal_a11y_e_tree_factory_get_type         (void);

#endif /* ! __GAL_A11Y_E_TREE_FACTORY_H__ */
