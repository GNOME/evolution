/*
 * gal-view-collection.h
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

#ifndef GAL_VIEW_COLLECTION_H
#define GAL_VIEW_COLLECTION_H

#include <e-util/gal-view.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW_COLLECTION \
	(gal_view_collection_get_type ())
#define GAL_VIEW_COLLECTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW_COLLECTION, GalViewCollection))
#define GAL_VIEW_COLLECTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW_COLLECTION, GalViewCollectionClass))
#define GAL_IS_VIEW_COLLECTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW_COLLECTION))
#define GAL_IS_VIEW_COLLECTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW_COLLECTION))
#define GAL_VIEW_COLLECTION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW_COLLECTION, GalViewCollectionClass))

G_BEGIN_DECLS

typedef struct _GalViewCollection GalViewCollection;
typedef struct _GalViewCollectionClass GalViewCollectionClass;
typedef struct _GalViewCollectionPrivate GalViewCollectionPrivate;

typedef struct _GalViewCollectionItem GalViewCollectionItem;

struct _GalViewCollection {
	GObject parent;
	GalViewCollectionPrivate *priv;
};

struct _GalViewCollectionClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*changed)		(GalViewCollection *collection);
};

struct _GalViewCollectionItem {
	GalView *view;
	gchar *id;
	gboolean changed;
	gboolean ever_changed;
	gboolean built_in;
	gchar *filename;
	gchar *title;
	gchar *type;
	GalViewCollection *collection;
	guint view_changed_id;
	gchar *accelerator;
};

GType		gal_view_collection_get_type	(void) G_GNUC_CONST;
GalViewCollection *
		gal_view_collection_new		(const gchar *system_directory,
						 const gchar *user_directory);
const gchar *	gal_view_collection_get_system_directory
						(GalViewCollection *collection);
const gchar *	gal_view_collection_get_user_directory
						(GalViewCollection *collection);

/* Query the view collection. */
gint		gal_view_collection_get_count	(GalViewCollection *collection);
GalView *	gal_view_collection_get_view	(GalViewCollection *collection,
						 gint n);
GalViewCollectionItem *
		gal_view_collection_get_view_item
						(GalViewCollection *collection,
						 gint n);
gint		gal_view_collection_get_view_index_by_id
						(GalViewCollection *collection,
						 const gchar *view_id);

/* Manipulate the view collection */
void		gal_view_collection_delete_view	(GalViewCollection *collection,
						 gint i);

void		gal_view_collection_save	(GalViewCollection *collection);

/* Use factory list to load a GalView file. */
GalView *	gal_view_collection_load_view_from_file
						(GalViewCollection *collection,
						 const gchar *type,
						 const gchar *filename);

/* Returns id of the new view.  These functions are used for
 * GalViewInstanceSaveAsDialog. */
const gchar *	gal_view_collection_append_with_title
						(GalViewCollection *collection,
						 const gchar *title,
						 GalView *view);
const gchar *	gal_view_collection_set_nth_view
						(GalViewCollection *collection,
						 gint i,
						 GalView *view);

const gchar *	gal_view_collection_get_default_view
						(GalViewCollection *collection);

G_END_DECLS

#endif /* GAL_VIEW_COLLECTION_H */
