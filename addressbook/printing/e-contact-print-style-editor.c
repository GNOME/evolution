/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-print-style-editor.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
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

#include "e-contact-print-style-editor.h"
#include "e-util/e-util-private.h"

static void e_contact_print_style_editor_init		(EContactPrintStyleEditor		 *card);
static void e_contact_print_style_editor_class_init	(EContactPrintStyleEditorClass	 *klass);
static void e_contact_print_style_editor_set_arg        (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_contact_print_style_editor_get_arg        (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_contact_print_style_editor_destroy        (GtkObject *object);

static GtkVBoxClass *parent_class = NULL;


/* The arguments we take */
enum {
	ARG_0,
	ARG_CARD
};

GtkType
e_contact_print_style_editor_get_type (void)
{
  static GtkType contact_print_style_editor_type = 0;

  if (!contact_print_style_editor_type)
    {
      static const GtkTypeInfo contact_print_style_editor_info =
      {
        "EContactPrintStyleEditor",
        sizeof (EContactPrintStyleEditor),
        sizeof (EContactPrintStyleEditorClass),
        (GtkClassInitFunc) e_contact_print_style_editor_class_init,
        (GtkObjectInitFunc) e_contact_print_style_editor_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      contact_print_style_editor_type = gtk_type_unique (gtk_vbox_get_type (), &contact_print_style_editor_info);
    }

  return contact_print_style_editor_type;
}

static void
e_contact_print_style_editor_class_init (EContactPrintStyleEditorClass *klass)
{
  GtkObjectClass *object_class;
  GtkVBoxClass *vbox_class;

  object_class = (GtkObjectClass*) klass;
  vbox_class = (GtkVBoxClass *) klass;

  parent_class = gtk_type_class (gtk_vbox_get_type ());
  
  object_class->set_arg = e_contact_print_style_editor_set_arg;
  object_class->get_arg = e_contact_print_style_editor_get_arg;
  object_class->destroy = e_contact_print_style_editor_destroy;
}

#if 0
static void
_add_image(GtkTable *table, gchar *image, int left, int right, int top, int bottom)
{
	gtk_table_attach(table,
			 gtk_widget_new(gtk_alignment_get_type(),
					"child", gnome_pixmap_new_from_file(image),
					"xalign", (double) 0,
					"yalign", (double) 0,
					"xscale", (double) 0,
					"yscale", (double) 0,
					NULL),
			 left, right, top, bottom,
			 GTK_FILL, GTK_FILL,
			 0, 0);
}
#endif

static void
e_contact_print_style_editor_init (EContactPrintStyleEditor *e_contact_print_style_editor)
{
	GladeXML *gui;
	char *gladefile;

	/*   e_contact_print_style_editor->card = NULL;*/
	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "e-contact-print.glade",
				      NULL);
	gui = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

	e_contact_print_style_editor->gui = gui;
	gtk_widget_reparent(glade_xml_get_widget(gui, "vbox-contact-print-style-editor"),
			    GTK_WIDGET(e_contact_print_style_editor));
}

void
e_contact_print_style_editor_destroy (GtkObject *object)
{
	EContactPrintStyleEditor *e_contact_print_style_editor = E_CONTACT_PRINT_STYLE_EDITOR(object);

	if (e_contact_print_style_editor->gui != NULL) {
		g_object_unref(e_contact_print_style_editor->gui);
		e_contact_print_style_editor->gui = NULL;
	}

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

GtkWidget*
e_contact_print_style_editor_new (char *filename)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_contact_print_style_editor_get_type ()));
	return widget;
}

static void
e_contact_print_style_editor_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EContactPrintStyleEditor *e_contact_print_style_editor;

	e_contact_print_style_editor = E_CONTACT_PRINT_STYLE_EDITOR (o);
	
	switch (arg_id){
	default:
		break;
	}
}

static void
e_contact_print_style_editor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EContactPrintStyleEditor *e_contact_print_style_editor;

	e_contact_print_style_editor = E_CONTACT_PRINT_STYLE_EDITOR (object);

	switch (arg_id) {
	default:
	  arg->type = GTK_TYPE_INVALID;
	  break;
	}
}
