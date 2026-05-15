/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Christopher James Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __GAL_A11Y_E_TEXT_FACTORY_H__
#define __GAL_A11Y_E_TEXT_FACTORY_H__

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

#endif /* __GAL_A11Y_E_TEXT_FACTORY_H__ */
