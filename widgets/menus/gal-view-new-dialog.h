/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gal-view-new-dialog.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GAL_VIEW_NEW_DIALOG_H__
#define __GAL_VIEW_NEW_DIALOG_H__

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

#define GAL_VIEW_NEW_DIALOG_TYPE			(gal_view_new_dialog_get_type ())
#define GAL_VIEW_NEW_DIALOG(obj)			(GTK_CHECK_CAST ((obj), GAL_VIEW_NEW_DIALOG_TYPE, GalViewNewDialog))
#define GAL_VIEW_NEW_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GAL_VIEW_NEW_DIALOG_TYPE, GalViewNewDialogClass))
#define GAL_IS_VIEW_NEW_DIALOG(obj)		(GTK_CHECK_TYPE ((obj), GAL_VIEW_NEW_DIALOG_TYPE))
#define GAL_IS_VIEW_NEW_DIALOG_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GAL_VIEW_NEW_DIALOG_TYPE))

typedef struct _GalViewNewDialog       GalViewNewDialog;
typedef struct _GalViewNewDialogClass  GalViewNewDialogClass;

struct _GalViewNewDialog
{
	GnomeDialog parent;
	
	/* item specific fields */
	GladeXML *gui;

	GalViewCollection *collection;
	GalViewFactory *selected_factory;
};

struct _GalViewNewDialogClass
{
	GnomeDialogClass parent_class;
};

GtkWidget *gal_view_new_dialog_new        (GalViewCollection *collection);
GtkType    gal_view_new_dialog_get_type   (void);

GtkWidget *gal_view_new_dialog_construct  (GalViewNewDialog  *dialog,
					   GalViewCollection *collection);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GAL_VIEW_NEW_DIALOG_H__ */
