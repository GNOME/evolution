/*
 *
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

#ifndef _GAL_VIEW_INSTANCE_H_
#define _GAL_VIEW_INSTANCE_H_

#include <glib-object.h>
#include <menus/gal-view-collection.h>

G_BEGIN_DECLS

#define GAL_VIEW_INSTANCE_TYPE        (gal_view_instance_get_type ())
#define GAL_VIEW_INSTANCE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_VIEW_INSTANCE_TYPE, GalViewInstance))
#define GAL_VIEW_INSTANCE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_VIEW_INSTANCE_TYPE, GalViewInstanceClass))
#define GAL_IS_VIEW_INSTANCE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_VIEW_INSTANCE_TYPE))
#define GAL_IS_VIEW_INSTANCE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_VIEW_INSTANCE_TYPE))

typedef struct {
	GObject base;

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
} GalViewInstance;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Signals
	 */
	void (*display_view) (GalViewInstance *instance,
			      GalView    *view);
	void (*changed)      (GalViewInstance *instance);
	void (*loaded)       (GalViewInstance *instance);
} GalViewInstanceClass;

/* Standard functions */
GType            gal_view_instance_get_type             (void);

/* */
/*collection should be loaded when you call this.
  instance_id: Which instance of this type of object is this (for most of evo, this is the folder id.) */
GalViewInstance *gal_view_instance_new                  (GalViewCollection *collection,
							 const gchar        *instance_id);
GalViewInstance *gal_view_instance_construct            (GalViewInstance   *instance,
							 GalViewCollection *collection,
							 const gchar        *instance_id);

/* Manipulate the current view. */
gchar            *gal_view_instance_get_current_view_id  (GalViewInstance   *instance);
void             gal_view_instance_set_current_view_id  (GalViewInstance   *instance,
							 const gchar        *view_id);
GalView         *gal_view_instance_get_current_view     (GalViewInstance   *instance);

/* Sets the current view to the given custom view. */
void             gal_view_instance_set_custom_view      (GalViewInstance   *instance,
							 GalView           *view);

/* Returns true if this instance has ever been used before. */
gboolean         gal_view_instance_exists               (GalViewInstance   *instance);

/* Manipulate the view collection */
/* void             gal_view_instance_set_as_default       (GalViewInstance   *instance); */
void             gal_view_instance_save_as              (GalViewInstance   *instance);

/* This is idempotent.  Once it's been called once, the rest of the calls are ignored. */
void             gal_view_instance_load                 (GalViewInstance   *instance);

/* These only mean anything before gal_view_instance_load is called the first time.  */
const gchar      *gal_view_instance_get_default_view     (GalViewInstance   *instance);
void             gal_view_instance_set_default_view     (GalViewInstance   *instance,
							 const gchar        *id);

G_END_DECLS

#endif /* _GAL_VIEW_INSTANCE_H_ */
