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

/* Combine name and email into an address, e.g. "Ettore Perazzoli
   <ettore@gnu.org>".  FIXME FIXME FIXME this does not handle quoting (commas
   will cause troubles), but it should.  */
static gchar *
make_full_address (const gchar *name,
		   const gchar *email)
{
	return g_strconcat (name, " <", email, ">", NULL);
}

/* This loads the selected address in the address GtkCList into the requested
   GtkList.  */
static void
add_address (EMsgComposerAddressDialog *dialog,
	     const gchar *list_name)
{
	GtkCList *src_clist;
	GtkCList *dest_clist;
	gchar *name, *email;
	gchar *text[2];
	guint row;

	src_clist = GTK_CLIST (glade_xml_get_widget (dialog->gui,
						     "address_clist"));
	if (src_clist->selection == NULL)
		return;

	dest_clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, list_name));
	row = GPOINTER_TO_INT (src_clist->selection->data);

	gtk_clist_get_text (src_clist, row, 0, &name);
	gtk_clist_get_text (src_clist, row, 1, &email);

	text[0] = make_full_address (name, email);
	text[1] = NULL;

	gtk_clist_append (dest_clist, text);

	g_free (text[0]);
}

static void
apply (EMsgComposerAddressDialog *dialog)
{
	gtk_signal_emit (GTK_OBJECT (dialog), signals[APPLY]);
}


/* Recipient list popup menu.  */

struct _RecipientListInfo {
	EMsgComposerAddressDialog *dialog;
	GtkCList *clist;
	gint row;		/* -1 if menu was popped up in an empty
				   area.  */
};
typedef struct _RecipientListInfo RecipientListInfo;

static void
copy_recipient (RecipientListInfo *info,
		gboolean remove)
{
	gchar *text;
	gint row;

	if (info->clist->selection == NULL)
		return;

	row = GPOINTER_TO_INT (info->clist->selection->data);
	gtk_clist_get_text (info->clist, row, 0, &text);

	g_free (info->dialog->cut_buffer);
	info->dialog->cut_buffer = g_strdup (text);

	if (remove)
		gtk_clist_remove (info->clist, row);

	gtk_selection_owner_set (GTK_WIDGET (info->clist),
				 GDK_SELECTION_PRIMARY,
				 GDK_CURRENT_TIME);
}

static void
copy_recipient_cb (GtkWidget *widget,
		   gpointer data)
{
	RecipientListInfo *info;

	info = (RecipientListInfo *) data;
	copy_recipient (info, FALSE);
	g_free (info);
}

static void
cut_recipient_cb (GtkWidget *widget,
		  gpointer data)
{
	RecipientListInfo *info;

	info = (RecipientListInfo *) data;
	copy_recipient (info, TRUE);
	g_free (info);
}

static void
paste_recipient_cb (GtkWidget *widget,
		    gpointer data)
{
	RecipientListInfo *info;
	GdkAtom atom;
	gchar *text[2];

	info = (RecipientListInfo *) data;

	atom = gdk_atom_intern ("STRING", FALSE);
	gtk_selection_convert (GTK_WIDGET (info->clist),
			       GDK_SELECTION_PRIMARY,
			       atom,
			       GDK_CURRENT_TIME);

	g_free (info);
}

static GnomeUIInfo recipient_list_item_popup_info[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Cut"),
				N_("Cut selected item into clipboard"),
				cut_recipient_cb,
				GNOME_STOCK_MENU_CUT),
	GNOMEUIINFO_ITEM_STOCK (N_("Copy"),
				N_("Copy selected item into clipboard"),
				copy_recipient_cb,
				GNOME_STOCK_MENU_COPY),
	GNOMEUIINFO_ITEM_STOCK (N_("Paste"),
				N_("Paste item from clipboard"),
				paste_recipient_cb,
				GNOME_STOCK_MENU_PASTE),
	GNOMEUIINFO_END
};

static GnomeUIInfo recipient_list_popup_info[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Paste"),
				N_("Paste item from clipboard"),
				paste_recipient_cb,
				GNOME_STOCK_MENU_PASTE),
	GNOMEUIINFO_END
};


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

static gint
recipient_clist_button_press_cb (GtkWidget *widget,
				 GdkEventButton *event,
				 gpointer data)
{
	EMsgComposerAddressDialog *dialog;
	RecipientListInfo *info;
	GtkWidget *popup;
	GtkCList *clist;
	gboolean on_row;
	gint row, column;

	dialog = E_MSG_COMPOSER_ADDRESS_DIALOG (data);

	clist = GTK_CLIST (widget);

	if (event->window != clist->clist_window || event->button != 3)
		return FALSE;

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y,
					       &row, &column);

	info = g_new (RecipientListInfo, 1);
	info->dialog = dialog;
	info->clist = clist;

	if (on_row) {
		gtk_clist_unselect_all (clist);
		gtk_clist_select_row (clist, row, 0);
		info->row = row;
		popup = gnome_popup_menu_new (recipient_list_item_popup_info);
	} else {
		info->row = -1;
		popup = gnome_popup_menu_new (recipient_list_popup_info);
	}

	gnome_popup_menu_do_popup_modal (popup, NULL, NULL, event, info);

	gtk_widget_destroy (popup);

	return TRUE;
}

/* FIXME needs more work.  */
static void
recipient_clist_selection_received_cb (GtkWidget *widget,
				       GtkSelectionData *selection_data,
				       guint time,
				       gpointer data)
{
	GtkCList *clist;
	gchar *text[2];
	gchar *p;

	puts (__FUNCTION__);

	if (selection_data->length < 0)
		return;

	clist = GTK_CLIST (widget);

	/* FIXME quoting.  */
	text[0] = g_strdup (selection_data->data);
	text[1] = NULL;

	/* It is a common mistake to paste `\n's, let's work around that.  */
	for (p = text[0]; *p != '\0'; p++) {
		if (*p == '\n') {
			*p = '\0';
			break;
		}
	}

	if (clist->selection != NULL) {
		gint row;

		row = GPOINTER_TO_INT (clist->selection->data);
		gtk_clist_insert (clist, row, text);
	} else {
		gtk_clist_append (clist, text);
	}

	g_free (text[0]);
}

static void
recipient_clist_selection_get_cb (GtkWidget *widget, 
				  GtkSelectionData *selection_data,
				  guint info,
				  guint time,
				  gpointer data)
{
	EMsgComposerAddressDialog *dialog;
	GdkAtom atom;

	puts (__FUNCTION__);

	dialog = E_MSG_COMPOSER_ADDRESS_DIALOG (data);
	if (dialog->cut_buffer == NULL)
		return;		/* FIXME should I do something special?  */

	atom = gdk_atom_intern ("STRING", FALSE);
	gtk_selection_data_set (selection_data, atom, 8,
				dialog->cut_buffer,
				strlen (dialog->cut_buffer));
}

static void
recipient_clist_selection_clear_event_cb (GtkWidget *widget,
					  GdkEventSelection *selection,
					  gpointer data)
{
	EMsgComposerAddressDialog *dialog;

	dialog = E_MSG_COMPOSER_ADDRESS_DIALOG (data);
	g_free (dialog->cut_buffer);
	dialog->cut_buffer = NULL;
}

static void
setup_recipient_list_signals (EMsgComposerAddressDialog *dialog,
			      const gchar *name)
{
	glade_connect (dialog->gui, name, "button_press_event",
		       GTK_SIGNAL_FUNC (recipient_clist_button_press_cb),
		       dialog);
	glade_connect (dialog->gui, name, "selection_received",
		       GTK_SIGNAL_FUNC (recipient_clist_selection_received_cb),
		       dialog);
	glade_connect (dialog->gui, name, "selection_get",
		       GTK_SIGNAL_FUNC (recipient_clist_selection_get_cb),
		       dialog);
	glade_connect (dialog->gui, name, "selection_clear_event",
		       GTK_SIGNAL_FUNC (recipient_clist_selection_clear_event_cb),
		       dialog);
}

static void
setup_signals (EMsgComposerAddressDialog *dialog)
{
	glade_connect (dialog->gui, "to_add_button", "clicked",
		       GTK_SIGNAL_FUNC (add_to_cb), dialog);
	glade_connect (dialog->gui, "cc_add_button", "clicked",
		       GTK_SIGNAL_FUNC (add_cc_cb), dialog);
	glade_connect (dialog->gui, "bcc_add_button", "clicked",
		       GTK_SIGNAL_FUNC (add_bcc_cb), dialog);

	setup_recipient_list_signals (dialog, "to_clist");
	setup_recipient_list_signals (dialog, "cc_clist");
	setup_recipient_list_signals (dialog, "bcc_clist");
}


static void
setup_selection_targets (EMsgComposerAddressDialog *dialog)
{
	gtk_selection_add_target (glade_xml_get_widget (dialog->gui, "to_clist"),
				  GDK_SELECTION_PRIMARY,
				  GDK_SELECTION_TYPE_STRING, 0);
	gtk_selection_add_target (glade_xml_get_widget (dialog->gui, "cc_clist"),
				  GDK_SELECTION_PRIMARY,
				  GDK_SELECTION_TYPE_STRING, 0);
	gtk_selection_add_target (glade_xml_get_widget (dialog->gui, "bcc_clist"),
				  GDK_SELECTION_PRIMARY,
				  GDK_SELECTION_TYPE_STRING, 0);
}


/* GnomeDialog methods.  */

static void
clicked (GnomeDialog *dialog,
	 gint button_number)
{
	switch (button_number) {
	case 0:			/* OK */
		apply (E_MSG_COMPOSER_ADDRESS_DIALOG (dialog));
		gnome_dialog_close (dialog);
		break;
	case 1:			/* Apply */
		apply (E_MSG_COMPOSER_ADDRESS_DIALOG (dialog));
		break;
	case 2:			/* Cancel */
		gnome_dialog_close (dialog);
		break;
	}
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
	g_free (dialog->cut_buffer);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* Initialization.  */

static void
class_init (EMsgComposerAddressDialogClass *class)
{
	GtkObjectClass *object_class;
	GnomeDialogClass *gnome_dialog_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	gnome_dialog_class = GNOME_DIALOG_CLASS (class);
	gnome_dialog_class->clicked = clicked;

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
	dialog->cut_buffer = NULL;
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
		(E_GLADEDIR "/e-msg-composer-address-dialog.glade",
		 "main_table");
	if (dialog->gui == NULL) {
		g_warning ("Cannot load `e-msg-composer-address-dialog.glade");
		return;
	}

	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG (dialog)->vbox),
			   glade_xml_get_widget (dialog->gui, "main_table"));

	setup_selection_targets (dialog);
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


static void
set_list (EMsgComposerAddressDialog *dialog,
	  const gchar *list_name,
	  GList *list)
{
	GtkCList *clist;
	GList *p;
	gchar *text[2];

	clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, list_name));

	gtk_clist_freeze (clist);
	gtk_clist_clear (clist);

	text[1] = NULL;
	for (p = list; p != NULL; p = p->next) {
		text[0] = (gchar *) p->data;
		gtk_clist_append (clist, text);
	}

	gtk_clist_thaw (clist);
}

void
e_msg_composer_address_dialog_set_to_list (EMsgComposerAddressDialog *dialog, 
					   GList *to_list)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_ADDRESS_DIALOG (dialog));

	set_list (dialog, "to_clist", to_list);
}

void
e_msg_composer_address_dialog_set_cc_list (EMsgComposerAddressDialog *dialog, 
					   GList *cc_list)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_ADDRESS_DIALOG (dialog));

	set_list (dialog, "cc_clist", cc_list);
}

void
e_msg_composer_address_dialog_set_bcc_list (EMsgComposerAddressDialog *dialog, 
					    GList *bcc_list)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_ADDRESS_DIALOG (dialog));

	set_list (dialog, "bcc_clist", bcc_list);
}


static GList *
get_list (EMsgComposerAddressDialog *dialog,
	  const gchar *clist_name)
{
	GtkCList *address_clist;
	GtkCList *clist;
	GList *list;
	guint i;

	clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, clist_name));

	list = NULL;
	for (i = 0; i < clist->rows; i++) {
		gchar *addr;

		gtk_clist_get_text (clist, i, 0, &addr);
		list = g_list_prepend (list, g_strdup (addr));
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
