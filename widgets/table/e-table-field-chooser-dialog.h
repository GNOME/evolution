/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-table-field-chooser-dialog.h
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
#ifndef __E_TABLE_FIELD_CHOOSER_DIALOG_H__
#define __E_TABLE_FIELD_CHOOSER_DIALOG_H__

#include <gnome.h>
#include <glade/glade.h>
#include "e-table-field-chooser.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* ETableFieldChooserDialog - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_TABLE_FIELD_CHOOSER_DIALOG_TYPE			(e_table_field_chooser_dialog_get_type ())
#define E_TABLE_FIELD_CHOOSER_DIALOG(obj)			(GTK_CHECK_CAST ((obj), E_TABLE_FIELD_CHOOSER_DIALOG_TYPE, ETableFieldChooserDialog))
#define E_TABLE_FIELD_CHOOSER_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TABLE_FIELD_CHOOSER_DIALOG_TYPE, ETableFieldChooserDialogClass))
#define E_IS_TABLE_FIELD_CHOOSER_DIALOG(obj)		(GTK_CHECK_TYPE ((obj), E_TABLE_FIELD_CHOOSER_DIALOG_TYPE))
#define E_IS_TABLE_FIELD_CHOOSER_DIALOG_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TABLE_FIELD_CHOOSER_DIALOG_TYPE))


typedef struct _ETableFieldChooserDialog       ETableFieldChooserDialog;
typedef struct _ETableFieldChooserDialogClass  ETableFieldChooserDialogClass;

struct _ETableFieldChooserDialog
{
	GnomeDialog parent;
	
	/* item specific fields */
	ETableFieldChooser *chooser;
};

struct _ETableFieldChooserDialogClass
{
	GnomeDialogClass parent_class;
};


GtkWidget *e_table_field_chooser_dialog_new(void);
GtkType    e_table_field_chooser_dialog_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_TABLE_FIELD_CHOOSER_DIALOG_H__ */
