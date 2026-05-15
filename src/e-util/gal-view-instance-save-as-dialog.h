/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H
#define GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H

#include <gtk/gtk.h>
#include <e-util/gal-view-collection.h>
#include <e-util/gal-view-instance.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW_INSTANCE_SAVE_AS_DIALOG \
	(gal_view_instance_save_as_dialog_get_type ())
#define GAL_VIEW_INSTANCE_SAVE_AS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW_INSTANCE_SAVE_AS_DIALOG, GalViewInstanceSaveAsDialog))
#define GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW_INSTANCE_SAVE_AS_DIALOG, GalViewInstanceSaveAsDialogClass))
#define GAL_IS_VIEW_INSTANCE_SAVE_AS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW_INSTANCE_SAVE_AS_DIALOG))
#define GAL_IS_VIEW_INSTANCE_SAVE_AS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW_INSTANCE_SAVE_AS_DIALOG))
#define GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW_INSTANCE_SAVE_AS_DIALOG, GalViewInstanceSaveAsDialogClass))

G_BEGIN_DECLS

typedef struct _GalViewInstanceSaveAsDialog GalViewInstanceSaveAsDialog;
typedef struct _GalViewInstanceSaveAsDialogClass GalViewInstanceSaveAsDialogClass;

typedef enum {
	GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_REPLACE,
	GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_CREATE
} GalViewInstanceSaveAsDialogToggle;

struct _GalViewInstanceSaveAsDialog {
	GtkDialog parent;

	/* item specific fields */
	GtkBuilder *builder;
	GtkTreeView *treeview;
	GtkTreeModel *model;

	GtkWidget *scrolledwindow, *radiobutton_replace;
	GtkWidget *entry_create, *radiobutton_create;

	GalViewInstance *instance;
	GalViewCollection *collection;

	GalViewInstanceSaveAsDialogToggle toggle;
};

struct _GalViewInstanceSaveAsDialogClass {
	GtkDialogClass parent_class;
};

GType		gal_view_instance_save_as_dialog_get_type (void);
GtkWidget *	gal_view_instance_save_as_dialog_new
					(GalViewInstance *instance);
void		gal_view_instance_save_as_dialog_save
					(GalViewInstanceSaveAsDialog *dialog);

G_END_DECLS

#endif /* GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H */
