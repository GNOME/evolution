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
#include <libgnome/gnome-i18n.h>

#include <addressbook/util/eab-book-util.h>
#include <addressbook/gui/contact-editor/e-contact-editor.h>
#include <addressbook/gui/contact-list-editor/e-contact-list-editor.h>
#include <addressbook/gui/contact-editor/e-contact-quick-add.h>
#include "eab-gui-util.h"
#include "e-select-names-popup.h"
#include <e-util/e-icon-factory.h>

#define LIST_ICON_NAME "stock_contact-list"
#define CONTACT_ICON_NAME "stock_contact"

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
	g_signal_handlers_disconnect_matched (G_OBJECT (w), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, info);

	popup_info_free ((PopupInfo *) info);
}

/* You are in a maze of twisty little callbacks, all alike... */

#if TOO_MANY_MENU_ITEMS
static void
make_contact_editor_cb (EBook *book, gpointer user_data)
{
	if (book) {
		EDestination *dest = E_DESTINATION (user_data);
		EContact *contact;

		contact = (EContact *) e_destination_get_contact (dest);
		if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
			EContactListEditor *ce;
			ce = e_addressbook_show_contact_list_editor (book, contact, FALSE, TRUE);
			e_contact_list_editor_raise (ce);
		}
		else {
			EContactEditor *ce;
			ce = e_addressbook_show_contact_editor (book, contact, FALSE, TRUE);
			e_contact_editor_raise (ce);
		}
		g_object_unref (dest);
	}
}

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
		e_destination_set_contact (dest, e_destination_get_contact (info->dest), n);
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
populate_popup_contact (GtkWidget *pop, gboolean list, PopupInfo *info)
{
	GdkPixbuf *pixbuf;
	GtkWidget *image;
	EContact *contact;
	GtkWidget *menuitem;
	GList *email_list;

	contact = e_destination_get_contact (info->dest);

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

	email_list = e_contact_get (contact, E_CONTACT_EMAIL);

	if (email_list) {
		menuitem = gtk_separator_menu_item_new();
		gtk_widget_show (menuitem);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
		
		if (g_list_length (email_list) > 1) {
			GList *l;
			GSList *radiogroup = NULL;
			gint n = e_destination_get_email_num (info->dest);
			gint j = g_list_length (email_list) - 1;

			for (l = g_list_last (email_list); l; l = l->prev) {
				char *email = l->data;
				char *label = NULL;

				label = g_strdup (email);

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
		} else {
			menuitem = gtk_menu_item_new_with_label (e_destination_get_email (info->dest));
			gtk_widget_show (menuitem);
			gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);
		}

		g_list_foreach (email_list, (GFunc)g_free, NULL);
		g_list_free (email_list);
	}

	menuitem = gtk_separator_menu_item_new ();
	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (pop), menuitem);

	pixbuf = e_icon_factory_get_icon (list ? LIST_ICON_NAME : CONTACT_ICON_NAME, E_ICON_SIZE_MENU);
	image = gtk_image_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);
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
populate_popup_nocontact (GtkWidget *pop, PopupInfo *info)
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
	if (e_destination_empty (dest))
		return;

	info = popup_info_new (text_model, dest, pos, index);
	
	if (e_destination_get_contact (dest)) {
		populate_popup_contact (menu, e_destination_is_evolution_list (dest), info);
	} else {
		populate_popup_nocontact (menu, info);
	}

	/* Clean up our info item after we've made our selection. */
	g_signal_connect (menu,
			  "selection-done",
			  G_CALLBACK (popup_info_cleanup),
			  info);
}
