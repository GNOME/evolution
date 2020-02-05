/*
 * gal-view-etable.h
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef GAL_VIEW_ETABLE_H
#define GAL_VIEW_ETABLE_H

#include <gtk/gtk.h>
#include <e-util/gal-view.h>
#include <e-util/e-table-state.h>
#include <e-util/e-table.h>
#include <e-util/e-tree.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW_ETABLE \
	(gal_view_etable_get_type ())
#define GAL_VIEW_ETABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW_ETABLE, GalViewEtable))
#define GAL_VIEW_ETABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW_ETABLE, GalViewEtableClass))
#define GAL_IS_VIEW_ETABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW_ETABLE))
#define GAL_IS_VIEW_ETABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW_ETABLE))
#define GAL_VIEW_ETABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW_ETABLE, GalViewEtableClass))

G_BEGIN_DECLS

typedef struct _GalViewEtable GalViewEtable;
typedef struct _GalViewEtableClass GalViewEtableClass;
typedef struct _GalViewEtablePrivate GalViewEtablePrivate;

struct _GalViewEtable {
	GalView parent;
	GalViewEtablePrivate *priv;
};

struct _GalViewEtableClass {
	GalViewClass parent_class;
};

GType		gal_view_etable_get_type	(void);
GalView *	gal_view_etable_new		(const gchar *title);
void		gal_view_etable_attach_table	(GalViewEtable *view,
						 ETable *table);
void		gal_view_etable_attach_tree	(GalViewEtable *view,
						 ETree *tree);
void		gal_view_etable_detach		(GalViewEtable *view);
ETable *	gal_view_etable_get_table	(GalViewEtable *view);
ETree *		gal_view_etable_get_tree	(GalViewEtable *view);

G_END_DECLS

#endif /* GAL_VIEW_ETABLE_H */
