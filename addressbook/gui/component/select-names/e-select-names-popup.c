/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-select-names-popup.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtklabel.h>
#include <libgnome/gnome-i18n.h>

#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/gui/contact-editor/e-contact-editor.h>
#include <addressbook/gui/contact-editor/e-contact-quick-add.h>
#include "e-addressbook-util.h"
#include "e-select-names-popup.h"

#define LIST_ICON_FILENAME "contact-list-16.png"
#define CONTACT_ICON_FILENAME "evolution-contacts-mini.png"

typedef struct _PopupInfo PopupInfo;
struct _PopupInfo {
	ESelectNamesTextModel *text_model;
	EDestination *dest;
	gint pos;
	gint index;
};

static PopupInfo *
popup_info_new (ESelectNamesTextModel *text_model, EDestination *dest, gint pos, gint index)
{
	PopupInfo *info = g_new0 (PopupInfo, 1);
	info->text_model = text_model;
	info->dest = dest;
	info->pos = pos;
	info->index = index;

	if (text_model)
		g_object_ref (text_model);

	if (dest)
		g_object_ref (dest);

	return info;
}

static void
popup_info_free (PopupInfo *info)
{
	if (info) {
		
		if (info->text_model)
			g_object_unref (info->text_model);

		if (info->dest)
			g_object_unref (info->dest);

		g_free (info);
	}
}

static void
popup_info_cleanup (GtkWidget *w, gpointer info)
{
	popup_info_free ((PopupInfo *) info);
}

/* You are in a maze of twisty little callbacks, all alike... */

static void
make_contact_editor_cb (EBook *book, gpointer user_data)
{
	if (book) {
		EDestination *dest = E_DESTINATION (user_data);
		ECard *card;

		card = (ECard *) e_destination_get_card (dest);
		if (e_card_evolution_list (card)) {
			EContactListEditor *ce;
			ce = e_addressbook_show_contact_list_editor (book, card, FALSE, TRUE);
			e_contact_list_editor_raise (ce);
		}
		else {
			EContactEditor *ce;
			ce = e_addressbook_show_contact_editor (book, card, FALSE, TRUE);
			e_contact_editor_raise (ce);
		}
		g_object_unref (dest);
	}
}

#if TOO_MANY_MENU_ITEMS
static void
edit_contact_info_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	if (info == NULL)
		return;

	g_object_ref (info->dest);
	e_book_use_default_book (make_contact_editor_cb, (gpointer) info->dest);
}
#endif

static void
change_email_num_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	gint n;
	EDestination *dest;
	
	if (info == NULL) 
		return;

	if (! GTK_CHECK_MENU_ITEM (w)->active)
		return;

	n = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "number"));

	if (n != e_destination_get_email_num (info->dest)) {
		dest = e_destination_new ();
		e_destination_set_card (dest, e_destination_get_card (info->dest), n);
		e_select_names_model_replace (info->text_model->source, info->index, dest);
	}
}

#if TOO_MANY_MENU_ITEMS
static void
remove_recipient_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	e_select_names_model_delete (info->text_model->source, info->index);
}

static void
remove_all_recipients_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	e_select_names_model_delete_all (info->text_model->source);
}

static void
toggle_html_mail_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (w);
	const EDestination *dest;

	if (info == NULL)
		return;

	dest = info->dest;

	item = GTK_CHECK_MENU_ITEM (item);
	e_destination_set_html_mail_pref ((EDestination *) dest, item->active);
}
#endif

static void
populate_popup_card (GtkWidget *pop, gboolean list, PopupInfo *info)
{
	GtkWidget *image;
	ECard *card;
	EIterator *iterator;
	GtkWidget *menuitem;

	card = e_destination_get_card (info->dest);

#if TOO_MANY_MENU_ITEMS
	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

	menuitem = gtk_menu_item_new_with_label (_("Remove All"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (remove_all_recipients_cb),
			  info);
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

	menuitem = gtk_menu_item_new_with_label (_("Remove"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (remove_recipient_cb),
			  info);
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

	menuitem = gtk_menu_item_new_with_label (list ? _("View Contact List") : _("View Contact Info"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (edit_contact_info_cb),
			  info);
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

	menuitem = gtk_check_menu_item_new_with_label (_("Send HTML Mail?"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					e_destination_get_html_mail_pref (info->dest));
	g_signal_connect (menuitem, "toggled",
			  G_CALLBACK (toggle_html_mail_cb),
			  info);
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
#endif

	if (card->email) {
		menuitem = gtk_separator_menu_item_new();
		gtk_widget_show (menuitem);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
		
		if (e_list_length (card->email) > 1) {
			GSList *radiogroup = NULL;
			gint n = e_destination_get_email_num (info->dest);
			gint j = e_list_length (card->email) - 1;

			iterator = e_list_get_iterator (card->email);
			for (e_iterator_last (iterator); e_iterator_is_valid (iterator); e_iterator_prev (iterator)) {
				char *email = (char *)e_iterator_get (iterator);
				char *label = NULL;

				if (!strncmp (email, "<?xml", 5)) {
					EDestination *dest = e_destination_import (email);
					if (dest) {
						label = g_strdup (e_destination_get_textrep (dest, TRUE));
						g_object_unref (dest);
					}
				}
				else {
					label = g_strdup (email);
				}

				if (list) {
					menuitem = gtk_menu_item_new_with_label (label);
				}
				else {
					menuitem = gtk_radio_menu_item_new_with_label (radiogroup, label);
					g_signal_connect (menuitem, "toggled",
							  G_CALLBACK (change_email_num_cb),
							  info);
					if (j == n) 
						gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), TRUE);

					g_object_set_data (G_OBJECT (menuitem), "number", GINT_TO_POINTER (j));
					radiogroup = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));
				}

				gtk_widget_show (menuitem);
				gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

				j--;

				g_free (label);
			}

			g_object_unref (iterator);
		} else {
			menuitem = gtk_menu_item_new_with_label (e_destination_get_email (info->dest));
			gtk_widget_show (menuitem);
			gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
		}
	}

	menuitem = gtk_separator_menu_item_new ();
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

	image = gtk_image_new_from_file (list
					 ? EVOLUTION_IMAGESDIR "/" LIST_ICON_FILENAME
					 : EVOLUTION_IMAGESDIR "/" CONTACT_ICON_FILENAME);
	gtk_widget_show (image);
	menuitem = gtk_image_menu_item_new_with_label (e_destination_get_name (info->dest));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem),
				       image);
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
}

static void
quick_add_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	e_contact_quick_add_free_form (e_destination_get_address (info->dest), NULL, NULL);
}

static void
populate_popup_nocard (GtkWidget *pop, PopupInfo *info)
{
	const gchar *str;
	GtkWidget *menuitem;

	menuitem = gtk_separator_menu_item_new ();
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

	menuitem = gtk_menu_item_new_with_label (_("Add to Contacts"));
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (quick_add_cb),
			  info);

#if TOO_MANY_MENU_ITEMS
	menuitem = gtk_check_menu_item_new_with_label (_("Send HTML Mail?"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					e_destination_get_html_mail_pref (info->dest));
	g_signal_connect (menuitem, "toggled",
			  G_CALLBACK (toggle_html_mail_cb),
			  info);
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
#endif

	menuitem = gtk_separator_menu_item_new ();
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

	str = e_destination_get_name (info->dest);
	if (! (str && *str))
		str = e_destination_get_email (info->dest);
	if (! (str && *str))
		str = _("Unnamed Contact");

	menuitem = gtk_menu_item_new_with_label (str);
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
}

void
e_select_names_populate_popup (GtkWidget *menu, ESelectNamesTextModel *text_model,
			       GdkEventButton *ev, gint pos, GtkWidget *for_widget)
{
	ESelectNamesModel *model;
	PopupInfo *info;
	EDestination *dest;
	gint index;

	g_return_if_fail (GTK_IS_MENU_SHELL (menu));
	g_return_if_fail (E_IS_SELECT_NAMES_TEXT_MODEL (text_model));
	g_return_if_fail (ev);
	g_return_if_fail (0 <= pos);

	model = text_model->source;

	e_select_names_model_text_pos (model, text_model->seplen, pos, &index, NULL, NULL);
	if (index < 0 || index >= e_select_names_model_count (model))
		return;

	/* XXX yuck, why does this return a const? */
	dest = (EDestination *)e_select_names_model_get_destination (model, index);
	if (e_destination_is_empty (dest))
		return;

	info = popup_info_new (text_model, dest, pos, index);
	
	if (e_destination_contains_card (dest)) {
		populate_popup_card (menu, e_destination_is_evolution_list (dest), info);
	} else {
		populate_popup_nocard (menu, info);
	}

	/* Clean up our info item after we've made our selection. */
	g_signal_connect (menu,
			  "selection-done",
			  G_CALLBACK (popup_info_cleanup),
			  info);
}
