/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-editor-fullname.c
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

#include <config.h>
#include <gnome.h>
#include <e-contact-editor-fullname.h>

static void e_contact_editor_fullname_init		(EContactEditorFullname		 *card);
static void e_contact_editor_fullname_class_init	(EContactEditorFullnameClass	 *klass);
static void e_contact_editor_fullname_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_contact_editor_fullname_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_contact_editor_fullname_destroy (GtkObject *object);

static void fill_in_info(EContactEditorFullname *editor);
static void extract_info(EContactEditorFullname *editor);

static GnomeDialogClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_NAME
};

GtkType
e_contact_editor_fullname_get_type (void)
{
	static GtkType contact_editor_fullname_type = 0;

	if (!contact_editor_fullname_type)
		{
			static const GtkTypeInfo contact_editor_fullname_info =
			{
				"EContactEditorFullname",
				sizeof (EContactEditorFullname),
				sizeof (EContactEditorFullnameClass),
				(GtkClassInitFunc) e_contact_editor_fullname_class_init,
				(GtkObjectInitFunc) e_contact_editor_fullname_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
				(GtkClassInitFunc) NULL,
			};

			contact_editor_fullname_type = gtk_type_unique (gnome_dialog_get_type (), &contact_editor_fullname_info);
		}

	return contact_editor_fullname_type;
}

static void
e_contact_editor_fullname_class_init (EContactEditorFullnameClass *klass)
{
	GtkObjectClass *object_class;
	GnomeDialogClass *dialog_class;

	object_class = (GtkObjectClass*) klass;
	dialog_class = (GnomeDialogClass *) klass;

	parent_class = gtk_type_class (gnome_dialog_get_type ());

	gtk_object_add_arg_type ("EContactEditorFullname::name", GTK_TYPE_POINTER, 
				 GTK_ARG_READWRITE, ARG_NAME);
 
	object_class->set_arg = e_contact_editor_fullname_set_arg;
	object_class->get_arg = e_contact_editor_fullname_get_arg;
	object_class->destroy = e_contact_editor_fullname_destroy;
}

static void
e_contact_editor_fullname_init (EContactEditorFullname *e_contact_editor_fullname)
{
	GladeXML *gui;
	GtkWidget *widget;

	gnome_dialog_append_button ( GNOME_DIALOG(e_contact_editor_fullname),
				     GNOME_STOCK_BUTTON_OK);
	
	gnome_dialog_append_button ( GNOME_DIALOG(e_contact_editor_fullname),
				     GNOME_STOCK_BUTTON_CANCEL);

	gtk_window_set_policy(GTK_WINDOW(e_contact_editor_fullname), TRUE, TRUE, FALSE);

	e_contact_editor_fullname->name = NULL;
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/fullname.glade", NULL);
	e_contact_editor_fullname->gui = gui;

	widget = glade_xml_get_widget(gui, "vbox-checkfullname");
	gtk_widget_ref(widget);
	gtk_container_remove(GTK_CONTAINER(widget->parent), widget);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (e_contact_editor_fullname)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);
}

void
e_contact_editor_fullname_destroy (GtkObject *object)
{
	EContactEditorFullname *e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME(object);

	if (e_contact_editor_fullname->gui)
		gtk_object_unref(GTK_OBJECT(e_contact_editor_fullname->gui));
	e_card_name_free(e_contact_editor_fullname->name);
}

GtkWidget*
e_contact_editor_fullname_new (ECardName *name)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_contact_editor_fullname_get_type ()));
	gtk_object_set (GTK_OBJECT(widget),
			"name", name,
			NULL);
	return widget;
}

static void
e_contact_editor_fullname_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EContactEditorFullname *e_contact_editor_fullname;

	e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME (o);
	
	switch (arg_id){
	case ARG_NAME:
		if (e_contact_editor_fullname->name)
			e_card_name_free(e_contact_editor_fullname->name);
		e_contact_editor_fullname->name = e_card_name_copy(GTK_VALUE_POINTER (*arg));
		fill_in_info(e_contact_editor_fullname);
		break;
	}
}

static void
e_contact_editor_fullname_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EContactEditorFullname *e_contact_editor_fullname;

	e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME (object);

	switch (arg_id) {
	case ARG_NAME:
		extract_info(e_contact_editor_fullname);
		GTK_VALUE_POINTER (*arg) = e_card_name_copy(e_contact_editor_fullname->name);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
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
	ECardName *name = editor->name;
	if (name) {
		fill_in_field(editor, "entry-title",  name->prefix);
		fill_in_field(editor, "entry-first",  name->given);
		fill_in_field(editor, "entry-middle", name->additional);
		fill_in_field(editor, "entry-last",   name->family);
		fill_in_field(editor, "entry-suffix", name->suffix);
	}
}

static char *
extract_field(EContactEditorFullname *editor, char *field)
{
	GtkEntry *entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, field));
	if (entry)
		return g_strdup(gtk_entry_get_text(entry));
	else
		return NULL;
}

static void
extract_info(EContactEditorFullname *editor)
{
	ECardName *name = editor->name;
	if (!name)
		name = e_card_name_new();
	name->prefix     = extract_field(editor, "entry-title" );
	name->given      = extract_field(editor, "entry-first" );
	name->additional = extract_field(editor, "entry-middle");
	name->family     = extract_field(editor, "entry-last"  );
	name->suffix     = extract_field(editor, "entry-suffix");
}
