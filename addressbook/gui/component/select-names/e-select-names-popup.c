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
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libgnomeui/gnome-popup-menu.h>

#include <addressbook/contact-editor/e-contact-quick-add.h>
#include "e-select-names-popup.h"

typedef struct _PopupInfo PopupInfo;
struct _PopupInfo {
	ESelectNamesModel *model;
	const EDestination *dest;
	gint pos;
	gint index;
};

static PopupInfo *
popup_info_new (ESelectNamesModel *model, const EDestination *dest, gint pos, gint index)
{
	PopupInfo *info = g_new0 (PopupInfo, 1);
	info->model = model;
	info->dest = dest;
	info->pos = pos;
	info->index = index;

	if (model)
		gtk_object_ref (GTK_OBJECT (model));

	if (dest)
		gtk_object_ref (GTK_OBJECT (dest));

	return info;
}

static void
popup_info_free (PopupInfo *info)
{
	if (info) {
		
		if (info->model)
			gtk_object_unref (GTK_OBJECT (info->model));

		if (info->dest)
			gtk_object_unref (GTK_OBJECT (info->dest));

		g_free (info);
	}
}

static void
popup_info_cleanup (GtkWidget *w, gpointer info)
{
	popup_info_free ((PopupInfo *) info);
}

static void
do_nothing (gpointer foo)
{

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

	n = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (w), "number"));

	if (n != e_destination_get_email_num (info->dest)) {
		dest = e_destination_new ();
		e_destination_set_card (dest, e_destination_get_card (info->dest), n);
		e_select_names_model_replace (info->model, info->index, dest);

	}
}

static void
remove_recipient_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	e_select_names_model_delete (info->model, info->index);
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
	e_select_names_model_delete_all (info->model);
}

static void
add_remove_all_recipients (GnomeUIInfo *uiinfo, PopupInfo *info)
{
	uiinfo->type = GNOME_APP_UI_ITEM;
	uiinfo->label = _("Remove All");
	uiinfo->moreinfo = remove_all_recipients_cb;
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
	gchar *name_str;

	/*
	 * Build up our GnomeUIInfo array.
	 */

	memset (uiinfo, 0, sizeof (uiinfo));
	memset (radioinfo, 0, sizeof (radioinfo));

	card = e_destination_get_card (info->dest);
	name_str = e_card_name_to_string (card->name);

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = name_str;
	++i;

	uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
	++i;

	if (e_list_length (card->email) > 1) {
		gint j = 0;

		using_radio = TRUE;

		iterator = e_list_get_iterator (card->email);
		for (e_iterator_reset (iterator); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
			
			radioinfo[j].type = GNOME_APP_UI_ITEM;
			radioinfo[j].label = (gchar *)e_iterator_get (iterator);
			radioinfo[j].moreinfo = change_email_num_cb;
			++j;
		}

		radioinfo[j].type = GNOME_APP_UI_ENDOFINFO;

		uiinfo[i].type = GNOME_APP_UI_RADIOITEMS;
		uiinfo[i].moreinfo = radioinfo;
		++i;

	} else {
		uiinfo[i].type = GNOME_APP_UI_ITEM;
		uiinfo[i].label = (gchar *) e_destination_get_email (info->dest);
		++i;
	}

	uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
	++i;

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = N_("Edit Contact Info");
	uiinfo[i].moreinfo = do_nothing;
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
	g_free (name_str);

	if (using_radio) {
		gint n = e_destination_get_email_num (info->dest);
		gint j;
		for (j=0; radioinfo[j].type != GNOME_APP_UI_ENDOFINFO; ++j) {
			gtk_object_set_data (GTK_OBJECT (radioinfo[j].widget), "number", GINT_TO_POINTER (j));
			if (j == n) 
				gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (radioinfo[n].widget), TRUE);
		}
	}

	return pop;
}

static void
quick_add_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	e_contact_quick_add_free_form (e_destination_get_string (info->dest), NULL, NULL);
}

static GtkWidget *
popup_menu_nocard (PopupInfo *info)
{
	GnomeUIInfo uiinfo[ARBITRARY_UIINFO_LIMIT];
	gint i=0;
	GtkWidget *pop;
	const gchar *str;

	memset (uiinfo, 0, sizeof (uiinfo));

	str = e_destination_get_string (info->dest);

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = (gchar *) str;
	++i;

	uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
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
	
	return pop;
}


void
e_select_names_popup (ESelectNamesModel *model, GdkEventButton *ev, gint pos)
{
	GtkWidget *popup;
	PopupInfo *info;
	const EDestination *dest;
	ECard *card;
	gint index;

	g_return_if_fail (model && E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (ev);
	g_return_if_fail (0 <= pos);

	e_select_names_model_text_pos (model, pos, &index, NULL, NULL);
	if (index < 0 || index >= e_select_names_model_count (model))
		return;

	dest = e_select_names_model_get_destination (model, index);
	card = e_destination_get_card (dest);

	info = popup_info_new (model, dest, pos, index);

	popup = card ? popup_menu_card (info) : popup_menu_nocard (info);

	if (popup) {
		/* Clean up our info item after we've made our selection. */
		gtk_signal_connect (GTK_OBJECT (popup),
				    "selection-done",
				    GTK_SIGNAL_FUNC (popup_info_cleanup),
				    info);

		gnome_popup_menu_do_popup (popup, NULL, NULL, ev, info);

	} else {

		popup_info_free (info);

	}
}
