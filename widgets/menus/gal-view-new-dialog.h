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

#ifndef __GAL_VIEW_NEW_DIALOG_H__
#define __GAL_VIEW_NEW_DIALOG_H__

#include <gtk/gtk.h>
#include <gal-view-collection.h>

G_BEGIN_DECLS

/* GalViewNewDialog - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define GAL_VIEW_NEW_DIALOG_TYPE		(gal_view_new_dialog_get_type ())
#define GAL_VIEW_NEW_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_VIEW_NEW_DIALOG_TYPE, GalViewNewDialog))
#define GAL_VIEW_NEW_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GAL_VIEW_NEW_DIALOG_TYPE, GalViewNewDialogClass))
#define GAL_IS_VIEW_NEW_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_VIEW_NEW_DIALOG_TYPE))
#define GAL_IS_VIEW_NEW_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), GAL_VIEW_NEW_DIALOG_TYPE))

typedef struct _GalViewNewDialog       GalViewNewDialog;
typedef struct _GalViewNewDialogClass  GalViewNewDialogClass;

struct _GalViewNewDialog
{
	GtkDialog parent;

	/* item specific fields */
	GtkBuilder *builder;

	GalViewCollection *collection;
	GalViewFactory *selected_factory;

	GtkListStore *list_store;

	GtkWidget *entry;
	GtkWidget *list;
};

struct _GalViewNewDialogClass
{
	GtkDialogClass parent_class;
};

GtkWidget *gal_view_new_dialog_new        (GalViewCollection *collection);
GType      gal_view_new_dialog_get_type   (void);

GtkWidget *gal_view_new_dialog_construct  (GalViewNewDialog  *dialog,
					   GalViewCollection *collection);

G_END_DECLS

#endif /* __GAL_VIEW_NEW_DIALOG_H__ */
