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
#include <gal/widgets/e-unicode.h>

#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/gui/contact-editor/e-contact-editor.h>
#include <addressbook/gui/contact-editor/e-contact-quick-add.h>
#include "e-addressbook-util.h"
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

/* You are in a maze of twisty little callbacks, all alike... */

static void
make_contact_editor_cb (EBook *book, gpointer user_data)
{
	if (book) {
		EDestination *dest = E_DESTINATION (user_data);
		EContactEditor *ce;
		ECard *card;

		card = (ECard *) e_destination_get_card (dest);
		ce = e_addressbook_show_contact_editor (book, card, FALSE, TRUE);
		e_contact_editor_raise (ce);
		gtk_object_unref (GTK_OBJECT (dest));
	}
}

static void
edit_contact_info_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *info = (PopupInfo *) user_data;
	if (info == NULL)
		return;

	gtk_object_ref (GTK_OBJECT (info->dest));
	e_book_use_local_address_book (make_contact_editor_cb, (gpointer) info->dest);
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
	gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (uiinfo->widget), TRUE);

}

/* Duplicate the string, mapping _ to __.  This is to make sure that underscores in
   e-mail addresses don't get mistaken for keyboard accelerators. */
static gchar *
quote_label (const gchar *str)
{
	gint len = str ? strlen (str) : -1;
	const gchar *c = str;
	gchar *d, *q;

	if (len < 0)
		return NULL;

	while (*c) {
		if (*c == '_')
			++len;
		++c;
	}

	q = g_new (gchar, len+1);
	c = str;
	d = q;
	while (*c) {
		*d = *c;
		if (*c == '_') {
			++d;
			*d = '_';
		}
		++c;
		++d;
	}
	*d = '\0';
	return q;
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
	GtkWidget *pop, *label;
	GList *item_children;
	EIterator *iterator;
	gint html_toggle;
	gchar *name_label, *quoted_name_label;

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

	if (e_list_length (card->email) > 1) {
		gint j = 0;

		using_radio = TRUE;

		iterator = e_list_get_iterator (card->email);
		for (e_iterator_reset (iterator); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
			gchar *label = (gchar *)e_iterator_get (iterator);
			
			if (label && *label) {
				radioinfo[j].type = GNOME_APP_UI_ITEM;
				radioinfo[j].label = label;
				radioinfo[j].moreinfo = change_email_num_cb;
				++j;
			}
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

	if (using_radio) {
		gint n = e_destination_get_email_num (info->dest);
		gint j;
		for (j=0; radioinfo[j].type != GNOME_APP_UI_ENDOFINFO; ++j) {
			gtk_object_set_data (GTK_OBJECT (radioinfo[j].widget), "number", GINT_TO_POINTER (j));
			if (j == n) 
				gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (radioinfo[n].widget), TRUE);
		}
	}

	init_html_mail (&(uiinfo[html_toggle]), info);

	/* Now set label of the first item to contact name */
	name_label = e_utf8_to_locale_string (e_destination_get_name (info->dest));
	quoted_name_label = quote_label (name_label);
	item_children = gtk_container_children (GTK_CONTAINER (uiinfo[0].widget));
	label = item_children->data;
	g_list_free (item_children);
	gtk_label_set_text (GTK_LABEL (label), quoted_name_label);
	g_free (name_label);
	g_free (quoted_name_label);

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
	GtkWidget *pop, *label;
	GList *item_children;
	const gchar *str;
	gchar *name_label, *quoted_name_label;
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
	if (str == NULL)
		str = e_destination_get_email (info->dest);
	if (str != NULL) {
		name_label = e_utf8_to_locale_string (str);
	} else {
		name_label = g_strdup (_("Unnamed Contact"));
	}
	quoted_name_label = quote_label (name_label);
	item_children = gtk_container_children (GTK_CONTAINER (uiinfo[0].widget));
	label = item_children->data;
	g_list_free (item_children);
	gtk_label_set_text (GTK_LABEL (label), quoted_name_label);
	g_free (name_label);
	g_free (quoted_name_label);
	
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
	if (e_destination_is_empty (dest))
		return;

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
