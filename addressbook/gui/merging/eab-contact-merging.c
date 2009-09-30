/*
 * Code for checking for duplicates when doing EContact work.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Christopher James Lahey <clahey@ximian.com>
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include "eab-contact-merging.h"
#include "eab-contact-compare.h"
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <string.h>
#include "addressbook/gui/widgets/eab-contact-display.h"
#include "e-util/e-util-private.h"
#include <glib/gi18n.h>

typedef struct dropdown_data dropdown_data;
typedef enum {
	E_CONTACT_MERGING_ADD,
	E_CONTACT_MERGING_COMMIT
} EContactMergingOpType;

typedef struct {
	EContactMergingOpType op;
	EBook *book;
	/*contact is the new contact which the user has tried to add to the addressbook*/
	EContact *contact;
	/*match is the duplicate contact already existing in the addressbook*/
	EContact *match;
	GList *avoid;
	EBookIdCallback id_cb;
	EBookCallback   cb;
	gpointer closure;
} EContactMergingLookup;

struct dropdown_data {
	EContact *match;
	EContactField field;
};
static void match_query_callback (EContact *contact, EContact *match, EABContactMatchType type, gpointer closure);

#define SIMULTANEOUS_MERGING_REQUESTS 20

static GList *merging_queue = NULL;
static gint running_merge_requests = 0;

static void
add_lookup (EContactMergingLookup *lookup)
{
	if (running_merge_requests < SIMULTANEOUS_MERGING_REQUESTS) {
		running_merge_requests++;
		eab_contact_locate_match_full (lookup->book, lookup->contact, lookup->avoid, match_query_callback, lookup);
	}
	else {
		merging_queue = g_list_append (merging_queue, lookup);
	}
}

static void
finished_lookup (void)
{
	running_merge_requests--;

	while (running_merge_requests < SIMULTANEOUS_MERGING_REQUESTS) {
		EContactMergingLookup *lookup;

		if (!merging_queue)
			break;

		lookup = merging_queue->data;

		merging_queue = g_list_remove_link (merging_queue, merging_queue);

		running_merge_requests++;
		eab_contact_locate_match_full (lookup->book, lookup->contact, lookup->avoid, match_query_callback, lookup);
	}
}

static void
free_lookup (EContactMergingLookup *lookup)
{
	g_object_unref (lookup->book);
	g_object_unref (lookup->contact);
	g_list_free (lookup->avoid);
	if (lookup->match)
		g_object_unref (lookup->match);
	g_free (lookup);
}

static void
final_id_cb (EBook *book, EBookStatus status, const gchar *id, gpointer closure)
{
	EContactMergingLookup *lookup = closure;

	if (lookup->id_cb)
		lookup->id_cb (lookup->book, status, id, lookup->closure);

	free_lookup (lookup);

	finished_lookup ();
}

static void
final_cb (EBook *book, EBookStatus status, gpointer closure)
{
	EContactMergingLookup *lookup = closure;

	if (lookup->cb)
		lookup->cb (lookup->book, status, lookup->closure);

	free_lookup (lookup);

	finished_lookup ();
}

static void
doit (EContactMergingLookup *lookup)
{
	if (lookup->op == E_CONTACT_MERGING_ADD)
		e_book_async_add_contact (lookup->book, lookup->contact, final_id_cb, lookup);
	else if (lookup->op == E_CONTACT_MERGING_COMMIT)
		e_book_async_commit_contact (lookup->book, lookup->contact, final_cb, lookup);
}

static void
cancelit (EContactMergingLookup *lookup)
{
	if (lookup->op == E_CONTACT_MERGING_ADD) {
		final_id_cb (lookup->book, E_BOOK_ERROR_CANCELLED, NULL, lookup);
	} else if (lookup->op == E_CONTACT_MERGING_COMMIT) {
		final_cb (lookup->book, E_BOOK_ERROR_CANCELLED, lookup);
	}
}

static void
dialog_map (GtkWidget *window, GdkEvent *event, GtkWidget *table)
{
	gint h, w;

	/* Spacing around the table */
	w = table->allocation.width + 30;
	/* buttons and outer spacing */
	h = table->allocation.height + 60;
	if (w > 400)
		w = 400;
	if (h > 450)
		h = 450;

	gtk_widget_set_size_request (window, w, h);
}

static void
dropdown_changed (GtkWidget *dropdown, dropdown_data *data)
{
	gchar *str;
	str = gtk_combo_box_get_active_text (GTK_COMBO_BOX (dropdown));

	if (g_ascii_strcasecmp(str, ""))
		e_contact_set (data->match, data->field, str);
	else
		e_contact_set (data->match, data->field, NULL);
	return;
}

static gint
mergeit (EContactMergingLookup *lookup)
{
	GtkWidget *scrolled_window, *label, *hbox, *dropdown;
	GtkDialog *dialog;
	GtkTable *table;
	EContactField field;
	gchar *str = NULL, *string = NULL, *string1 = NULL;
	gint num_of_email;
	GList *email_attr_list;
	gint row = -1;
	gint value = 0, result;

	dialog = (GtkDialog *)(gtk_dialog_new_with_buttons (_("Merge Contact"), NULL, GTK_DIALOG_NO_SEPARATOR, NULL));
	gtk_container_set_border_width (GTK_CONTAINER(dialog), 5);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	table = (GtkTable *) gtk_table_new (20, 2, FALSE);
	gtk_container_set_border_width ((GtkContainer *) table, 12);
	gtk_table_set_row_spacings (table, 6);
	gtk_table_set_col_spacings (table, 2);

	gtk_dialog_add_buttons ((GtkDialog *) dialog,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("_Merge"), GTK_RESPONSE_OK,
				NULL);

	email_attr_list = e_contact_get_attributes (lookup->match, E_CONTACT_EMAIL);
	num_of_email = g_list_length (email_attr_list);

	/*we match all the string fields of the already existing contact and the new contact.*/
	for (field = E_CONTACT_FULL_NAME; field != (E_CONTACT_LAST_SIMPLE_STRING -1); field++) {
		dropdown_data *data = NULL;
		string = (gchar *)e_contact_get_const (lookup->contact, field);
		string1 = (gchar *)e_contact_get_const (lookup->match, field);

		/*the field must exist in the new as well as the duplicate contact*/
		if (string && *string) {
			/*Four email id's present, should be compared with all email id's in duplicate contact */
			/*Merge only if number of email id's in existing contact is less than 4 */
			if ((field == E_CONTACT_EMAIL_1 || field == E_CONTACT_EMAIL_2
			    || field == E_CONTACT_EMAIL_3 || field == E_CONTACT_EMAIL_4) && (num_of_email < 4)) {
				EContactField use_field = field;
				row++;
				str = (gchar *)e_contact_get_const (lookup->contact, use_field);
				switch (num_of_email)
				{
				case 0:
					use_field = E_CONTACT_EMAIL_1;
					break;
				case 1:
					/*New contact has email that is NOT equal to email in duplicate contact*/
					if ((str && *str) && (g_ascii_strcasecmp(e_contact_get_const (lookup->match, E_CONTACT_EMAIL_1),str))) {
						use_field = E_CONTACT_EMAIL_2;
						break;
					}
					else/*Either the new contact has no email OR the email already exist in the duplicate contact*/
						continue;
				case 2:
					/*New contact has email and it is equal to neither of the 2 emails in the duplicate contact*/
					if ((str && *str) &&
							(g_ascii_strcasecmp(str,e_contact_get_const (lookup->match, E_CONTACT_EMAIL_1))) &&
							(g_ascii_strcasecmp(e_contact_get_const (lookup->match, E_CONTACT_EMAIL_2),str))) {
						use_field = E_CONTACT_EMAIL_3;
						break;
					}
					else
						continue;
				case 3:
					/*New contact has email and it is equal to none of the 3 emails in the duplicate contact*/
					if ((str && *str) &&
							(g_ascii_strcasecmp(e_contact_get_const (lookup->match, E_CONTACT_EMAIL_1),str)) &&
							(g_ascii_strcasecmp(e_contact_get_const (lookup->match, E_CONTACT_EMAIL_2),str)) &&
							(g_ascii_strcasecmp(e_contact_get_const (lookup->match, E_CONTACT_EMAIL_3),str)))
						use_field = E_CONTACT_EMAIL_4;
					else
						continue;
				}
				label = gtk_label_new (_("Email"));
				hbox = gtk_hbox_new (FALSE, 0);
				gtk_box_pack_start (GTK_BOX(hbox), (GtkWidget*)label, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *)hbox, 0, 1, row, row + 1);

				dropdown = gtk_combo_box_new_text();
				gtk_combo_box_append_text (GTK_COMBO_BOX (dropdown), string);

				data = g_new0 (dropdown_data, 1);

				gtk_combo_box_append_text (GTK_COMBO_BOX (dropdown), "");

				gtk_combo_box_set_active (GTK_COMBO_BOX (dropdown), 0);
				data->field = use_field;
				data->match = lookup->match;
				e_contact_set (lookup->match, use_field, string);
				g_signal_connect (dropdown, "changed", G_CALLBACK(dropdown_changed), data);

				hbox = gtk_hbox_new (FALSE, 0);
				gtk_box_pack_start (GTK_BOX(hbox), (GtkWidget*)dropdown, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *)hbox, 1, 2, row, row + 1);
				gtk_widget_show ((GtkWidget *)dropdown);
				continue;
			}
			if (((field == E_CONTACT_FULL_NAME) && (!g_ascii_strcasecmp(string, string1)))) {
				row++;
				label = gtk_label_new (e_contact_pretty_name(field));
				hbox = gtk_hbox_new (FALSE, 0);
				gtk_box_pack_start (GTK_BOX(hbox), (GtkWidget*)label, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *)hbox, 0, 1, row, row + 1);

				label = gtk_label_new (string);
				hbox = gtk_hbox_new (FALSE, 0);
				gtk_box_pack_start (GTK_BOX(hbox), (GtkWidget*)label, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget*)hbox, 1, 2, row, row + 1);
				continue;
			}

			/*for all string fields except name and email*/
			if (!(string1 && *string1) || (g_ascii_strcasecmp(string, string1))) {
				row++;
				label = gtk_label_new (e_contact_pretty_name(field));
				hbox = gtk_hbox_new (FALSE, 0);
				gtk_box_pack_start (GTK_BOX(hbox), (GtkWidget*)label, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *)hbox, 0, 1, row, row + 1);
				data = g_new0 (dropdown_data, 1);
				dropdown = gtk_combo_box_new_text();
				gtk_combo_box_append_text (GTK_COMBO_BOX (dropdown), string);
				e_contact_set (lookup->match, field, string);

				if (string1 && *string1)
					gtk_combo_box_append_text (GTK_COMBO_BOX (dropdown), string1);
				else
					gtk_combo_box_append_text (GTK_COMBO_BOX (dropdown), "");

				gtk_combo_box_set_active (GTK_COMBO_BOX (dropdown), 0);
				data->field = field;
				data->match = lookup->match;

				if (field == E_CONTACT_NICKNAME || field == E_CONTACT_GIVEN_NAME)
					gtk_widget_set_sensitive ((GtkWidget *)dropdown, FALSE);

				g_signal_connect (dropdown, "changed", G_CALLBACK(dropdown_changed), data);
				hbox = gtk_hbox_new (FALSE, 0);
				gtk_box_pack_start (GTK_BOX(hbox), (GtkWidget*)dropdown, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *)hbox, 1, 2, row, row + 1);
				gtk_widget_show_all ((GtkWidget *)dropdown);
			}
		}
	}

	gtk_window_set_default_size (GTK_WINDOW (dialog), 420, 300);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), GTK_WIDGET (table));
	gtk_box_pack_start (GTK_BOX (dialog->vbox), GTK_WIDGET (scrolled_window), TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);
	g_signal_connect (dialog, "map-event", G_CALLBACK (dialog_map), table);
	gtk_widget_show_all ((GtkWidget *)table);
	result = gtk_dialog_run (dialog);

	switch (result)
	{
	case GTK_RESPONSE_OK:
		     lookup->contact = lookup->match;
		     e_book_async_remove_contact (lookup->book, lookup->match, NULL, lookup);
		     e_book_async_add_contact (lookup->book, lookup->contact, final_id_cb, lookup);
		     value = 1;
		     break;
	case GTK_RESPONSE_CANCEL:
	default:
		     value = 0;
		     break;
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_list_free (email_attr_list);
	return value;
}

static gboolean
check_if_same (EContact *contact, EContact *match)
{
	EContactField field;
	GList *email_attr_list;
	gint num_of_email;
	gchar *str = NULL, *string = NULL, *string1 = NULL;

	for (field = E_CONTACT_FULL_NAME; field != (E_CONTACT_LAST_SIMPLE_STRING -1); field++) {
		email_attr_list = e_contact_get_attributes (match, E_CONTACT_EMAIL);
		num_of_email = g_list_length (email_attr_list);

		if ((field == E_CONTACT_EMAIL_1 || field == E_CONTACT_EMAIL_2
		     || field == E_CONTACT_EMAIL_3 || field == E_CONTACT_EMAIL_4) && (num_of_email<4)) {
			str = (gchar *)e_contact_get_const (contact, field);
			switch (num_of_email)
			{
			case 0:
				return FALSE;
			case 1:
				if ((str && *str) && (g_ascii_strcasecmp(e_contact_get_const (match, E_CONTACT_EMAIL_1),str)))
					return FALSE;
			case 2:
				if ((str && *str) && (g_ascii_strcasecmp(str,e_contact_get_const (match, E_CONTACT_EMAIL_1))) &&
						(g_ascii_strcasecmp(e_contact_get_const (match, E_CONTACT_EMAIL_2),str)))
					return FALSE;
			case 3:
				if ((str && *str) && (g_ascii_strcasecmp(e_contact_get_const (match, E_CONTACT_EMAIL_1),str)) &&
						(g_ascii_strcasecmp(e_contact_get_const (match, E_CONTACT_EMAIL_2),str)) &&
						(g_ascii_strcasecmp(e_contact_get_const (match, E_CONTACT_EMAIL_3),str)))
					return FALSE;
			}
		}
		else {
			string = (gchar *)e_contact_get_const (contact, field);
			string1 = (gchar *)e_contact_get_const (match, field);
			if ((string && *string) && (string1 && *string1) && (g_ascii_strcasecmp(string1,string)))
				return FALSE;
			/*if the field entry exist in either of the contacts,we'll have to give the choice and thus merge button should be sensitive*/
			else if ((string && *string) && !(string1 && *string1))
				return FALSE;
		}
	}
	g_list_free (email_attr_list);
	return TRUE;
}

static void
response (GtkWidget *dialog, gint response, EContactMergingLookup *lookup)
{
	static gint merge_response;

	switch (response) {
	case 0:
		doit (lookup);
		break;
	case 1:
		cancelit (lookup);
		break;
	case 2:
		merge_response = mergeit (lookup);
		if (merge_response)
			break;
		return;
	case GTK_RESPONSE_DELETE_EVENT:
		cancelit (lookup);
		break;
	}

	gtk_widget_destroy (dialog);
}

static void
match_query_callback (EContact *contact, EContact *match, EABContactMatchType type, gpointer closure)
{
	EContactMergingLookup *lookup = closure;
	gchar *gladefile;
	gint flag;

	if ((gint) type <= (gint) EAB_CONTACT_MATCH_VAGUE) {
		doit (lookup);
	} else {
		GladeXML *ui;

		GtkWidget *widget, *merge_button;

		lookup->match = g_object_ref (match);
		if (lookup->op == E_CONTACT_MERGING_ADD) {
			/* Compares all the values of contacts and return true, if they match */
			flag = check_if_same (contact, match);
			gladefile = g_build_filename (EVOLUTION_GLADEDIR,
						      "eab-contact-duplicate-detected.glade",
						      NULL);
			ui = glade_xml_new (gladefile, NULL, NULL);
			merge_button = glade_xml_get_widget (ui, "button5");
			/* Merge Button not sensitive when all values are same */
			if (flag)
				gtk_widget_set_sensitive (GTK_WIDGET (merge_button), FALSE);
			g_free (gladefile);
		} else if (lookup->op == E_CONTACT_MERGING_COMMIT) {
			gladefile = g_build_filename (EVOLUTION_GLADEDIR,
						      "eab-contact-commit-duplicate-detected.glade",
						      NULL);
			ui = glade_xml_new (gladefile, NULL, NULL);
			g_free (gladefile);
		} else {
			doit (lookup);
			return;
		}

		widget = glade_xml_get_widget (ui, "custom-old-contact");
		eab_contact_display_render (EAB_CONTACT_DISPLAY (widget),
					    match, EAB_CONTACT_DISPLAY_RENDER_COMPACT);

		widget = glade_xml_get_widget (ui, "custom-new-contact");
		eab_contact_display_render (EAB_CONTACT_DISPLAY (widget),
					    contact, EAB_CONTACT_DISPLAY_RENDER_COMPACT);

		widget = glade_xml_get_widget (ui, "dialog-duplicate-contact");

		gtk_widget_ensure_style (widget);
		gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (widget)->vbox), 0);
		gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (widget)->action_area), 12);

		g_signal_connect (widget, "response",
				  G_CALLBACK (response), lookup);

		gtk_widget_show_all (widget);
	}
}

gboolean
eab_merging_book_add_contact (EBook           *book,
			      EContact        *contact,
			      EBookIdCallback  cb,
			      gpointer         closure)
{
	EContactMergingLookup *lookup;

	lookup = g_new (EContactMergingLookup, 1);

	lookup->op = E_CONTACT_MERGING_ADD;
	lookup->book = g_object_ref (book);
	lookup->contact = g_object_ref (contact);
	lookup->id_cb = cb;
	lookup->closure = closure;
	lookup->avoid = NULL;
	lookup->match = NULL;

	add_lookup (lookup);

	return TRUE;
}

gboolean
eab_merging_book_commit_contact (EBook                 *book,
				 EContact              *contact,
				 EBookCallback          cb,
				 gpointer               closure)
{
	EContactMergingLookup *lookup;

	lookup = g_new (EContactMergingLookup, 1);

	lookup->op = E_CONTACT_MERGING_COMMIT;
	lookup->book = g_object_ref (book);
	lookup->contact = g_object_ref (contact);
	lookup->cb = cb;
	lookup->closure = closure;
	lookup->avoid = g_list_append (NULL, contact);
	lookup->match = NULL;

	add_lookup (lookup);

	return TRUE;
}

GtkWidget *
_eab_contact_merging_create_contact_display(gchar *name,
					    gchar *string1, gchar *string2,
					    gint int1, gint int2);

GtkWidget *
_eab_contact_merging_create_contact_display(gchar *name,
					    gchar *string1, gchar *string2,
					    gint int1, gint int2)
{
	return eab_contact_display_new();
}
