/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gal-define-views-dialog.h
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
#ifndef __GAL_DEFINE_VIEWS_DIALOG_H__
#define __GAL_DEFINE_VIEWS_DIALOG_H__

#include <gnome.h>
#include <glade/glade.h>

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

#define GAL_DEFINE_VIEWS_DIALOG_TYPE			(gal_define_views_dialog_get_type ())
#define GAL_DEFINE_VIEWS_DIALOG(obj)			(GTK_CHECK_CAST ((obj), GAL_DEFINE_VIEWS_DIALOG_TYPE, GalDefineViewsDialog))
#define GAL_DEFINE_VIEWS_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GAL_DEFINE_VIEWS_DIALOG_TYPE, GalDefineViewsDialogClass))
#define GAL_IS_DEFINE_VIEWS_DIALOG(obj)		(GTK_CHECK_TYPE ((obj), GAL_DEFINE_VIEWS_DIALOG_TYPE))
#define GAL_IS_DEFINE_VIEWS_DIALOG_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GAL_DEFINE_VIEWS_DIALOG_TYPE))

typedef struct _GalDefineViewsDialog       GalDefineViewsDialog;
typedef struct _GalDefineViewsDialogClass  GalDefineViewsDialogClass;

struct _GalDefineViewsDialog
{
	GnomeDialog parent;
	
	/* item specific fields */
	GladeXML *gui;
};

struct _GalDefineViewsDialogClass
{
	GnomeDialogClass parent_class;
};

GtkWidget               *gal_define_views_dialog_new          (void);
GtkType                  gal_define_views_dialog_get_type     (void);

void                     gal_define_views_dialog_add_section  (GalDefineViewsDialog      *gal_define_views_dialog);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GAL_DEFINE_VIEWS_DIALOG_H__ */
