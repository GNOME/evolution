/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-editor.c
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

#include <gnome.h>
#include "e-contact-editor.h"
static void e_contact_editor_init		(EContactEditor		 *card);
static void e_contact_editor_class_init	(EContactEditorClass	 *klass);
static void e_contact_editor_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_contact_editor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static GtkNotebookClass *parent_class = NULL;

#if 0
enum {
	E_CONTACT_EDITOR_RESIZE,
	E_CONTACT_EDITOR_LAST_SIGNAL
};

static guint e_contact_editor_signals[E_CONTACT_EDITOR_LAST_SIGNAL] = { 0 };
#endif

/* The arguments we take */
enum {
	ARG_0,
	ARG_CARD
};

GtkType
e_contact_editor_get_type (void)
{
  static GtkType contact_editor_type = 0;

  if (!contact_editor_type)
    {
      static const GtkTypeInfo contact_editor_info =
      {
        "EContactEditor",
        sizeof (EContactEditor),
        sizeof (EContactEditorClass),
        (GtkClassInitFunc) e_contact_editor_class_init,
        (GtkObjectInitFunc) e_contact_editor_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      contact_editor_type = gtk_type_unique (gtk_notebook_get_type (), &contact_editor_info);
    }

  return contact_editor_type;
}

static void
e_contact_editor_class_init (EContactEditorClass *klass)
{
  GtkObjectClass *object_class;
  GtkNotebookClass *notebook_class;

  object_class = (GtkObjectClass*) klass;
  notebook_class = (GtkNotebookClass *) klass;

  parent_class = gtk_type_class (gtk_notebook_get_type ());

#if 0  
  e_contact_editor_signals[E_CONTACT_EDITOR_RESIZE] =
	  gtk_signal_new ("resize",
			  GTK_RUN_LAST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactEditorClass, resize),
			  gtk_marshal_NONE__NONE,
			  GTK_TYPE_NONE, 0);
  
  
  gtk_object_class_add_signals (object_class, e_contact_editor_signals, E_CONTACT_EDITOR_LAST_SIGNAL);
#endif
  
  gtk_object_add_arg_type ("EContactEditor::card", GTK_TYPE_OBJECT, 
			   GTK_ARG_READWRITE, ARG_CARD);
 
  object_class->set_arg = e_contact_editor_set_arg;
  object_class->get_arg = e_contact_editor_get_arg;
}

static GtkWidget *
_create_page_general_name(EContactEditor *e_contact_editor)
{
	GtkWidget *table;
	GtkWidget *alignment;
	table = gtk_table_new(3, 4, FALSE);
	alignment = gtk_alignment_new(0, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(alignment),
			  gnome_pixmap_new_from_file("head.png"));
	gtk_table_attach(GTK_TABLE(table),
			 alignment,
			 0, 1, 0, 4,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Full Name:")),
			 1, 2, 0, 1,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Job Title:")),
			 1, 2, 1, 2,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Company:")),
			 1, 2, 2, 3,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("File as:")),
			 1, 2, 3, 4,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 0, 1,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 1, 2,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 2, 3,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_combo_new(),
			 2, 3, 3, 4,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	return table;
}

static GtkWidget *
_create_page_general_phone(EContactEditor *e_contact_editor)
{
	GtkWidget *table;
	GtkWidget *alignment;
	table = gtk_table_new(3, 4, FALSE);
	alignment = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(alignment),
			  gnome_pixmap_new_from_file("phone.png"));
	
	gtk_table_attach(GTK_TABLE(table),
			 alignment,
			 0, 1, 0, 4,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Home:")),
			 1, 2, 0, 1,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Car:")),
			 1, 2, 1, 2,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Mobile:")),
			 1, 2, 2, 3,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Business Fax:")),
			 1, 2, 3, 4,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 0, 1,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 1, 2,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 2, 3,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 3, 4,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	return table;
}

static GtkWidget *
_create_page_general_email(EContactEditor *e_contact_editor)
{
	GtkWidget *table;
	GtkWidget *alignment;
	table = gtk_table_new(3, 1, FALSE);
	alignment = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(alignment),
			  gnome_pixmap_new_from_file("email.png"));
	
	gtk_table_attach(GTK_TABLE(table),
			 alignment,
			 0, 1, 0, 1,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Email:")),
			 1, 2, 0, 1,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 0, 1,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	return table;
}

static GtkWidget *
_create_page_general_web(EContactEditor *e_contact_editor)
{
	GtkWidget *table;
	GtkWidget *alignment;
	table = gtk_table_new(3, 1, FALSE);
	alignment = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(alignment),
			  gnome_pixmap_new_from_file("web.png"));
	
	gtk_table_attach(GTK_TABLE(table),
			 alignment,
			 0, 1, 0, 1,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Web page address:")),
			 1, 2, 0, 1,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_entry_new(),
			 2, 3, 0, 1,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	return table;
}

static GtkWidget *
_create_page_general_snailmail(EContactEditor *e_contact_editor)
{
	GtkWidget *table;
	GtkWidget *alignment;
	GtkWidget *text;
	table = gtk_table_new(3, 3, FALSE);
	alignment = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(alignment),
			  gnome_pixmap_new_from_file("snailmail.png"));
	
	gtk_table_attach(GTK_TABLE(table),
			 alignment,
			 0, 1, 0, 1,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Address:")),
			 1, 2, 0, 1,
			 0, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_label_new(_("Business:")),
			 1, 2, 1, 2,
			 0, 0,
			 0, 0);
	text = gtk_text_new(NULL, NULL);
	gtk_object_set(GTK_OBJECT(text),
		       "editable", TRUE,
		       NULL);
	gtk_table_attach(GTK_TABLE(table),
			 text,
			 2, 3, 0, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 gtk_check_button_new_with_label(_("This is the mailing address")),
			 2, 3, 2, 3,
			 0, 0,
			 0, 0);
	return table;
}

static GtkWidget *
_create_page_general_comments(EContactEditor *e_contact_editor)
{
	GtkWidget *text;
	text = gtk_text_new(NULL, NULL);
	gtk_object_set (GTK_OBJECT(text),
			"editable", TRUE,
			NULL);
	return text;
}

static GtkWidget *
_create_page_general_extras(EContactEditor *e_contact_editor)
{
	return gtk_entry_new();
}

static GtkWidget *
_create_page_general( EContactEditor *e_contact_editor )
{
	GtkWidget *table;
	table = gtk_table_new(2, 5, FALSE);
	gtk_table_attach(GTK_TABLE(table),
			 _create_page_general_name(e_contact_editor),
			 0, 1,
			 0, 1,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 _create_page_general_phone(e_contact_editor),
			 1, 2,
			 0, 1,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 _create_page_general_email(e_contact_editor),
			 0, 1,
			 1, 2,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 _create_page_general_web(e_contact_editor),
			 0, 1,
			 2, 3,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 _create_page_general_snailmail(e_contact_editor),
			 1, 2,
			 1, 3,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 _create_page_general_comments(e_contact_editor),
			 0, 2,
			 3, 4,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);
	gtk_table_attach(GTK_TABLE(table),
			 _create_page_general_extras(e_contact_editor),
			 0, 2,
			 4, 5,
			 GTK_FILL | GTK_EXPAND, 0,
			 0, 0);
	return table;
}

static void
e_contact_editor_init (EContactEditor *e_contact_editor)
{
	GtkNotebook *notebook = GTK_NOTEBOOK (e_contact_editor);

	/*   e_contact_editor->card = NULL;*/
	e_contact_editor->fields = NULL;

	gtk_notebook_append_page (notebook,
				  _create_page_general(e_contact_editor),
				  gtk_label_new(_("General")));
}

GtkWidget*
e_contact_editor_new (void *card)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_contact_editor_get_type ()));
	gtk_object_set (GTK_OBJECT(widget),
			"card", card,
			NULL);
	return widget;
}

static void
e_contact_editor_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EContactEditor *e_contact_editor;

	e_contact_editor = E_CONTACT_EDITOR (o);
	
	switch (arg_id){
	case ARG_CARD:
	  /*	  e_contact_editor->card = GTK_VALUE_POINTER (*arg);
	  _update_card(e_contact_editor);
	  gnome_canvas_item_request_update (item);*/
	  break;
	}
}

static void
e_contact_editor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EContactEditor *e_contact_editor;

	e_contact_editor = E_CONTACT_EDITOR (object);

	switch (arg_id) {
	case ARG_CARD:
	  /* GTK_VALUE_POINTER (*arg) = e_contact_editor->card; */
	  break;
	default:
	  arg->type = GTK_TYPE_INVALID;
	  break;
	}
}
