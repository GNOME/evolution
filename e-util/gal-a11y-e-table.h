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

#ifndef __GAL_A11Y_E_TABLE_H__
#define __GAL_A11Y_E_TABLE_H__

#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>
#include <atk/atkobject.h>
#include <atk/atkcomponent.h>

#define GAL_A11Y_TYPE_E_TABLE            (gal_a11y_e_table_get_type ())
#define GAL_A11Y_E_TABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE, GalA11yETable))
#define GAL_A11Y_E_TABLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE, GalA11yETableClass))
#define GAL_A11Y_IS_E_TABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE))
#define GAL_A11Y_IS_E_TABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE))

typedef struct _GalA11yETable GalA11yETable;
typedef struct _GalA11yETableClass GalA11yETableClass;
typedef struct _GalA11yETablePrivate GalA11yETablePrivate;

struct _GalA11yETable {
	GtkContainerAccessible object;
	GalA11yETablePrivate *priv;
};

struct _GalA11yETableClass {
	GtkContainerAccessibleClass parent_class;
};

/* Standard Glib function */
GType      gal_a11y_e_table_get_type  (void);
AtkObject *gal_a11y_e_table_new       (GObject *table);

#endif /* __GAL_A11Y_E_TABLE_H__ */
