/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-address-dialog.c
 *
 * Copyright (C) 1999  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#include <gnome.h>
#include "e-msg-composer-address-dialog.h"


enum {
	APPLY,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static GnomeDialogClass *parent_class = NULL;


/* This function should load the addresses we know of into the dialog.  We
   don't have a precise setup for the addressbook yet, so we will just put some
   fake entries in.  */
static void
load_addresses (EMsgComposerAddressDialog *dialog)
{
	gchar *text[][3] = {
		{ "Bertrand Guiheneuf", "Bertrand.Guiheneuf@aful.org", NULL },
		{ "Ettore Perazzoli", "ettore@gnu.org", NULL },
		{ "Miguel de Icaza", "miguel@gnu.org", NULL },
		{ "Nat Friedman", "nat@nat.org", NULL },
		{ NULL, NULL, NULL }
	};
	GtkCList *clist;
	guint i;

	clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, "address_clist"));

	for (i = 0; text[i][0] != NULL; i++)
		gtk_clist_append (clist, text[i]);
}

/* This loads the selected address in the address GtkCList into the requested
   GtkList.  */
static void
add_address (EMsgComposerAddressDialog *dialog,
	     const gchar *list_name)
{
	GtkCList *src_clist;
	GtkCList *dest_clist;
	guint row;
	gchar *text[2];

	src_clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, "address_clist"));
	dest_clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, list_name));
	row = GPOINTER_TO_INT (src_clist->selection->data);

	gtk_clist_get_text (src_clist, row, 0, &text[0]);
	text[1] = NULL;
	gtk_clist_append (dest_clist, text);
	gtk_clist_set_row_data (dest_clist, dest_clist->rows - 1,
				GINT_TO_POINTER (row));
}


/* Signals.  */

static void
add_to_cb (GtkWidget *widget,
	   gpointer data)
{
	add_address (E_MSG_COMPOSER_ADDRESS_DIALOG (data), "to_clist");
}

static void
add_cc_cb (GtkWidget *widget,
	   gpointer data)
{
	add_address (E_MSG_COMPOSER_ADDRESS_DIALOG (data), "cc_clist");
}

static void
add_bcc_cb (GtkWidget *widget,
	   gpointer data)
{
	add_address (E_MSG_COMPOSER_ADDRESS_DIALOG (data), "bcc_clist");
}

static void
glade_connect (GladeXML *gui,
	       const gchar *widget_name,
	       const gchar *signal_name,
	       GtkSignalFunc callback,
	       gpointer callback_data)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (gui, widget_name);
	if (widget == NULL)
		g_warning ("Widget `%s' was not found.", widget_name);
	else
		gtk_signal_connect (GTK_OBJECT (widget), signal_name,
				    GTK_SIGNAL_FUNC (callback), callback_data);
}

static void
setup_signals (EMsgComposerAddressDialog *dialog)
{
	glade_connect (dialog->gui, "to_add_button", "clicked", add_to_cb,
		       dialog);
	glade_connect (dialog->gui, "cc_add_button", "clicked", add_cc_cb,
		       dialog);
	glade_connect (dialog->gui, "bcc_add_button", "clicked", add_bcc_cb,
		       dialog);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposerAddressDialog *dialog;
	GtkCList *address_clist;
	GList *p;

	dialog = E_MSG_COMPOSER_ADDRESS_DIALOG (object);

	gtk_object_unref (GTK_OBJECT (dialog->gui));

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* Initialization.  */

static void
class_init (EMsgComposerAddressDialogClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gnome_dialog_get_type ());

	signals[APPLY]
		= gtk_signal_new ("apply",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EMsgComposerAddressDialogClass,
						     apply),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposerAddressDialog *dialog)
{
	dialog->gui = NULL;
}


GtkType
e_msg_composer_address_dialog_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposerAddressDialog",
			sizeof (EMsgComposerAddressDialog),
			sizeof (EMsgComposerAddressDialogClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gnome_dialog_get_type (), &info);
	}

	return type;
}

void
e_msg_composer_address_dialog_construct (EMsgComposerAddressDialog *dialog)
{
	static const gchar *buttons[] = {
		GNOME_STOCK_BUTTON_OK,
		GNOME_STOCK_BUTTON_APPLY,
		GNOME_STOCK_BUTTON_CANCEL,
		NULL
	};

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_ADDRESS_DIALOG (dialog));

	gnome_dialog_constructv (GNOME_DIALOG (dialog),
				 _("Select recipients' addresses"),
				 buttons);

	dialog->gui = glade_xml_new
		(E_GUIDIR "/e-msg-composer-address-dialog.glade", "main_table");
	if (dialog->gui == NULL) {
		g_warning ("Cannot load `e-msg-composer-address-dialog.glade");
		return;
	}

	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG (dialog)->vbox),
			   glade_xml_get_widget (dialog->gui, "main_table"));

	load_addresses (dialog);
	setup_signals (dialog);
}

GtkWidget *
e_msg_composer_address_dialog_new (void)
{
	EMsgComposerAddressDialog *new;

	new = gtk_type_new (e_msg_composer_address_dialog_get_type ());
	e_msg_composer_address_dialog_construct (new);

	return GTK_WIDGET (new);
}


static gchar *
make_full_address (const gchar *name,
		   const gchar *email)
{
	/* FIXME handle quoting.  */

	return g_strconcat (name, " <", email, ">", NULL);
}

static GList *
get_list (EMsgComposerAddressDialog *dialog,
	  const gchar *clist_name)
{
	GtkCList *address_clist;
	GtkCList *clist;
	GList *list;
	guint i;

	address_clist = GTK_CLIST (glade_xml_get_widget (dialog->gui,
							 "address_clist"));
	clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, clist_name));

	list = NULL;
	for (i = 0; i < clist->rows; i++) {
		gchar *name, *email;
		guint addr_row;

		addr_row = GPOINTER_TO_INT (gtk_clist_get_row_data (clist, i));
		gtk_clist_get_text (clist, addr_row, 0, &name);
		gtk_clist_get_text (clist, addr_row, 0, &email);

		list = g_list_prepend (list, make_full_address (name, email));
	}

	return g_list_reverse (list);
}

GList *
e_msg_composer_address_dialog_get_to_list (EMsgComposerAddressDialog *dialog)
{
	g_return_val_if_fail (dialog != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_ADDRESS_DIALOG (dialog), NULL);

	return get_list (dialog, "to_clist");
}

GList *
e_msg_composer_address_dialog_get_cc_list (EMsgComposerAddressDialog *dialog)
{
	g_return_val_if_fail (dialog != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_ADDRESS_DIALOG (dialog), NULL);

	return get_list (dialog, "cc_clist");
}

GList *
e_msg_composer_address_dialog_get_bcc_list (EMsgComposerAddressDialog *dialog)
{
	g_return_val_if_fail (dialog != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_ADDRESS_DIALOG (dialog), NULL);

	return get_list (dialog, "bcc_clist");
}
