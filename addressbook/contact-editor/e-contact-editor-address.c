/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-editor-address.c
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
#include <e-util/e-unicode.h>
#include <e-contact-editor-address.h>

static void e_contact_editor_address_init		(EContactEditorAddress		 *card);
static void e_contact_editor_address_class_init	(EContactEditorAddressClass	 *klass);
static void e_contact_editor_address_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_contact_editor_address_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_contact_editor_address_destroy (GtkObject *object);

static void fill_in_info(EContactEditorAddress *editor);
static void extract_info(EContactEditorAddress *editor);

static GnomeDialogClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_ADDRESS
};

GtkType
e_contact_editor_address_get_type (void)
{
	static GtkType contact_editor_address_type = 0;

	if (!contact_editor_address_type)
		{
			static const GtkTypeInfo contact_editor_address_info =
			{
				"EContactEditorAddress",
				sizeof (EContactEditorAddress),
				sizeof (EContactEditorAddressClass),
				(GtkClassInitFunc) e_contact_editor_address_class_init,
				(GtkObjectInitFunc) e_contact_editor_address_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
				(GtkClassInitFunc) NULL,
			};

			contact_editor_address_type = gtk_type_unique (gnome_dialog_get_type (), &contact_editor_address_info);
		}

	return contact_editor_address_type;
}

static void
e_contact_editor_address_class_init (EContactEditorAddressClass *klass)
{
	GtkObjectClass *object_class;
	GnomeDialogClass *dialog_class;

	object_class = (GtkObjectClass*) klass;
	dialog_class = (GnomeDialogClass *) klass;

	parent_class = gtk_type_class (gnome_dialog_get_type ());

	gtk_object_add_arg_type ("EContactEditorAddress::address", GTK_TYPE_POINTER, 
				 GTK_ARG_READWRITE, ARG_ADDRESS);
 
	object_class->set_arg = e_contact_editor_address_set_arg;
	object_class->get_arg = e_contact_editor_address_get_arg;
	object_class->destroy = e_contact_editor_address_destroy;
}

static void
e_contact_editor_address_init (EContactEditorAddress *e_contact_editor_address)
{
	GladeXML *gui;
	GtkWidget *widget;

	gnome_dialog_append_button ( GNOME_DIALOG(e_contact_editor_address),
				     GNOME_STOCK_BUTTON_OK);
	
	gnome_dialog_append_button ( GNOME_DIALOG(e_contact_editor_address),
				     GNOME_STOCK_BUTTON_CANCEL);

	gtk_window_set_policy(GTK_WINDOW(e_contact_editor_address), FALSE, TRUE, FALSE);

	e_contact_editor_address->address = NULL;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/fulladdr.glade", NULL);
	e_contact_editor_address->gui = gui;

	widget = glade_xml_get_widget(gui, "table-checkaddress");
	gtk_widget_ref(widget);
	gtk_container_remove(GTK_CONTAINER(widget->parent), widget);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (e_contact_editor_address)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);
}

void
e_contact_editor_address_destroy (GtkObject *object)
{
	EContactEditorAddress *e_contact_editor_address = E_CONTACT_EDITOR_ADDRESS(object);

	if (e_contact_editor_address->gui)
		gtk_object_unref(GTK_OBJECT(e_contact_editor_address->gui));
	e_card_delivery_address_free(e_contact_editor_address->address);
}

GtkWidget*
e_contact_editor_address_new (const ECardDeliveryAddress *address)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_contact_editor_address_get_type ()));
	gtk_object_set (GTK_OBJECT(widget),
			"address", address,
			NULL);
	return widget;
}

static void
e_contact_editor_address_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EContactEditorAddress *e_contact_editor_address;

	e_contact_editor_address = E_CONTACT_EDITOR_ADDRESS (o);
	
	switch (arg_id){
	case ARG_ADDRESS:
		if (e_contact_editor_address->address)
			e_card_delivery_address_free(e_contact_editor_address->address);
		e_contact_editor_address->address = e_card_delivery_address_copy(GTK_VALUE_POINTER (*arg));
		fill_in_info(e_contact_editor_address);
		break;
	}
}

static void
e_contact_editor_address_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EContactEditorAddress *e_contact_editor_address;

	e_contact_editor_address = E_CONTACT_EDITOR_ADDRESS (object);

	switch (arg_id) {
	case ARG_ADDRESS:
		extract_info(e_contact_editor_address);
		GTK_VALUE_POINTER (*arg) = e_card_delivery_address_copy(e_contact_editor_address->address);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
fill_in_field(EContactEditorAddress *editor, char *field, char *string)
{
	GtkEditable *editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, field));
	if (editable) {
		e_utf8_gtk_editable_set_text(editable, string);
	}
}

static void
fill_in_info(EContactEditorAddress *editor)
{
	ECardDeliveryAddress *address = editor->address;
	if (address) {
		fill_in_field(editor, "text-street"  , address->street );
		fill_in_field(editor, "entry-po"     , address->po     );
		fill_in_field(editor, "entry-ext"    , address->ext    );
		fill_in_field(editor, "entry-city"   , address->city   );
		fill_in_field(editor, "entry-region" , address->region );
		fill_in_field(editor, "entry-code"   , address->code   );
		fill_in_field(editor, "entry-country", address->country);
	}
}

static char *
extract_field(EContactEditorAddress *editor, char *field)
{
	GtkEditable *editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, field));
	if (editable)
		return e_utf8_gtk_editable_get_text(editable);
	else
		return NULL;
}

static void
extract_info(EContactEditorAddress *editor)
{
	ECardDeliveryAddress *address = editor->address;
	if (!address)
		address = e_card_delivery_address_new();
	address->street  = extract_field(editor, "text-street"  );
	address->po      = extract_field(editor, "entry-po"     );
	address->ext     = extract_field(editor, "entry-ext"    );
	address->city    = extract_field(editor, "entry-city"   );
	address->region  = extract_field(editor, "entry-region" );
	address->code    = extract_field(editor, "entry-code"   );
	address->country = extract_field(editor, "entry-country");
}
