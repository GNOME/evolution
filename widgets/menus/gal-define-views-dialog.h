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

#ifndef GAL_DEFINE_VIEWS_DIALOG_H
#define GAL_DEFINE_VIEWS_DIALOG_H

#include <gtk/gtk.h>
#include <menus/gal-view-collection.h>

/* Standard GObject macros */
#define GAL_TYPE_DEFINE_VIEWS_DIALOG \
	(gal_define_views_dialog_get_type ())
#define GAL_DEFINE_VIEWS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_DEFINE_VIEWS_DIALOG, GalDefineViewsDialog))
#define GAL_DEFINE_VIEWS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_DEFINE_VIEWS_DIALOG, GalDefineViewsDialogClass))
#define GAL_IS_DEFINE_VIEWS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_DEFINE_VIEWS_DIALOG))
#define GAL_IS_DEFINE_VIEWS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_DEFINE_VIEWS_DIALOG))
#define GAL_DEFINE_VIEWS_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_DEFINE_VIEWS_DIALOG, GalDefineViewsDialogClass))

G_BEGIN_DECLS

typedef struct _GalDefineViewsDialog GalDefineViewsDialog;
typedef struct _GalDefineViewsDialogClass GalDefineViewsDialogClass;

struct _GalDefineViewsDialog {
	GtkDialog parent;

	/* item specific fields */
	GtkBuilder *builder;
	GtkTreeView *treeview;
	GtkTreeModel *model;

	GalViewCollection *collection;
};

struct _GalDefineViewsDialogClass {
	GtkDialogClass parent_class;
};

GType		gal_define_views_dialog_get_type (void);
GtkWidget *	gal_define_views_dialog_new	(GalViewCollection *collection);

G_END_DECLS

#endif /* GAL_DEFINE_VIEWS_DIALOG_H */
