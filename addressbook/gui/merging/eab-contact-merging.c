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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eab-contact-merging.h"
#include "eab-contact-compare.h"
#include <gtk/gtk.h>
#include <string.h>
#include "addressbook/gui/widgets/eab-contact-display.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include <glib/gi18n.h>

typedef struct dropdown_data dropdown_data;
typedef enum {
	E_CONTACT_MERGING_ADD,
	E_CONTACT_MERGING_COMMIT,
	E_CONTACT_MERGING_FIND
} EContactMergingOpType;

typedef struct {
	EContactMergingOpType op;
	ESourceRegistry *registry;
	EBookClient *book_client;
	/*contact is the new contact which the user has tried to add to the addressbook*/
	EContact *contact;
	/*match is the duplicate contact already existing in the addressbook*/
	EContact *match;
	GList *avoid;
	EABMergingAsyncCallback cb;
	EABMergingIdAsyncCallback id_cb;
	EABMergingContactAsyncCallback c_cb;
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
		eab_contact_locate_match_full (
			lookup->registry, lookup->book_client,
			lookup->contact, lookup->avoid,
			match_query_callback, lookup);
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
		eab_contact_locate_match_full (
			lookup->registry, lookup->book_client,
			lookup->contact, lookup->avoid,
			match_query_callback, lookup);
	}
}

static void
free_lookup (EContactMergingLookup *lookup)
{
	g_object_unref (lookup->registry);
	g_object_unref (lookup->book_client);
	g_object_unref (lookup->contact);
	g_list_free (lookup->avoid);
	if (lookup->match)
		g_object_unref (lookup->match);
	g_free (lookup);
}

static void
final_id_cb (EBookClient *book_client,
             const GError *error,
             const gchar *id,
             gpointer closure)
{
	EContactMergingLookup *lookup = closure;

	if (lookup->id_cb)
		lookup->id_cb (
			lookup->book_client,
			error, id, lookup->closure);

	free_lookup (lookup);

	finished_lookup ();
}

static void
final_cb_as_id (EBookClient *book_client,
                const GError *error,
                gpointer closure)
{
	EContactMergingLookup *lookup = closure;

	if (lookup->id_cb)
		lookup->id_cb (
			lookup->book_client,
			error, lookup->contact ?
				e_contact_get_const (
				lookup->contact, E_CONTACT_UID) : NULL,
			lookup->closure);

	free_lookup (lookup);

	finished_lookup ();
}

static void
final_cb (EBookClient *book_client,
          const GError *error,
          gpointer closure)
{
	EContactMergingLookup *lookup = closure;

	if (lookup->cb)
		lookup->cb (lookup->book_client, error, lookup->closure);

	free_lookup (lookup);

	finished_lookup ();
}

static void
modify_contact_ready_cb (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	EContactMergingLookup *lookup = user_data;
	GError *error = NULL;

	g_return_if_fail (book_client != NULL);
	g_return_if_fail (lookup != NULL);

	e_book_client_modify_contact_finish (book_client, result, &error);

	if (lookup->op == E_CONTACT_MERGING_ADD)
		final_cb_as_id (book_client, error, lookup);
	else
		final_cb (book_client, error, lookup);

	if (error)
		g_error_free (error);
}

static void
add_contact_ready_cb (GObject *source_object,
                      GAsyncResult *result,
                      gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	EContactMergingLookup *lookup = user_data;
	gchar *uid = NULL;
	GError *error = NULL;

	g_return_if_fail (book_client != NULL);
	g_return_if_fail (lookup != NULL);

	if (!e_book_client_add_contact_finish (book_client, result, &uid, &error))
		uid = NULL;

	final_id_cb (book_client, error, uid, lookup);

	if (error)
		g_error_free (error);
}

static void
doit (EContactMergingLookup *lookup,
      gboolean force_modify)
{
	if (lookup->op == E_CONTACT_MERGING_ADD) {
		if (force_modify)
			e_book_client_modify_contact (lookup->book_client, lookup->contact, NULL, modify_contact_ready_cb, lookup);
		else
			e_book_client_add_contact (lookup->book_client, lookup->contact, NULL, add_contact_ready_cb, lookup);
	} else if (lookup->op == E_CONTACT_MERGING_COMMIT)
		e_book_client_modify_contact (lookup->book_client, lookup->contact, NULL, modify_contact_ready_cb, lookup);
}

static void
cancelit (EContactMergingLookup *lookup)
{
	GError *error = e_client_error_create (E_CLIENT_ERROR_CANCELLED, NULL);

	if (lookup->op == E_CONTACT_MERGING_ADD) {
		final_id_cb (lookup->book_client, error, NULL, lookup);
	} else if (lookup->op == E_CONTACT_MERGING_COMMIT) {
		final_cb (lookup->book_client, error, lookup);
	}

	g_error_free (error);
}

static void
dialog_map (GtkWidget *window,
            GdkEvent *event,
            GtkWidget *table)
{
	GtkAllocation allocation;
	gint h, w;

	gtk_widget_get_allocation (table, &allocation);

	/* Spacing around the table */
	w = allocation.width + 30;
	/* buttons and outer spacing */
	h = allocation.height + 60;
	if (w > 400)
		w = 400;
	if (h > 450)
		h = 450;

	gtk_widget_set_size_request (window, w, h);
}

static void
dropdown_changed (GtkWidget *dropdown,
                  dropdown_data *data)
{
	gchar *str;
	str = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (dropdown));

	if (g_ascii_strcasecmp (str, ""))
		e_contact_set (data->match, data->field, str);
	else
		e_contact_set (data->match, data->field, NULL);
	return;
}

static void
remove_contact_ready_cb (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	EContactMergingLookup *lookup = user_data;
	GError *error = NULL;

	g_return_if_fail (book_client != NULL);
	g_return_if_fail (lookup != NULL);

	e_book_client_remove_contact_finish (book_client, result, &error);

	if (error != NULL) {
		g_warning (
			"%s: Failed to remove contact: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	e_book_client_add_contact (
		book_client, lookup->contact, NULL,
		add_contact_ready_cb, lookup);
}

static gint
mergeit (EContactMergingLookup *lookup)
{
	GtkWidget *scrolled_window, *label, *hbox, *dropdown;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkTable *table;
	EContactField field;
	gchar *str = NULL, *string = NULL, *string1 = NULL;
	gint num_of_email;
	GList *email_attr_list;
	gint row = -1;
	gint value = 0, result;

	dialog = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (dialog), _("Merge Contact"));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	table = (GtkTable *) gtk_table_new (20, 2, FALSE);
	gtk_container_set_border_width ((GtkContainer *) table, 12);
	gtk_table_set_row_spacings (table, 6);
	gtk_table_set_col_spacings (table, 2);

	gtk_dialog_add_buttons (
		GTK_DIALOG (dialog),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		_("_Merge"), GTK_RESPONSE_OK,
		NULL);

	email_attr_list = e_contact_get_attributes (lookup->match, E_CONTACT_EMAIL);
	num_of_email = g_list_length (email_attr_list);

	/*we match all the string fields of the already existing contact and the new contact.*/
	for (field = E_CONTACT_FULL_NAME; field != (E_CONTACT_LAST_SIMPLE_STRING -1); field++) {
		dropdown_data *data = NULL;
		string = (gchar *) e_contact_get_const (lookup->contact, field);
		string1 = (gchar *) e_contact_get_const (lookup->match, field);

		/*the field must exist in the new as well as the duplicate contact*/
		if (string && *string) {
			/*Four email id's present, should be compared with all email id's in duplicate contact */
			/*Merge only if number of email id's in existing contact is less than 4 */
			if ((field == E_CONTACT_EMAIL_1 || field == E_CONTACT_EMAIL_2
			    || field == E_CONTACT_EMAIL_3 || field == E_CONTACT_EMAIL_4) && (num_of_email < 4)) {
				EContactField use_field = field;
				row++;
				str = (gchar *) e_contact_get_const (lookup->contact, use_field);
				switch (num_of_email)
				{
				case 0:
					use_field = E_CONTACT_EMAIL_1;
					break;
				case 1:
					/*New contact has email that is NOT equal to email in duplicate contact*/
					if ((str && *str) && (g_ascii_strcasecmp (e_contact_get_const (lookup->match, E_CONTACT_EMAIL_1),str))) {
						use_field = E_CONTACT_EMAIL_2;
						break;
					}
					else/*Either the new contact has no email OR the email already exist in the duplicate contact */
						continue;
				case 2:
					/*New contact has email and it is equal to neither of the 2 emails in the duplicate contact*/
					if ((str && *str) &&
							(g_ascii_strcasecmp (str,e_contact_get_const (lookup->match, E_CONTACT_EMAIL_1))) &&
							(g_ascii_strcasecmp (e_contact_get_const (lookup->match, E_CONTACT_EMAIL_2),str))) {
						use_field = E_CONTACT_EMAIL_3;
						break;
					}
					else
						continue;
				case 3:
					/*New contact has email and it is equal to none of the 3 emails in the duplicate contact*/
					if ((str && *str) &&
							(g_ascii_strcasecmp (e_contact_get_const (lookup->match, E_CONTACT_EMAIL_1),str)) &&
							(g_ascii_strcasecmp (e_contact_get_const (lookup->match, E_CONTACT_EMAIL_2),str)) &&
							(g_ascii_strcasecmp (e_contact_get_const (lookup->match, E_CONTACT_EMAIL_3),str)))
						use_field = E_CONTACT_EMAIL_4;
					else
						continue;
				}
				label = gtk_label_new (_("Email"));
				hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
				gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget *) label, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *) hbox, 0, 1, row, row + 1);

				dropdown = gtk_combo_box_text_new ();
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), string);

				data = g_new0 (dropdown_data, 1);

				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), "");

				gtk_combo_box_set_active (GTK_COMBO_BOX (dropdown), 0);
				data->field = use_field;
				data->match = lookup->match;
				e_contact_set (lookup->match, use_field, string);
				g_signal_connect (
					dropdown, "changed",
					G_CALLBACK (dropdown_changed), data);

				hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
				gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget *) dropdown, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *) hbox, 1, 2, row, row + 1);
				gtk_widget_show ((GtkWidget *) dropdown);
				continue;
			}
			if (((field == E_CONTACT_FULL_NAME) && (!g_ascii_strcasecmp (string, string1)))) {
				row++;
				label = gtk_label_new (e_contact_pretty_name (field));
				hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
				gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget *) label, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *) hbox, 0, 1, row, row + 1);

				label = gtk_label_new (string);
				hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
				gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget *) label, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *) hbox, 1, 2, row, row + 1);
				continue;
			}

			/*for all string fields except name and email*/
			if (!(string1 && *string1) || (g_ascii_strcasecmp (string, string1))) {
				row++;
				label = gtk_label_new (e_contact_pretty_name (field));
				hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
				gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget *) label, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *) hbox, 0, 1, row, row + 1);
				data = g_new0 (dropdown_data, 1);
				dropdown = gtk_combo_box_text_new ();
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), string);
				e_contact_set (lookup->match, field, string);

				if (string1 && *string1)
					gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), string1);
				else
					gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), "");

				gtk_combo_box_set_active (GTK_COMBO_BOX (dropdown), 0);
				data->field = field;
				data->match = lookup->match;

				if (field == E_CONTACT_NICKNAME || field == E_CONTACT_GIVEN_NAME)
					gtk_widget_set_sensitive ((GtkWidget *) dropdown, FALSE);

				g_signal_connect (
					dropdown, "changed",
					G_CALLBACK (dropdown_changed), data);

				hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
				gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget *) dropdown, FALSE, FALSE, 0);
				gtk_table_attach_defaults (table, (GtkWidget *) hbox, 1, 2, row, row + 1);
				gtk_widget_show_all ((GtkWidget *) dropdown);
			}
		}
	}

	gtk_window_set_default_size (GTK_WINDOW (dialog), 420, 300);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), GTK_WIDGET (table));
	gtk_box_pack_start (GTK_BOX (content_area), GTK_WIDGET (scrolled_window), TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);
	g_signal_connect (
		dialog, "map-event",
		G_CALLBACK (dialog_map), table);
	gtk_widget_show_all ((GtkWidget *) table);
	result = gtk_dialog_run (GTK_DIALOG (dialog));

	switch (result) {
	case GTK_RESPONSE_OK:
		g_object_unref (lookup->contact);
		lookup->contact = g_object_ref (lookup->match);
		e_book_client_remove_contact (
			lookup->book_client,
			lookup->match, NULL,
			remove_contact_ready_cb, lookup);
		value = 1;
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		value = 0;
		break;
	}
	gtk_widget_destroy (dialog);
	g_list_free_full (email_attr_list, (GDestroyNotify) e_vcard_attribute_free);
	return value;
}

static gboolean
check_if_same (EContact *contact,
               EContact *match)
{
	EContactField field;
	GList *email_attr_list;
	gint num_of_email;
	gchar *str = NULL, *string = NULL, *string1 = NULL;
	gboolean res = TRUE;

	email_attr_list = e_contact_get_attributes (match, E_CONTACT_EMAIL);
	num_of_email = g_list_length (email_attr_list);

	for (field = E_CONTACT_FULL_NAME; res && field != (E_CONTACT_LAST_SIMPLE_STRING -1); field++) {

		if ((field == E_CONTACT_EMAIL_1 || field == E_CONTACT_EMAIL_2
		     || field == E_CONTACT_EMAIL_3 || field == E_CONTACT_EMAIL_4) && (num_of_email < 4)) {
			str = (gchar *) e_contact_get_const (contact, field);
			switch (num_of_email)
			{
			case 0:
				res = FALSE;
				break;
			case 1:
				if ((str && *str) && (g_ascii_strcasecmp (e_contact_get_const (match, E_CONTACT_EMAIL_1),str)))
					res = FALSE;
				break;
			case 2:
				if ((str && *str) && (g_ascii_strcasecmp (str,e_contact_get_const (match, E_CONTACT_EMAIL_1))) &&
						(g_ascii_strcasecmp (e_contact_get_const (match, E_CONTACT_EMAIL_2),str)))
					res = FALSE;
				break;
			case 3:
				if ((str && *str) && (g_ascii_strcasecmp (e_contact_get_const (match, E_CONTACT_EMAIL_1),str)) &&
						(g_ascii_strcasecmp (e_contact_get_const (match, E_CONTACT_EMAIL_2),str)) &&
						(g_ascii_strcasecmp (e_contact_get_const (match, E_CONTACT_EMAIL_3),str)))
					res = FALSE;
				break;
			}
		}
		else {
			string = (gchar *) e_contact_get_const (contact, field);
			string1 = (gchar *) e_contact_get_const (match, field);
			if ((string && *string) && (string1 && *string1) && (g_ascii_strcasecmp (string1, string))) {
				res = FALSE;
				break;
			/*if the field entry exist in either of the contacts,we'll have to give the choice and thus merge button should be sensitive*/
			} else if ((string && *string) && !(string1 && *string1)) {
				res = FALSE;
				break;
			}
		}
	}

	g_list_free_full (email_attr_list, (GDestroyNotify) e_vcard_attribute_free);

	return res;
}

static void
response (GtkWidget *dialog,
          gint response,
          EContactMergingLookup *lookup)
{
	static gint merge_response;

	switch (response) {
	case 0:
		doit (lookup, FALSE);
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
match_query_callback (EContact *contact,
                      EContact *match,
                      EABContactMatchType type,
                      gpointer closure)
{
	EContactMergingLookup *lookup = closure;
	gint flag;
	gboolean same_uids;

	if (lookup->op == E_CONTACT_MERGING_FIND) {
		if (lookup->c_cb)
			lookup->c_cb (
				lookup->book_client, NULL,
				(gint) type <= (gint)
				EAB_CONTACT_MATCH_VAGUE ? NULL : match,
				lookup->closure);

		free_lookup (lookup);
		finished_lookup ();
		return;
	}

	/* if had same UID, then we are editing old contact, thus force commit change to it */
	same_uids = contact && match
		&& e_contact_get_const (contact, E_CONTACT_UID)
		&& e_contact_get_const (match, E_CONTACT_UID)
		&& g_str_equal (e_contact_get_const (contact, E_CONTACT_UID), e_contact_get_const (match, E_CONTACT_UID));

	if ((gint) type <= (gint) EAB_CONTACT_MATCH_VAGUE || same_uids) {
		doit (lookup, same_uids);
	} else {
		GtkBuilder *builder;
		GtkWidget *container;
		GtkWidget *merge_button;
		GtkWidget *widget;

		/* XXX I think we're leaking the GtkBuilder. */
		builder = gtk_builder_new ();

		lookup->match = g_object_ref (match);
		if (lookup->op == E_CONTACT_MERGING_ADD) {
			/* Compares all the values of contacts and return true, if they match */
			flag = check_if_same (contact, match);
			e_load_ui_builder_definition (
				builder, "eab-contact-duplicate-detected.ui");
			merge_button = e_builder_get_widget (builder, "button5");
			/* Merge Button not sensitive when all values are same */
			if (flag)
				gtk_widget_set_sensitive (GTK_WIDGET (merge_button), FALSE);
		} else if (lookup->op == E_CONTACT_MERGING_COMMIT) {
			e_load_ui_builder_definition (
				builder, "eab-contact-commit-duplicate-detected.ui");
		} else {
			doit (lookup, FALSE);
			return;
		}

		widget = e_builder_get_widget (builder, "custom-old-contact");
		eab_contact_display_set_mode (
			EAB_CONTACT_DISPLAY (widget),
			EAB_CONTACT_DISPLAY_RENDER_COMPACT);
		eab_contact_display_set_contact (
			EAB_CONTACT_DISPLAY (widget), match);

		widget = e_builder_get_widget (builder, "custom-new-contact");
		eab_contact_display_set_mode (
			EAB_CONTACT_DISPLAY (widget),
			EAB_CONTACT_DISPLAY_RENDER_COMPACT);
		eab_contact_display_set_contact (
			EAB_CONTACT_DISPLAY (widget), contact);

		widget = e_builder_get_widget (builder, "dialog-duplicate-contact");

		gtk_widget_ensure_style (widget);

		container = gtk_dialog_get_action_area (GTK_DIALOG (widget));
		gtk_container_set_border_width (GTK_CONTAINER (container), 12);

		container = gtk_dialog_get_content_area (GTK_DIALOG (widget));
		gtk_container_set_border_width (GTK_CONTAINER (container), 0);

		g_signal_connect (
			widget, "response",
			G_CALLBACK (response), lookup);

		gtk_widget_show_all (widget);
	}
}

gboolean
eab_merging_book_add_contact (ESourceRegistry *registry,
                              EBookClient *book_client,
                              EContact *contact,
                              EABMergingIdAsyncCallback cb,
                              gpointer closure)
{
	EContactMergingLookup *lookup;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	lookup = g_new (EContactMergingLookup, 1);

	lookup->op = E_CONTACT_MERGING_ADD;
	lookup->registry = g_object_ref (registry);
	lookup->book_client = g_object_ref (book_client);
	lookup->contact = g_object_ref (contact);
	lookup->id_cb = cb;
	lookup->closure = closure;
	lookup->avoid = NULL;
	lookup->match = NULL;

	add_lookup (lookup);

	return TRUE;
}

gboolean
eab_merging_book_modify_contact (ESourceRegistry *registry,
                                 EBookClient *book_client,
                                 EContact *contact,
                                 EABMergingAsyncCallback cb,
                                 gpointer closure)
{
	EContactMergingLookup *lookup;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	lookup = g_new (EContactMergingLookup, 1);

	lookup->op = E_CONTACT_MERGING_COMMIT;
	lookup->registry = g_object_ref (registry);
	lookup->book_client = g_object_ref (book_client);
	lookup->contact = g_object_ref (contact);
	lookup->cb = cb;
	lookup->closure = closure;
	lookup->avoid = g_list_append (NULL, contact);
	lookup->match = NULL;

	add_lookup (lookup);

	return TRUE;
}

gboolean
eab_merging_book_find_contact (ESourceRegistry *registry,
                               EBookClient *book_client,
                               EContact *contact,
                               EABMergingContactAsyncCallback cb,
                               gpointer closure)
{
	EContactMergingLookup *lookup;

	lookup = g_new (EContactMergingLookup, 1);

	lookup->op = E_CONTACT_MERGING_FIND;
	lookup->registry = g_object_ref (registry);
	lookup->book_client = g_object_ref (book_client);
	lookup->contact = g_object_ref (contact);
	lookup->c_cb = cb;
	lookup->closure = closure;
	lookup->avoid = g_list_append (NULL, contact);
	lookup->match = NULL;

	add_lookup (lookup);

	return TRUE;
}
