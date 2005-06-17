/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-define-views-dialog.h
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

#ifndef __GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H__
#define __GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H__

#include <gtk/gtkdialog.h>
#include <glade/glade.h>
#include <table/e-table-model.h>
#include <widgets/menus/gal-view-collection.h>
#include <widgets/menus/gal-view-instance.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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
	ETableModel *model;

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

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_H__ */
