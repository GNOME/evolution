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
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtklabel.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libgnomeui/gnome-popup-menu.h>

#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/gui/contact-editor/e-contact-editor.h>
#include <addressbook/gui/contact-editor/e-contact-quick-add.h>
#include "e-addressbook-util.h"
#include "e-select-names-popup.h"

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

static void
edit_contact_info_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	if (info == NULL)
		return;

	g_object_ref (info->dest);
	e_book_use_default_book (make_contact_editor_cb, (gpointer) info->dest);
}

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

static void
remove_recipient_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	e_select_names_model_delete (info->text_model->source, info->index);
}

static void
add_remove_recipient (GnomeUIInfo *uiinfo, PopupInfo *info)
{
	uiinfo->type = GNOME_APP_UI_ITEM;
	uiinfo->label = _("Remove");
	uiinfo->moreinfo = remove_recipient_cb;
}

static void
remove_all_recipients_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	e_select_names_model_delete_all (info->text_model->source);
}

static void
add_remove_all_recipients (GnomeUIInfo *uiinfo, PopupInfo *info)
{
	uiinfo->type = GNOME_APP_UI_ITEM;
	uiinfo->label = _("Remove All");
	uiinfo->moreinfo = remove_all_recipients_cb;
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

static void
add_html_mail (GnomeUIInfo *uiinfo, PopupInfo *info)
{
	uiinfo->type = GNOME_APP_UI_TOGGLEITEM;
	uiinfo->label = _("Send HTML Mail?");
	uiinfo->moreinfo = toggle_html_mail_cb;
}

static void
init_html_mail (GnomeUIInfo *uiinfo, PopupInfo *info)
{
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (uiinfo->widget),
					e_destination_get_html_mail_pref (info->dest));
}

static void
set_uiinfo_label (GnomeUIInfo *uiinfo, const gchar *str)
{
	GtkWidget *label;
	GList *item_children;
	
	item_children = gtk_container_get_children (GTK_CONTAINER (uiinfo->widget));
	label = item_children->data;
	g_list_free (item_children);
	gtk_label_set_text (GTK_LABEL (label), str);
}

#define ARBITRARY_UIINFO_LIMIT 64
static GtkWidget *
popup_menu_card (PopupInfo *info)
{
	GnomeUIInfo uiinfo[ARBITRARY_UIINFO_LIMIT];
	GnomeUIInfo radioinfo[ARBITRARY_UIINFO_LIMIT];
	gboolean using_radio = FALSE;
	ECard *card;
	gint i=0;
	GtkWidget *pop;
	EIterator *iterator;
	gint html_toggle;
	gint mail_label = -1;
	const gchar *mail_label_str = NULL;

	/*
	 * Build up our GnomeUIInfo array.
	 */

	memset (uiinfo, 0, sizeof (uiinfo));
	memset (radioinfo, 0, sizeof (radioinfo));

	card = e_destination_get_card (info->dest);

	/* Use an empty label for now, we'll fill it later.
	   If we set uiinfo label to contact name here, gnome_popup_menu_new
	   could screw it up trying make a "translation". */
	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = "";
	++i;

	uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
	++i;

	if (card->email) {
		
		if (e_list_length (card->email) > 1) {
			gint j = 0;

			using_radio = TRUE;

			iterator = e_list_get_iterator (card->email);
			for (e_iterator_reset (iterator); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
				gchar *label = (gchar *)e_iterator_get (iterator);
				if (label && *label) {
					radioinfo[j].label = "";
					radioinfo[j].type = GNOME_APP_UI_ITEM;
					radioinfo[j].moreinfo = change_email_num_cb;
					++j;
				}
			}
			g_object_unref (iterator);
			
			radioinfo[j].type = GNOME_APP_UI_ENDOFINFO;
			
			uiinfo[i].type = GNOME_APP_UI_RADIOITEMS;
			uiinfo[i].moreinfo = radioinfo;
			++i;
			
		} else {
			uiinfo[i].type = GNOME_APP_UI_ITEM;
			uiinfo[i].label = "";
			mail_label_str = e_destination_get_email (info->dest);
			mail_label = i;
			++i;
		}

		uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
		++i;
	}

	add_html_mail (&(uiinfo[i]), info);
	html_toggle = i;
	++i;

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = N_("Edit Contact Info");
	uiinfo[i].moreinfo = edit_contact_info_cb;
	++i;

	add_remove_recipient (&(uiinfo[i]), info);
	++i;
	
	add_remove_all_recipients (&(uiinfo[i]), info);
	++i;
		
	uiinfo[i].type = GNOME_APP_UI_ENDOFINFO;

	/*
	 * Now do something with it...
	 */

	pop = gnome_popup_menu_new (uiinfo);

	init_html_mail (&(uiinfo[html_toggle]), info);

	/* Properly handle the names & e-mail addresses so that they don't get leaked and so that
	   underscores are interpreted as key accelerators.  This sucks. */

	set_uiinfo_label (&(uiinfo[0]), e_destination_get_name (info->dest));
	
	if (mail_label >= 0) {
		set_uiinfo_label (&(uiinfo[mail_label]), e_destination_get_email (info->dest));
	}

	if (using_radio) {
		gint n = e_destination_get_email_num (info->dest);
		gint j = 0;
		iterator = e_list_get_iterator (card->email);
		for (e_iterator_reset (iterator); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
			gchar *label = (gchar *)e_iterator_get (iterator);
			if (label && *label) {
				set_uiinfo_label (&(radioinfo[j]), label);
				
				g_object_set_data (G_OBJECT (radioinfo[j].widget), "number", GINT_TO_POINTER (j));

				if (j == n) 
					gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (radioinfo[n].widget), TRUE);
				
				++j;
			}
		}
		g_object_unref (iterator);
	}

	return pop;
}

static GtkWidget *
popup_menu_list (PopupInfo *info)
{
	GnomeUIInfo uiinfo[ARBITRARY_UIINFO_LIMIT];
	GtkWidget *pop;
	const gchar *str;
	gchar *gs;
	gint i = 0, subcount = 0, max_subcount = 10;
	ECard *card;
	EIterator *iterator;

	memset (uiinfo, 0, sizeof (uiinfo));

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = "";
	++i;

	uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
	++i;

	card = e_destination_get_card (info->dest);

	if (card->email) {
		
		iterator = e_list_get_iterator (card->email);
		for (e_iterator_reset (iterator); e_iterator_is_valid (iterator) && subcount < max_subcount; e_iterator_next (iterator)) {
			gchar *label = (gchar *) e_iterator_get (iterator);
			if (label && *label) {
				uiinfo[i].type = GNOME_APP_UI_ITEM;
				uiinfo[i].label = "";
				++i;
				++subcount;
			}
		}
		if (e_iterator_is_valid (iterator)) {
			uiinfo[i].type = GNOME_APP_UI_ITEM;
			uiinfo[i].label = "";
			++i;
		}
		
		uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
		++i;

		g_object_unref (iterator);
	}

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = N_("Edit Contact List");
	uiinfo[i].moreinfo = edit_contact_info_cb;
	++i;

	add_remove_recipient (&(uiinfo[i]), info);
	++i;
	
	add_remove_all_recipients (&(uiinfo[i]), info);
	++i;

	uiinfo[i].type = GNOME_APP_UI_ENDOFINFO;

	pop = gnome_popup_menu_new (uiinfo);

	/* Now set labels properly. */
	
	str = e_destination_get_name (info->dest);
	if (!(str && *str))
		str = _("Unnamed Contact List");
	set_uiinfo_label (&(uiinfo[0]), str);

	if (card->email) {
		
		iterator = e_list_get_iterator (card->email);
		i = 2;
		for (e_iterator_reset (iterator); e_iterator_is_valid (iterator) && subcount < max_subcount; e_iterator_next (iterator)) {
			gchar *label = (gchar *) e_iterator_get (iterator);
			if (label && *label) {
				EDestination *subdest = e_destination_import (label);
				set_uiinfo_label (&(uiinfo[i]), e_destination_get_address (subdest));
				++i;
				g_object_unref (subdest);
			}
		}
		if (e_iterator_is_valid (iterator)) {
			gs = g_strdup_printf (N_("(%d not shown)"), e_list_length (card->email) - max_subcount);
			set_uiinfo_label (&(uiinfo[i]), gs);
			g_free (gs);
		}

		g_object_unref (iterator);
	}


	return pop;
}

static void
quick_add_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	e_contact_quick_add_free_form (e_destination_get_address (info->dest), NULL, NULL);
}

static GtkWidget *
popup_menu_nocard (PopupInfo *info)
{
	GnomeUIInfo uiinfo[ARBITRARY_UIINFO_LIMIT];
	gint i=0;
	GtkWidget *pop;
	const gchar *str;
	gint html_toggle;

	memset (uiinfo, 0, sizeof (uiinfo));

	/* Use an empty label for now, we'll fill it later.
	   If we set uiinfo label to contact name here, gnome_popup_menu_new
	   could screw it up trying make a "translation". */
	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = "";
	++i;

	uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
	++i;

	add_html_mail (&(uiinfo[i]), info);
	html_toggle = i;
	++i;
	
	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = _("Add to Contacts");
	uiinfo[i].moreinfo = quick_add_cb;
	++i;

	add_remove_recipient (&(uiinfo[i]), info);
	++i;

	add_remove_all_recipients (&(uiinfo[i]), info);
	++i;

	uiinfo[i].type = GNOME_APP_UI_ENDOFINFO;

	pop = gnome_popup_menu_new (uiinfo);

	init_html_mail (&(uiinfo[html_toggle]), info);

	/* Now set label of the first item to contact name */
	str = e_destination_get_name (info->dest);
	if (! (str && *str))
		str = e_destination_get_email (info->dest);
	if (! (str && *str))
		str = _("Unnamed Contact");
	
	set_uiinfo_label (&(uiinfo[0]), str);
	
	return pop;
}

void
e_select_names_popup (ESelectNamesTextModel *text_model, GdkEventButton *ev, gint pos, GtkWidget *for_widget)
{
	ESelectNamesModel *model;
	GtkWidget *popup;
	PopupInfo *info;
	EDestination *dest;
	ECard *card;
	gint index;

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

	card = e_destination_get_card (dest);

	info = popup_info_new (text_model, dest, pos, index);
	
	if (e_destination_contains_card (dest)) {
		if (e_destination_is_evolution_list (dest))
			popup = popup_menu_list (info);
		else
			popup = popup_menu_card (info);
	} else {
		popup = popup_menu_nocard (info);
	}

	if (popup) {
		/* Clean up our info item after we've made our selection. */
		g_signal_connect (popup,
				  "selection-done",
				  G_CALLBACK (popup_info_cleanup),
				  info);

		gnome_popup_menu_do_popup (popup, NULL, NULL, ev, info, for_widget);

	} else {

		popup_info_free (info);

	}
}
