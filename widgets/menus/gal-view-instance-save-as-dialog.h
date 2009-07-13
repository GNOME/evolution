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

#ifndef __GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H__
#define __GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H__

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <widgets/menus/gal-view-collection.h>
#include <widgets/menus/gal-view-instance.h>

G_BEGIN_DECLS

/* GalViewInstanceSaveAsDialog - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TYPE			(gal_view_instance_save_as_dialog_get_type ())
#define GAL_VIEW_INSTANCE_SAVE_AS_DIALOG(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TYPE, GalViewInstanceSaveAsDialog))
#define GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TYPE, GalViewInstanceSaveAsDialogClass))
#define GAL_IS_VIEW_INSTANCE_SAVE_AS_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TYPE))
#define GAL_IS_VIEW_INSTANCE_SAVE_AS_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TYPE))

typedef struct _GalViewInstanceSaveAsDialog       GalViewInstanceSaveAsDialog;
typedef struct _GalViewInstanceSaveAsDialogClass  GalViewInstanceSaveAsDialogClass;

typedef enum {
	GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_REPLACE,
	GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_CREATE
} GalViewInstanceSaveAsDialogToggle;

struct _GalViewInstanceSaveAsDialog
{
	GtkDialog parent;

	/* item specific fields */
	GladeXML *gui;
	GtkTreeView *treeview;
	GtkTreeModel *model;

	GtkWidget *scrolledwindow, *radiobutton_replace;
	GtkWidget *entry_create, *radiobutton_create;

	GalViewInstance *instance;
	GalViewCollection *collection;

	GalViewInstanceSaveAsDialogToggle toggle;
};

struct _GalViewInstanceSaveAsDialogClass
{
	GtkDialogClass parent_class;
};

GtkWidget *gal_view_instance_save_as_dialog_new       (GalViewInstance             *instance);
GType      gal_view_instance_save_as_dialog_get_type  (void);

void       gal_view_instance_save_as_dialog_save      (GalViewInstanceSaveAsDialog *dialog);

G_END_DECLS

#endif /* __GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H__ */
