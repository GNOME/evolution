/*
 * gal-view.h
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

#ifndef GAL_VIEW_H
#define GAL_VIEW_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW \
	(gal_view_get_type ())
#define GAL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW, GalView))
#define GAL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW, GalViewClass))
#define GAL_IS_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW))
#define GAL_IS_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW))
#define GAL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW, GalViewClass))

G_BEGIN_DECLS

typedef struct _GalView GalView;
typedef struct _GalViewClass GalViewClass;
typedef struct _GalViewPrivate GalViewPrivate;

struct _GalView {
	GObject parent;
	GalViewPrivate *priv;
};

struct _GalViewClass {
	GObjectClass parent_class;

	const gchar *type_code;

	/* Methods */
	void		(*load)			(GalView *view,
						 const gchar *filename);
	void		(*save)			(GalView *view,
						 const gchar *filename);
	GalView *	(*clone)		(GalView *view);

	/* Signals */
	void		(*changed)		(GalView *view);
};

GType		gal_view_get_type		(void);
void		gal_view_load			(GalView *view,
						 const gchar *filename);
void		gal_view_save			(GalView *view,
						 const gchar *filename);
const gchar *	gal_view_get_title		(GalView *view);
void		gal_view_set_title		(GalView *view,
						 const gchar *title);
GalView *	gal_view_clone			(GalView *view);
void		gal_view_changed		(GalView *view);

#ifndef __GI_SCANNER__
#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GalView, g_object_unref)
#endif
#endif

G_END_DECLS

#endif /* GAL_VIEW_H */
