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

#include <gtk/gtk.h>

#include "gal/util/e-i18n.h"
#include "gal/util/e-util.h"

#include "e-table-field-chooser-dialog.h"

static void e_table_field_chooser_dialog_init		(ETableFieldChooserDialog		 *card);
static void e_table_field_chooser_dialog_class_init	(ETableFieldChooserDialogClass	 *klass);
static void e_table_field_chooser_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_table_field_chooser_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_table_field_chooser_dialog_dispose (GObject *object);
static void e_table_field_chooser_dialog_response (GtkDialog *dialog, gint id);

#define PARENT_TYPE GTK_TYPE_DIALOG
static GtkDialogClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_DND_CODE,
	PROP_FULL_HEADER,
	PROP_HEADER
};

E_MAKE_TYPE (e_table_field_chooser_dialog,
	     "ETableFieldChooserDialog",
	     ETableFieldChooserDialog,
	     e_table_field_chooser_dialog_class_init,
	     e_table_field_chooser_dialog_init,
	     PARENT_TYPE);

static void
e_table_field_chooser_dialog_class_init (ETableFieldChooserDialogClass *klass)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;

	object_class = (GObjectClass*) klass;
	dialog_class = GTK_DIALOG_CLASS (klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose      = e_table_field_chooser_dialog_dispose;
	object_class->set_property = e_table_field_chooser_dialog_set_property;
	object_class->get_property = e_table_field_chooser_dialog_get_property;

	dialog_class->response = e_table_field_chooser_dialog_response;

	g_object_class_install_property (object_class, PROP_DND_CODE,
					 g_param_spec_string ("dnd_code",
							      _("DnD code"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FULL_HEADER,
					 g_param_spec_object ("full_header",
							      _("Full Header"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HEADER,
					 g_param_spec_object ("header",
							      _("Header"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));
}

static void
e_table_field_chooser_dialog_init (ETableFieldChooserDialog *e_table_field_chooser_dialog)
{
	GtkWidget *widget;

	e_table_field_chooser_dialog->etfc = NULL;
	e_table_field_chooser_dialog->dnd_code = g_strdup("");
	e_table_field_chooser_dialog->full_header = NULL;
	e_table_field_chooser_dialog->header = NULL;

	gtk_dialog_add_button(GTK_DIALOG(e_table_field_chooser_dialog),
			      GTK_STOCK_CLOSE, GTK_RESPONSE_OK);

	gtk_window_set_policy(GTK_WINDOW(e_table_field_chooser_dialog), FALSE, TRUE, FALSE);

	widget = e_table_field_chooser_new();
	e_table_field_chooser_dialog->etfc = E_TABLE_FIELD_CHOOSER(widget);
	
	g_object_set(widget,
		     "dnd_code", e_table_field_chooser_dialog->dnd_code,
		     "full_header", e_table_field_chooser_dialog->full_header,
		     "header", e_table_field_chooser_dialog->header,
		     NULL);
	
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(e_table_field_chooser_dialog)->vbox),
			   widget, TRUE, TRUE, 0);

	gtk_widget_show(GTK_WIDGET(widget));

	gtk_window_set_title (GTK_WINDOW (e_table_field_chooser_dialog), _("Add a column..."));
}

GtkWidget*
e_table_field_chooser_dialog_new (void)
{
	GtkWidget *widget = g_object_new (E_TABLE_FIELD_CHOOSER_DIALOG_TYPE, NULL);
	return widget;
}

static void
e_table_field_chooser_dialog_dispose (GObject *object)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG (object);

	if (etfcd->dnd_code)
		g_free (etfcd->dnd_code);
	etfcd->dnd_code = NULL;

	if (etfcd->full_header)
		g_object_unref (etfcd->full_header);
	etfcd->full_header = NULL;

	if (etfcd->header)
		g_object_unref (etfcd->header);
	etfcd->header = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_table_field_chooser_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG(object);
	switch (prop_id){
	case PROP_DND_CODE:
		g_free(etfcd->dnd_code);
		etfcd->dnd_code = g_strdup(g_value_get_string (value));
		if (etfcd->etfc)
			g_object_set(etfcd->etfc,
				     "dnd_code", etfcd->dnd_code,
				     NULL);
		break;
	case PROP_FULL_HEADER:
		if (etfcd->full_header)
			g_object_unref (etfcd->full_header);
		if (g_value_get_object (value))
			etfcd->full_header = E_TABLE_HEADER(g_value_get_object (value));
		else
			etfcd->full_header = NULL;
		if (etfcd->full_header)
			g_object_ref (etfcd->full_header);
		if (etfcd->etfc)
			g_object_set(etfcd->etfc,
				     "full_header", etfcd->full_header,
				     NULL);
		break;
	case PROP_HEADER:
		if (etfcd->header)
			g_object_unref (etfcd->header);
		if (g_value_get_object (value))
			etfcd->header = E_TABLE_HEADER(g_value_get_object (value));
		else
			etfcd->header = NULL;
		if (etfcd->header)
			g_object_ref (etfcd->header);
		if (etfcd->etfc)
			g_object_set(etfcd->etfc,
				     "header", etfcd->header,
				     NULL);
		break;
	default:
		break;
	}
}

static void
e_table_field_chooser_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG(object);
	switch (prop_id) {
	case PROP_DND_CODE:
		g_value_set_string (value, g_strdup (etfcd->dnd_code));
		break;
	case PROP_FULL_HEADER:
		g_value_set_object (value, etfcd->full_header);
		break;
	case PROP_HEADER:
		g_value_set_object (value, etfcd->header);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_table_field_chooser_dialog_response (GtkDialog *dialog, int id)
{
	if (id == GTK_RESPONSE_OK)
		gtk_widget_destroy (GTK_WIDGET (dialog));
}
