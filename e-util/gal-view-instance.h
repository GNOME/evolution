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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef GAL_VIEW_INSTANCE_H
#define GAL_VIEW_INSTANCE_H

#include <e-util/gal-view-collection.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW_INSTANCE \
	(gal_view_instance_get_type ())
#define GAL_VIEW_INSTANCE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW_INSTANCE, GalViewInstance))
#define GAL_VIEW_INSTANCE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW_INSTANCE, GalViewInstanceClass))
#define GAL_IS_VIEW_INSTANCE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW_INSTANCE))
#define GAL_IS_VIEW_INSTANCE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW_INSTANCE))
#define GAL_VIEW_INSTANCE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW_INSTANCE, GalViewInstanceClass))

G_BEGIN_DECLS

typedef struct _GalViewInstance GalViewInstance;
typedef struct _GalViewInstanceClass GalViewInstanceClass;

struct _GalViewInstance {
	GObject parent;

	GalViewCollection *collection;

	gchar *instance_id;
	gchar *current_view_filename;
	gchar *custom_filename;

	gchar *current_title;
	gchar *current_type;
	gchar *current_id;

	GalView *current_view;

	guint view_changed_id;
	guint collection_changed_id;

	guint loaded : 1;
	gchar *default_view;
};

struct _GalViewInstanceClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*display_view)		(GalViewInstance *instance,
						 GalView *view);
	void		(*changed)		(GalViewInstance *instance);
	void		(*loaded)		(GalViewInstance *instance);
};

GType		gal_view_instance_get_type	(void) G_GNUC_CONST;

/*collection should be loaded when you call this.
  instance_id: Which instance of this type of object is this (for most of evo, this is the folder id.) */
GalViewInstance *
		gal_view_instance_new		(GalViewCollection *collection,
						 const gchar *instance_id);
GalViewInstance *
		gal_view_instance_construct	(GalViewInstance *instance,
						 GalViewCollection *collection,
						 const gchar *instance_id);

/* Manipulate the current view. */
gchar *		gal_view_instance_get_current_view_id
						(GalViewInstance *instance);
void		gal_view_instance_set_current_view_id
						(GalViewInstance *instance,
						 const gchar *view_id);
GalView *	gal_view_instance_get_current_view
						(GalViewInstance *instance);

/* Sets the current view to the given custom view. */
void		gal_view_instance_set_custom_view
						(GalViewInstance *instance,
						 GalView *view);

/* Returns true if this instance has ever been used before. */
gboolean	gal_view_instance_exists	(GalViewInstance *instance);

/* Manipulate the view collection */
void		gal_view_instance_save_as	(GalViewInstance *instance);

/* This is idempotent.  Once it's been called
 * once, the rest of the calls are ignored. */
void		gal_view_instance_load		(GalViewInstance *instance);

/* These only mean anything before gal_view_instance_load()
 * is called the first time.  */
const gchar *	gal_view_instance_get_default_view
						(GalViewInstance *instance);
void		gal_view_instance_set_default_view
						(GalViewInstance   *instance,
						 const gchar *id);

G_END_DECLS

#endif /* GAL_VIEW_INSTANCE_H */
