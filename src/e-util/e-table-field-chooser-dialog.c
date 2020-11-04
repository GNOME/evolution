/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include "e-table-field-chooser-dialog.h"

enum {
	PROP_0,
	PROP_DND_CODE,
	PROP_FULL_HEADER,
	PROP_HEADER
};

G_DEFINE_TYPE (
	ETableFieldChooserDialog,
	e_table_field_chooser_dialog,
	GTK_TYPE_DIALOG)

static void
e_table_field_chooser_dialog_set_property (GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG (object);
	switch (property_id) {
	case PROP_DND_CODE:
		g_free (etfcd->dnd_code);
		etfcd->dnd_code = g_strdup (g_value_get_string (value));
		if (etfcd->etfc)
			g_object_set (
				etfcd->etfc,
				"dnd_code", etfcd->dnd_code,
				NULL);
		break;
	case PROP_FULL_HEADER:
		if (etfcd->full_header)
			g_object_unref (etfcd->full_header);
		if (g_value_get_object (value))
			etfcd->full_header = E_TABLE_HEADER (g_value_get_object (value));
		else
			etfcd->full_header = NULL;
		if (etfcd->full_header)
			g_object_ref (etfcd->full_header);
		if (etfcd->etfc)
			g_object_set (
				etfcd->etfc,
				"full_header", etfcd->full_header,
				NULL);
		break;
	case PROP_HEADER:
		if (etfcd->header)
			g_object_unref (etfcd->header);
		if (g_value_get_object (value))
			etfcd->header = E_TABLE_HEADER (g_value_get_object (value));
		else
			etfcd->header = NULL;
		if (etfcd->header)
			g_object_ref (etfcd->header);
		if (etfcd->etfc)
			g_object_set (
				etfcd->etfc,
				"header", etfcd->header,
				NULL);
		break;
	default:
		break;
	}
}

static void
e_table_field_chooser_dialog_get_property (GObject *object,
                                           guint property_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG (object);
	switch (property_id) {
	case PROP_DND_CODE:
		g_value_set_string (value, etfcd->dnd_code);
		break;
	case PROP_FULL_HEADER:
		g_value_set_object (value, etfcd->full_header);
		break;
	case PROP_HEADER:
		g_value_set_object (value, etfcd->header);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_table_field_chooser_dialog_dispose (GObject *object)
{
	ETableFieldChooserDialog *etfcd = E_TABLE_FIELD_CHOOSER_DIALOG (object);

	g_clear_pointer (&etfcd->dnd_code, g_free);
	g_clear_object (&etfcd->full_header);
	g_clear_object (&etfcd->header);

	G_OBJECT_CLASS (e_table_field_chooser_dialog_parent_class)->dispose (object);
}

static void
e_table_field_chooser_dialog_response (GtkDialog *dialog,
                                       gint id)
{
	if (id == GTK_RESPONSE_OK)
		gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
e_table_field_chooser_dialog_class_init (ETableFieldChooserDialogClass *class)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_table_field_chooser_dialog_set_property;
	object_class->get_property = e_table_field_chooser_dialog_get_property;
	object_class->dispose = e_table_field_chooser_dialog_dispose;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = e_table_field_chooser_dialog_response;

	g_object_class_install_property (
		object_class,
		PROP_DND_CODE,
		g_param_spec_string (
			"dnd_code",
			"DnD code",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FULL_HEADER,
		g_param_spec_object (
			"full_header",
			"Full Header",
			NULL,
			E_TYPE_TABLE_HEADER,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HEADER,
		g_param_spec_object (
			"header",
			"Header",
			NULL,
			E_TYPE_TABLE_HEADER,
			G_PARAM_READWRITE));
}

static void
e_table_field_chooser_dialog_init (ETableFieldChooserDialog *e_table_field_chooser_dialog)
{
	GtkDialog *dialog;
	GtkWidget *content_area;
	GtkWidget *widget;

	dialog = GTK_DIALOG (e_table_field_chooser_dialog);

	e_table_field_chooser_dialog->etfc = NULL;
	e_table_field_chooser_dialog->dnd_code = g_strdup ("");
	e_table_field_chooser_dialog->full_header = NULL;
	e_table_field_chooser_dialog->header = NULL;

	gtk_dialog_add_button (dialog, _("_Close"), GTK_RESPONSE_OK);

	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

	widget = e_table_field_chooser_new ();
	e_table_field_chooser_dialog->etfc = E_TABLE_FIELD_CHOOSER (widget);

	g_object_set (
		widget,
		"dnd_code", e_table_field_chooser_dialog->dnd_code,
		"full_header", e_table_field_chooser_dialog->full_header,
		"header", e_table_field_chooser_dialog->header,
		NULL);

	content_area = gtk_dialog_get_content_area (dialog);
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);

	gtk_widget_show (GTK_WIDGET (widget));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Add a Column"));
}

GtkWidget *
e_table_field_chooser_dialog_new (void)
{
	return g_object_new (E_TYPE_TABLE_FIELD_CHOOSER_DIALOG, NULL);
}

