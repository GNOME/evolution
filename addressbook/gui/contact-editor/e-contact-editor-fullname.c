/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * eab-contact-editor-phones.c
 * Copyright (C) 2003  Ximian, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <config.h>
#include "e-contact-editor-fullname.h"
#include <e-util/e-icon-factory.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>

static void e_contact_editor_fullname_init		(EContactEditorFullname		 *card);
static void e_contact_editor_fullname_class_init	(EContactEditorFullnameClass	 *klass);
static void e_contact_editor_fullname_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_contact_editor_fullname_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_contact_editor_fullname_dispose (GObject *object);

static void fill_in_info(EContactEditorFullname *editor);
static void extract_info(EContactEditorFullname *editor);

static GtkDialogClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_NAME,
	PROP_EDITABLE
};

GType
e_contact_editor_fullname_get_type (void)
{
	static GType contact_editor_fullname_type = 0;

	if (!contact_editor_fullname_type) {
		static const GTypeInfo contact_editor_fullname_info =  {
			sizeof (EContactEditorFullnameClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_contact_editor_fullname_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EContactEditorFullname),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_contact_editor_fullname_init,
		};

		contact_editor_fullname_type = g_type_register_static (GTK_TYPE_DIALOG, "EContactEditorFullname", &contact_editor_fullname_info, 0);
	}

	return contact_editor_fullname_type;
}

static void
e_contact_editor_fullname_class_init (EContactEditorFullnameClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (GTK_TYPE_DIALOG);

	object_class->set_property = e_contact_editor_fullname_set_property;
	object_class->get_property = e_contact_editor_fullname_get_property;
	object_class->dispose = e_contact_editor_fullname_dispose;

	g_object_class_install_property (object_class, PROP_NAME, 
					 g_param_spec_pointer ("name",
							       _("Name"),
							       /*_( */"XXX blurb" /*)*/,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE, 
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));
}

static void
e_contact_editor_fullname_init (EContactEditorFullname *e_contact_editor_fullname)
{
	GladeXML *gui;
	GtkWidget *widget;
	GList *icon_list;

	gtk_widget_realize (GTK_WIDGET (e_contact_editor_fullname));
	gtk_dialog_set_has_separator (GTK_DIALOG (e_contact_editor_fullname),
				      FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (e_contact_editor_fullname)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (e_contact_editor_fullname)->action_area), 12);

	gtk_dialog_add_buttons (GTK_DIALOG (e_contact_editor_fullname),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_resizable(GTK_WINDOW(e_contact_editor_fullname), TRUE);

	e_contact_editor_fullname->name = NULL;
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/fullname.glade", NULL, NULL);
	e_contact_editor_fullname->gui = gui;

	widget = glade_xml_get_widget(gui, "dialog-checkfullname");
	gtk_window_set_title (GTK_WINDOW (e_contact_editor_fullname),
			      GTK_WINDOW (widget)->title);

	widget = glade_xml_get_widget(gui, "table-checkfullname");
	g_object_ref(widget);
	gtk_container_remove(GTK_CONTAINER(widget->parent), widget);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (e_contact_editor_fullname)->vbox), widget, TRUE, TRUE, 0);
	g_object_unref(widget);

	icon_list = e_icon_factory_get_icon_list ("stock_contact");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (e_contact_editor_fullname), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
}

void
e_contact_editor_fullname_dispose (GObject *object)
{
	EContactEditorFullname *e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME(object);

	if (e_contact_editor_fullname->gui) {
		g_object_unref(e_contact_editor_fullname->gui);
		e_contact_editor_fullname->gui = NULL;
	}
	
	if (e_contact_editor_fullname->name) {
		e_contact_name_free(e_contact_editor_fullname->name);
		e_contact_editor_fullname->name = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

GtkWidget*
e_contact_editor_fullname_new (const EContactName *name)
{
	GtkWidget *widget = g_object_new (E_TYPE_CONTACT_EDITOR_FULLNAME, NULL);

	g_object_set (widget,
		      "name", name,
		      NULL);
	return widget;
}

static void
e_contact_editor_fullname_set_property (GObject *object, guint prop_id,
					const GValue *value, GParamSpec *pspec)
{
	EContactEditorFullname *e_contact_editor_fullname;

	e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME (object);
	
	switch (prop_id){
	case PROP_NAME:
		e_contact_name_free(e_contact_editor_fullname->name);

		if (g_value_get_pointer (value) != NULL) {
			e_contact_editor_fullname->name = e_contact_name_copy(g_value_get_pointer (value));
			fill_in_info(e_contact_editor_fullname);
		}
		else {
			e_contact_editor_fullname->name = NULL;
		}
		break;
	case PROP_EDITABLE: {
		int i;
		char *widget_names[] = {
			"combo-title",
			"combo-suffix",
			"entry-first",
			"entry-middle",
			"entry-last",
			"label-title",
			"label-suffix",
			"label-first",
			"label-middle",
			"label-last",
			NULL
		};
		e_contact_editor_fullname->editable = g_value_get_boolean (value) ? TRUE : FALSE;
		for (i = 0; widget_names[i] != NULL; i ++) {
			GtkWidget *w = glade_xml_get_widget(e_contact_editor_fullname->gui, widget_names[i]);
			if (GTK_IS_ENTRY (w)) {
				gtk_editable_set_editable (GTK_EDITABLE (w),
							   e_contact_editor_fullname->editable);
			}
			else if (GTK_IS_COMBO (w)) {
				gtk_editable_set_editable (GTK_EDITABLE (GTK_COMBO (w)->entry),
							   e_contact_editor_fullname->editable);
				gtk_widget_set_sensitive (GTK_COMBO (w)->button, e_contact_editor_fullname->editable);
			}
			else if (GTK_IS_LABEL (w)) {
				gtk_widget_set_sensitive (w, e_contact_editor_fullname->editable);
			}
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_contact_editor_fullname_get_property (GObject *object, guint prop_id,
					GValue *value, GParamSpec *pspec)
{
	EContactEditorFullname *e_contact_editor_fullname;

	e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME (object);

	switch (prop_id) {
	case PROP_NAME:
		extract_info(e_contact_editor_fullname);
		g_value_set_pointer (value, e_contact_name_copy(e_contact_editor_fullname->name));
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, e_contact_editor_fullname->editable ? TRUE : FALSE);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
fill_in_field(EContactEditorFullname *editor, char *field, char *string)
{
	GtkEntry *entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, field));
	if (entry) {
		if (string)
			gtk_entry_set_text(entry, string);
		else
			gtk_entry_set_text(entry, "");
	}
}

static void
fill_in_info(EContactEditorFullname *editor)
{
	EContactName *name = editor->name;
	if (name) {
		fill_in_field(editor, "entry-title",  name->prefixes);
		fill_in_field(editor, "entry-first",  name->given);
		fill_in_field(editor, "entry-middle", name->additional);
		fill_in_field(editor, "entry-last",   name->family);
		fill_in_field(editor, "entry-suffix", name->suffixes);
	}
}

static char *
extract_field(EContactEditorFullname *editor, char *field)
{
	GtkEntry *entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, field));
	if (entry)
		return g_strdup (gtk_entry_get_text(entry));
	else
		return NULL;
}

static void
extract_info(EContactEditorFullname *editor)
{
	EContactName *name = editor->name;
	if (!name) {
		name = e_contact_name_new();
		editor->name = name;
	}

	name->prefixes   = extract_field(editor, "entry-title" );
	name->given      = extract_field(editor, "entry-first" );
	name->additional = extract_field(editor, "entry-middle");
	name->family     = extract_field(editor, "entry-last"  );
	name->suffixes   = extract_field(editor, "entry-suffix");
}
