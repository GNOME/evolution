/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef GAL_VIEW_COLLECTION_H
#define GAL_VIEW_COLLECTION_H

#include <e-util/gal-view-factory.h>

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

typedef struct _GalViewCollectionItem GalViewCollectionItem;

struct _GalViewCollection {
	GObject parent;

	GalViewCollectionItem **view_data;
	gint view_count;

	GList *factory_list;

	GalViewCollectionItem **removed_view_data;
	gint removed_view_count;

	guint loaded : 1;
	guint default_view_built_in : 1;

	gchar *system_dir;
	gchar *local_dir;

	gchar *default_view;

	gchar *title;
};

struct _GalViewCollectionClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*display_view)		(GalViewCollection *collection,
						 GalView *view);
	void		(*changed)		(GalViewCollection *collection);
};

struct _GalViewCollectionItem {
	GalView *view;
	gchar *id;
	guint changed : 1;
	guint ever_changed : 1;
	guint built_in : 1;
	gchar *filename;
	gchar *title;
	gchar *type;
	GalViewCollection *collection;
	guint view_changed_id;
};

GType		gal_view_collection_get_type	(void) G_GNUC_CONST;
GalViewCollection *
		gal_view_collection_new		(void);
void		gal_view_collection_set_title	(GalViewCollection *collection,
						 const gchar        *title);

/* Set up the view collection.  Call these two functions
 * before ever doing load or save and never call them again. */
void		gal_view_collection_set_storage_directories
						(GalViewCollection *collection,
						 const gchar *system_dir,
						 const gchar *local_dir);
void		gal_view_collection_add_factory	(GalViewCollection *collection,
						 GalViewFactory *factory);

/* Send the display view signal.  This function is deprecated. */
void		gal_view_collection_display_view
						(GalViewCollection *collection,
						 GalView *view);

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
gchar *		gal_view_collection_get_view_id_by_index
						(GalViewCollection *collection,
						 gint n);

/* Manipulate the view collection */
void		gal_view_collection_append	(GalViewCollection *collection,
						 GalView *view);
void		gal_view_collection_delete_view	(GalViewCollection *collection,
						 gint i);
void		gal_view_collection_copy_view	(GalViewCollection *collection,
						 gint i);

/* Call set_storage_directories and add factories for anything
 * that might be found there before doing either of these. */
void		gal_view_collection_load	(GalViewCollection *collection);
void		gal_view_collection_save	(GalViewCollection *collection);
gboolean	gal_view_collection_loaded	(GalViewCollection *collection);

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
void		gal_view_collection_set_default_view
						(GalViewCollection *collection,
						 const gchar *id);

G_END_DECLS

#endif /* _GAL_VIEW_COLLECTION_H_ */
