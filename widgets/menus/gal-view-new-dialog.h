/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-new-dialog.h
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __GAL_VIEW_NEW_DIALOG_H__
#define __GAL_VIEW_NEW_DIALOG_H__

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gal-view-collection.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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
	GladeXML *gui;

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

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GAL_VIEW_NEW_DIALOG_H__ */
