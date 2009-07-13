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

#ifndef _GAL_VIEW_H_
#define _GAL_VIEW_H_

#include <gtk/gtk.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

#define GAL_VIEW_TYPE        (gal_view_get_type ())
#define GAL_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_VIEW_TYPE, GalView))
#define GAL_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_VIEW_TYPE, GalViewClass))
#define GAL_IS_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_VIEW_TYPE))
#define GAL_IS_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_VIEW_TYPE))
#define GAL_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GAL_VIEW_TYPE, GalViewClass))

typedef struct {
	GObject base;
} GalView;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	void        (*edit)           (GalView    *view, GtkWindow *parent_window);
	void        (*load)           (GalView    *view,
				       const gchar *filename);
	void        (*save)           (GalView    *view,
				       const gchar *filename);
	const gchar *(*get_title)      (GalView    *view);
	void        (*set_title)      (GalView    *view,
				       const gchar *title);
	const gchar *(*get_type_code)  (GalView    *view);
	GalView    *(*clone)          (GalView    *view);

	/* Signals */
	void        (*changed)        (GalView    *view);
} GalViewClass;

/* Standard functions */
GType       gal_view_get_type       (void);

/* Open an editor dialog for this view, modal/transient for the GtkWindow arg. */
void        gal_view_edit           (GalView    *view,
				     GtkWindow *parent);

/* xml load and save functions */
void        gal_view_load           (GalView    *view,
				     const gchar *filename);
void        gal_view_save           (GalView    *view,
				     const gchar *filename);

/* Title functions */
const gchar *gal_view_get_title      (GalView    *view);
void        gal_view_set_title      (GalView    *view,
				     const gchar *title);

/* View type. */
const gchar *gal_view_get_type_code  (GalView    *view);

/* Cloning the view */
GalView    *gal_view_clone          (GalView    *view);

/* Changed signal */
void        gal_view_changed        (GalView    *view);

G_END_DECLS

#endif /* _GAL_VIEW_H_ */
