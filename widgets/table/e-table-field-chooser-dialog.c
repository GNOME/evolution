/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser-dialog.c
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

#include <config.h>
#include "e-table-field-chooser-dialog.h"
#include <libgnomeui/gnome-stock.h>
#include "gal/util/e-i18n.h"

static void e_table_field_chooser_dialog_init		(ETableFieldChooserDialog		 *card);
static void e_table_field_chooser_dialog_class_init	(ETableFieldChooserDialogClass	 *klass);
static void e_table_field_chooser_dialog_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_table_field_chooser_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_table_field_chooser_dialog_destroy (GtkObject *object);
static void e_table_field_chooser_dialog_clicked (GnomeDialog *dialog, gint button);

static GnomeDialogClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_DND_CODE,
	ARG_FULL_HEADER,
	ARG_HEADER
};

GtkType
e_table_field_chooser_dialog_get_type (void)
{
	static GtkType table_field_chooser_dialog_type = 0;

	if (!table_field_chooser_dialog_type)
		{
			static const GtkTypeInfo table_field_chooser_dialog_info =
			{

				"ETableFieldChooserDialog",
				sizeof (ETableFieldChooserDialog),
				sizeof (ETableFieldChooserDialogClass),
				(GtkClassInitFunc) e_table_field_chooser_dialog_class_init,
				(GtkObjectInitFunc) e_table_field_chooser_dialog_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
				(GtkClassInitFunc) NULL,
			};

			table_field_chooser_dialog_type = gtk_type_unique (gnome_dialog_get_type (), &table_field_chooser_dialog_info);
		}

	return table_field_chooser_dialog_type;
}

static void
e_table_field_chooser_dialog_class_init (ETableFieldChooserDialogClass *klass)
{
	GtkObjectClass *object_class;
	GnomeDialogClass *dialog_class;

	object_class = (GtkObjectClass*) klass;
	dialog_class = GNOME_DIALOG_CLASS (klass);

	parent_class = gtk_type_class (gnome_dialog_get_type ());

	object_class->destroy = e_table_field_chooser_dialog_destroy;
	object_class->set_arg = e_table_field_chooser_dialog_set_arg;
	object_class->get_arg = e_table_field_chooser_dialog_get_arg;

	dialog_class->clicked = e_table_field_chooser_dialog_clicked;

	gtk_object_add_arg_type ("ETableFieldChooserDialog::dnd_code", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_DND_CODE);
	gtk_object_add_arg_type ("ETableFieldChooserDialog::full_header", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_FULL_HEADER);
	gtk_object_add_arg_type ("ETableFieldChooserDialog::header", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_HEADER);
}

static void
e_table_field_chooser_dialog_init (ETableFieldChooserDialog *e_table_field_chooser_dialog)
{
	GtkWidget *widget;

	e_table_field_chooser_dialog->etfc = NULL;
	e_table_field_chooser_dialog->dnd_code = g_strdup("");
	e_table_field_chooser_dialog->full_header = NULL;
	e_table_field_chooser_dialog->header = NULL;

	gnome_dialog_append_buttons(GNOME_DIALOG(e_table_field_chooser_dialog),
				    GNOME_STOCK_BUTTON_CLOSE,
				    NULL);

	gtk_window_set_policy(GTK_WINDOW(e_table_field_chooser_dialog), FALSE, TRUE, FALSE);

	widget = e_table_field_chooser_new();
	e_table_field_chooser_dialog->etfc = E_TABLE_FIELD_CHOOSER(widget);
	
	gtk_object_set(GTK_OBJECT(widget),
		       "dnd_code", e_table_field_chooser_dialog->dnd_code,
		       "full_header", e_table_field_chooser_dialog->full_header,
		       "header", e_table_field_chooser_dialog->header,
		       NULL);
	
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(e_table_field_chooser_dialog)->vbox),
			   widget, TRUE, TRUE, 0);

	gtk_widget_show(GTK_WIDGET(widget));

	gtk_window_set_title (GTK_WINDOW (e_table_field_chooser_dialog), _("Add a column..."));
}

GtkWidget*
e_table_field_chooser_dialog_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_table_field_chooser_dialog_get_type ()));
	return widget;
}

static void
e_table_field_chooser_dialog_destroy (GtkObject *object)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG (object);
	g_free(etfcd->dnd_code);
	if (etfcd->full_header)
		gtk_object_unref(GTK_OBJECT(etfcd->full_header));
	if (etfcd->header)
		gtk_object_unref(GTK_OBJECT(etfcd->header));
}

static void
e_table_field_chooser_dialog_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG(object);
	switch (arg_id){
	case ARG_DND_CODE:
		g_free(etfcd->dnd_code);
		etfcd->dnd_code = g_strdup(GTK_VALUE_STRING (*arg));
		if (etfcd->etfc)
			gtk_object_set(GTK_OBJECT(etfcd->etfc),
				       "dnd_code", etfcd->dnd_code,
				       NULL);
		break;
	case ARG_FULL_HEADER:
		if (etfcd->full_header)
			gtk_object_unref(GTK_OBJECT(etfcd->full_header));
		if (GTK_VALUE_OBJECT(*arg))
			etfcd->full_header = E_TABLE_HEADER(GTK_VALUE_OBJECT(*arg));
		else
			etfcd->full_header = NULL;
		if (etfcd->full_header)
			gtk_object_ref(GTK_OBJECT(etfcd->full_header));
		if (etfcd->etfc)
			gtk_object_set(GTK_OBJECT(etfcd->etfc),
				       "full_header", etfcd->full_header,
				       NULL);
		break;
	case ARG_HEADER:
		if (etfcd->header)
			gtk_object_unref(GTK_OBJECT(etfcd->header));
		if (GTK_VALUE_OBJECT(*arg))
			etfcd->header = E_TABLE_HEADER(GTK_VALUE_OBJECT(*arg));
		else
			etfcd->header = NULL;
		if (etfcd->header)
			gtk_object_ref(GTK_OBJECT(etfcd->header));
		if (etfcd->etfc)
			gtk_object_set(GTK_OBJECT(etfcd->etfc),
				       "header", etfcd->header,
				       NULL);
		break;
	default:
		break;
	}
}

static void
e_table_field_chooser_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG(object);
	switch (arg_id) {
	case ARG_DND_CODE:
		GTK_VALUE_STRING (*arg) = g_strdup (etfcd->dnd_code);
		break;
	case ARG_FULL_HEADER:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(etfcd->full_header);
		break;
	case ARG_HEADER:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(etfcd->header);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
e_table_field_chooser_dialog_clicked (GnomeDialog *dialog, int button)
{
	if (button == 0)
		gnome_dialog_close(dialog);
}
