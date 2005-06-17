/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-view-instance.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _GAL_VIEW_INSTANCE_H_
#define _GAL_VIEW_INSTANCE_H_

#include <gtk/gtkobject.h>
#include <widgets/menus/gal-view-collection.h>
#include <widgets/misc/e-popup-menu.h>

G_BEGIN_DECLS

#define GAL_VIEW_INSTANCE_TYPE        (gal_view_instance_get_type ())
#define GAL_VIEW_INSTANCE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_VIEW_INSTANCE_TYPE, GalViewInstance))
#define GAL_VIEW_INSTANCE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_VIEW_INSTANCE_TYPE, GalViewInstanceClass))
#define GAL_IS_VIEW_INSTANCE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_VIEW_INSTANCE_TYPE))
#define GAL_IS_VIEW_INSTANCE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_VIEW_INSTANCE_TYPE))

typedef struct {
	GObject base;

	GalViewCollection *collection;

	char *instance_id;
	char *current_view_filename;
	char *custom_filename;

	char *current_title;
	char *current_type;
	char *current_id;

	GalView *current_view;

	guint view_changed_id;
	guint collection_changed_id;

	guint loaded : 1;
	char *default_view;
} GalViewInstance;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Signals
	 */
	void (*display_view) (GalViewInstance *instance,
			      GalView    *view);
	void (*changed)      (GalViewInstance *instance);
} GalViewInstanceClass;

/* Standard functions */
GType            gal_view_instance_get_type             (void);

/* */
/*collection should be loaded when you call this.
  instance_id: Which instance of this type of object is this (for most of evo, this is the folder id.) */
GalViewInstance *gal_view_instance_new                  (GalViewCollection *collection,
							 const char        *instance_id);
GalViewInstance *gal_view_instance_construct            (GalViewInstance   *instance,
							 GalViewCollection *collection,
							 const char        *instance_id);

/* Manipulate the current view. */
char            *gal_view_instance_get_current_view_id  (GalViewInstance   *instance);
void             gal_view_instance_set_current_view_id  (GalViewInstance   *instance,
							 const char        *view_id);
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
const char      *gal_view_instance_get_default_view     (GalViewInstance   *instance);
void             gal_view_instance_set_default_view     (GalViewInstance   *instance,
							 const char        *id);

EPopupMenu      *gal_view_instance_get_popup_menu       (GalViewInstance   *instance);
void             gal_view_instance_free_popup_menu      (GalViewInstance   *instance,
							 EPopupMenu        *menu);

G_END_DECLS

#endif /* _GAL_VIEW_INSTANCE_H_ */
