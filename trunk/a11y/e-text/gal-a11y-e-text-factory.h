/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2001 Chris Lahey
 */

#ifndef __GAL_A11Y_E_TEXT_FACTORY_H__
#define __GAL_A11Y_E_TEXT_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobjectfactory.h>

#define GAL_A11Y_TYPE_E_TEXT_FACTORY            (gal_a11y_e_text_factory_get_type ())
#define GAL_A11Y_E_TEXT_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TEXT_FACTORY, GalA11yETextFactory))
#define GAL_A11Y_E_TEXT_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TEXT_FACTORY, GalA11yETextFactoryClass))
#define GAL_A11Y_IS_E_TEXT_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TEXT_FACTORY))
#define GAL_A11Y_IS_E_TEXT_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TEXT_FACTORY))

typedef struct _GalA11yETextFactory GalA11yETextFactory;
typedef struct _GalA11yETextFactoryClass GalA11yETextFactoryClass;

struct _GalA11yETextFactory {
	AtkObjectFactory object;
};

struct _GalA11yETextFactoryClass {
	AtkObjectFactoryClass parent_class;
};


/* Standard Glib function */
GType              gal_a11y_e_text_factory_get_type         (void);

#endif /* ! __GAL_A11Y_E_TEXT_FACTORY_H__ */
