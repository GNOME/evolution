/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser-dialog.h
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

#ifndef __E_TABLE_FIELD_CHOOSER_DIALOG_H__
#define __E_TABLE_FIELD_CHOOSER_DIALOG_H__

#include <gtk/gtkdialog.h>
#include <table/e-table-field-chooser.h>
#include <table/e-table-header.h>

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
#define E_TABLE_FIELD_CHOOSER_DIALOG(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TABLE_FIELD_CHOOSER_DIALOG_TYPE, ETableFieldChooserDialog))
#define E_TABLE_FIELD_CHOOSER_DIALOG_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TABLE_FIELD_CHOOSER_DIALOG_TYPE, ETableFieldChooserDialogClass))
#define E_IS_TABLE_FIELD_CHOOSER_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TABLE_FIELD_CHOOSER_DIALOG_TYPE))
#define E_IS_TABLE_FIELD_CHOOSER_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TABLE_FIELD_CHOOSER_DIALOG_TYPE))


typedef struct _ETableFieldChooserDialog       ETableFieldChooserDialog;
typedef struct _ETableFieldChooserDialogClass  ETableFieldChooserDialogClass;

struct _ETableFieldChooserDialog
{
	GtkDialog parent;
	
	/* item specific fields */
	ETableFieldChooser *etfc;
	gchar              *dnd_code;
	ETableHeader       *full_header;
	ETableHeader       *header;
};

struct _ETableFieldChooserDialogClass
{
	GtkDialogClass parent_class;
};


GtkWidget *e_table_field_chooser_dialog_new(void);
GType      e_table_field_chooser_dialog_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_TABLE_FIELD_CHOOSER_DIALOG_H__ */
