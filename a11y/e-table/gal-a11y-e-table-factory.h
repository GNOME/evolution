/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2001 Chris Lahey
 */

#ifndef __GAL_A11Y_E_TABLE_FACTORY_H__
#define __GAL_A11Y_E_TABLE_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobjectfactory.h>

#define GAL_A11Y_TYPE_E_TABLE_FACTORY            (gal_a11y_e_table_factory_get_type ())
#define GAL_A11Y_E_TABLE_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE_FACTORY, GalA11yETableFactory))
#define GAL_A11Y_E_TABLE_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE_FACTORY, GalA11yETableFactoryClass))
#define GAL_A11Y_IS_E_TABLE_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE_FACTORY))
#define GAL_A11Y_IS_E_TABLE_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE_FACTORY))

typedef struct _GalA11yETableFactory GalA11yETableFactory;
typedef struct _GalA11yETableFactoryClass GalA11yETableFactoryClass;

struct _GalA11yETableFactory {
	AtkObject object;
};

struct _GalA11yETableFactoryClass {
	AtkObjectClass parent_class;
};


/* Standard Glib function */
GType              gal_a11y_e_table_factory_get_type         (void);

#endif /* ! __GAL_A11Y_E_TABLE_FACTORY_H__ */
