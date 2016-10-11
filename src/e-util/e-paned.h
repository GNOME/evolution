/*
 * e-paned.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_PANED_H
#define E_PANED_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_PANED \
	(e_paned_get_type ())
#define E_PANED(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PANED, EPaned))
#define E_PANED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PANED, EPanedClass))
#define E_IS_PANED(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PANED))
#define E_IS_PANED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PANED))
#define E_PANED_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PANED, EPanedClass))

G_BEGIN_DECLS

typedef struct _EPaned EPaned;
typedef struct _EPanedClass EPanedClass;
typedef struct _EPanedPrivate EPanedPrivate;

struct _EPaned {
	GtkPaned parent;
	EPanedPrivate *priv;
};

struct _EPanedClass {
	GtkPanedClass parent_class;
};

GType		e_paned_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_paned_new			(GtkOrientation orientation);
gint		e_paned_get_hposition		(EPaned *paned);
void		e_paned_set_hposition		(EPaned *paned,
						 gint hposition);
gint		e_paned_get_vposition		(EPaned *paned);
void		e_paned_set_vposition		(EPaned *paned,
						 gint vposition);
gdouble		e_paned_get_proportion		(EPaned *paned);
void		e_paned_set_proportion		(EPaned *paned,
						 gdouble proportion);
gboolean	e_paned_get_fixed_resize	(EPaned *paned);
void		e_paned_set_fixed_resize	(EPaned *paned,
						 gboolean fixed_resize);

G_END_DECLS

#endif /* E_PANED_H */
