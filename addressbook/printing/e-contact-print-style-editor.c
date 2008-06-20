/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-contact-print-style-editor.c
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-contact-print-style-editor.h"
#include "e-util/e-util-private.h"

static void e_contact_print_style_editor_init		(EContactPrintStyleEditor		 *card);
static void e_contact_print_style_editor_class_init	(EContactPrintStyleEditorClass	 *class);
static void e_contact_print_style_editor_finalize       (GObject *object);

static gpointer parent_class;

GType
e_contact_print_style_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EContactPrintStyleEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) e_contact_print_style_editor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (EContactPrintStyleEditor),
			0,    /* n_preallocs */
			(GInstanceInitFunc) e_contact_print_style_editor_init,
			NULL  /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_VBOX, "EContactPrintStyleEditor",
			&type_info, 0);
	}

	return type;
}

static void
e_contact_print_style_editor_class_init (EContactPrintStyleEditorClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_contact_print_style_editor_finalize;
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

static void
e_contact_print_style_editor_finalize (GObject *object)
{
	EContactPrintStyleEditor *e_contact_print_style_editor = E_CONTACT_PRINT_STYLE_EDITOR(object);

	if (e_contact_print_style_editor->gui != NULL) {
		g_object_unref(e_contact_print_style_editor->gui);
		e_contact_print_style_editor->gui = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget*
e_contact_print_style_editor_new (char *filename)
{
	return g_object_new (e_contact_print_style_editor_get_type (), NULL);
}
