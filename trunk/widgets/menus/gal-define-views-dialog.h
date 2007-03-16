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

#ifndef __GAL_DEFINE_VIEWS_DIALOG_H__
#define __GAL_DEFINE_VIEWS_DIALOG_H__

#include <gtk/gtkdialog.h>
#include <glade/glade.h>
#include <table/e-table-model.h>
#include <widgets/menus/gal-view-collection.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* GalDefineViewsDialog - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define GAL_DEFINE_VIEWS_DIALOG_TYPE		(gal_define_views_dialog_get_type ())
#define GAL_DEFINE_VIEWS_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_DEFINE_VIEWS_DIALOG_TYPE, GalDefineViewsDialog))
#define GAL_DEFINE_VIEWS_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GAL_DEFINE_VIEWS_DIALOG_TYPE, GalDefineViewsDialogClass))
#define GAL_IS_DEFINE_VIEWS_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_DEFINE_VIEWS_DIALOG_TYPE))
#define GAL_IS_DEFINE_VIEWS_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), GAL_DEFINE_VIEWS_DIALOG_TYPE))

typedef struct _GalDefineViewsDialog       GalDefineViewsDialog;
typedef struct _GalDefineViewsDialogClass  GalDefineViewsDialogClass;

struct _GalDefineViewsDialog
{
	GtkDialog parent;

	/* item specific fields */
	GladeXML *gui;
	ETableModel *model;

	GalViewCollection *collection;
};

struct _GalDefineViewsDialogClass
{
	GtkDialogClass parent_class;
};

GtkWidget               *gal_define_views_dialog_new          (GalViewCollection *collection);
GType                    gal_define_views_dialog_get_type     (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GAL_DEFINE_VIEWS_DIALOG_H__ */
