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

#ifndef __GAL_A11Y_E_TREE_H__
#define __GAL_A11Y_E_TREE_H__

#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>
#include <atk/atkobject.h>
#include <atk/atkcomponent.h>

#define GAL_A11Y_TYPE_E_TREE            (gal_a11y_e_tree_get_type ())
#define GAL_A11Y_E_TREE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TREE, GalA11yETree))
#define GAL_A11Y_E_TREE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TREE, GalA11yETreeClass))
#define GAL_A11Y_IS_E_TREE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TREE))
#define GAL_A11Y_IS_E_TREE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TREE))

typedef struct _GalA11yETree GalA11yETree;
typedef struct _GalA11yETreeClass GalA11yETreeClass;
typedef struct _GalA11yETreePrivate GalA11yETreePrivate;

struct _GalA11yETree {
	GtkContainerAccessible object;
	GalA11yETreePrivate *priv;
};

struct _GalA11yETreeClass {
	GtkContainerAccessibleClass parent_class;
};

/* Standard Glib function */
GType      gal_a11y_e_tree_get_type  (void);
AtkObject *gal_a11y_e_tree_new       (GObject *tree);

#endif /* __GAL_A11Y_E_TREE_H__ */
