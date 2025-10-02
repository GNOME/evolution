/*
 * Code for checking for duplicates when doing EContact work.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Christopher James Lahey <clahey@ximian.com>
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "eab-contact-merging.h"
#include "eab-contact-compare.h"
#include <gtk/gtk.h>
#include <string.h>
#include "addressbook/gui/widgets/eab-contact-display.h"
#include "addressbook/util/eab-book-util.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include <glib/gi18n.h>

#include <camel/camel.h>

typedef enum {
	E_CONTACT_MERGING_ADD,
	E_CONTACT_MERGING_COMMIT,
	E_CONTACT_MERGING_FIND
} EContactMergingOpType;

typedef struct _MergeDialogData {
	GtkWidget *dialog;
	GList *use_email_attr_list, *contact_email_attr_list, *match_email_attr_list; /* EVCardAttribute * */
	GList *use_tel_attr_list, *contact_tel_attr_list, *match_tel_attr_list; /* EVCardAttribute * */
	GList *use_sip_attr_list, *contact_sip_attr_list, *match_sip_attr_list; /* EVCardAttribute * */
	GList *use_im_list, *contact_im_list, *match_im_list; /* gchar * */
	gint row;
} MergeDialogData;

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

	MergeDialogData *merge_dialog_data;

	gboolean can_add_copy;
} EContactMergingLookup;

typedef struct _dropdown_data {
	EContact *match;
	EContactField field;

	/* which list item to set to the attr or NULL */
	GList *list_item;
	gpointer item;
} dropdown_data;

static void match_query_callback (EContact *contact, EContact *match, EABContactMatchType type, gpointer closure);

#define SIMULTANEOUS_MERGING_REQUESTS 20
#define EVOLUTION_UI_SLOT_PARAM "X-EVOLUTION-UI-SLOT"

static GList *merging_queue = NULL;
static gint running_merge_requests = 0;

static void
merge_dialog_data_free (MergeDialogData *mdd)
{
	if (!mdd)
		return;

	gtk_widget_destroy (mdd->dialog);

	g_list_free (mdd->match_email_attr_list);
	g_list_free (mdd->contact_email_attr_list);
	g_list_free (mdd->use_email_attr_list);

	g_list_free (mdd->match_tel_attr_list);
	g_list_free (mdd->contact_tel_attr_list);
	g_list_free (mdd->use_tel_attr_list);

	g_list_free (mdd->match_im_list);
	g_list_free (mdd->contact_im_list);
	g_list_free (mdd->use_im_list);

	g_list_free (mdd->match_sip_attr_list);
	g_list_free (mdd->contact_sip_attr_list);
	g_list_free (mdd->use_sip_attr_list);

	g_slice_free (MergeDialogData, mdd);
}

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

static EContactMergingLookup *
new_lookup (void)
{
	return g_slice_new0 (EContactMergingLookup);
}

static void
free_lookup (EContactMergingLookup *lookup)
{
	merge_dialog_data_free (lookup->merge_dialog_data);
	g_object_unref (lookup->registry);
	g_object_unref (lookup->book_client);
	g_object_unref (lookup->contact);
	g_list_free (lookup->avoid);
	if (lookup->match)
		g_object_unref (lookup->match);
	g_slice_free (EContactMergingLookup, lookup);
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

static gboolean
ask_should_add (EContactMergingLookup *lookup)
{
	ESource *source;
	gchar *contact_info;
	gint response;

	source = e_client_get_source (E_CLIENT (lookup->book_client));

	contact_info = e_contact_get (lookup->contact, E_CONTACT_FILE_AS);
	if (!contact_info || !*contact_info) {
		g_free (contact_info);
		contact_info = e_contact_get (lookup->contact, E_CONTACT_FULL_NAME);
	}

	response = e_alert_run_dialog_for_args (NULL,
		"addressbook:ask-add-existing",
		contact_info && *contact_info ? contact_info : _("Unnamed"),
		e_source_get_display_name (source), NULL);

	g_free (contact_info);

	return response == GTK_RESPONSE_ACCEPT;
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

	if (error != NULL)
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

	e_book_client_add_contact_finish (book_client, result, &uid, &error);

	if (lookup->can_add_copy && g_error_matches (error, E_BOOK_CLIENT_ERROR, E_BOOK_CLIENT_ERROR_CONTACT_ID_ALREADY_EXISTS)) {
		lookup->can_add_copy = FALSE;

		if (ask_should_add (lookup)) {
			gchar *new_uid;

			new_uid = e_util_generate_uid ();
			e_contact_set (lookup->contact, E_CONTACT_UID, new_uid);
			g_free (new_uid);

			e_book_client_add_contact (lookup->book_client, lookup->contact, E_BOOK_OPERATION_FLAG_NONE, NULL, add_contact_ready_cb, lookup);
		} else {
			g_clear_error (&error);
			final_id_cb (book_client, error, uid, lookup);
		}
	} else {
		final_id_cb (book_client, error, uid, lookup);
	}

	if (error != NULL)
		g_error_free (error);
	g_free (uid);
}

static void
doit (EContactMergingLookup *lookup,
      gboolean force_modify)
{
	if (lookup->op == E_CONTACT_MERGING_ADD) {
		if (force_modify)
			e_book_client_modify_contact (lookup->book_client, lookup->contact, E_BOOK_OPERATION_FLAG_NONE, NULL, modify_contact_ready_cb, lookup);
		else
			e_book_client_add_contact (lookup->book_client, lookup->contact, E_BOOK_OPERATION_FLAG_NONE, NULL, add_contact_ready_cb, lookup);
	} else if (lookup->op == E_CONTACT_MERGING_COMMIT)
		e_book_client_modify_contact (lookup->book_client, lookup->contact, E_BOOK_OPERATION_FLAG_NONE, NULL, modify_contact_ready_cb, lookup);
}

static void
cancelit (EContactMergingLookup *lookup)
{
	GError *error;

	error = g_error_new_literal (
		G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Cancelled"));

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
            GtkWidget *grid)
{
	GtkAllocation allocation;
	gint h, w;

	gtk_widget_get_allocation (grid, &allocation);

	/* Spacing around the grid */
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
	gchar *str = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (dropdown));

	if (str && *str)
		e_contact_set (data->match, data->field, str);
	else
		e_contact_set (data->match, data->field, NULL);

	g_free (str);
}

static void
attr_dropdown_changed (GtkWidget *dropdown,
			dropdown_data *data)
{
	gchar *str = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (dropdown));

	if (str && *str)
		data->list_item->data = data->item;
	else
		data->list_item->data = NULL;

	g_free (str);
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
		book_client, lookup->contact, E_BOOK_OPERATION_FLAG_NONE, NULL,
		add_contact_ready_cb, lookup);
}

static void
create_dropdowns_for_multival_attr (GList *match_attr_list,
				    GList *contact_attr_list,
				    GList **use_attr_list,
				    gint *row,
				    GtkGrid *grid,
				    const gchar * (*label_str) (EVCardAttribute*))
{
	GtkWidget *label, *dropdown;
	GList *miter, *citer;
	GHashTable *match_attrs; /* attr in the 'match' contact from address book */

	match_attrs = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);

	for (miter = match_attr_list; miter; miter = g_list_next (miter)) {
		EVCardAttribute *attr = miter->data;
		gchar *value;

		value = e_vcard_attribute_get_value (attr);
		if (value && *value) {
			g_hash_table_insert (match_attrs, value, attr);
			*use_attr_list = g_list_prepend (*use_attr_list, attr);
		} else {
			g_free (value);
		}
	}

	*use_attr_list = g_list_reverse (*use_attr_list);

	for (citer = contact_attr_list; citer; citer = g_list_next (citer)) {
		EVCardAttribute *attr = citer->data;
		gchar *value;

		value = e_vcard_attribute_get_value (attr);
		if (value && *value) {
			if (!g_hash_table_lookup (match_attrs, value)) {
				dropdown_data *data;

				/* the attr is not set in both contacts */
				*use_attr_list = g_list_append (*use_attr_list, attr);

				/* remove to avoid collisions with match UI_SLOTs */
				e_vcard_attribute_remove_param (attr, EVOLUTION_UI_SLOT_PARAM);

				(*row)++;
				label = gtk_label_new (label_str (attr));
				gtk_grid_attach (grid, label, 0, *row, 1, 1);

				dropdown = gtk_combo_box_text_new ();
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), value);

				data = g_new0 (dropdown_data, 1);

				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), "");

				gtk_combo_box_set_active (GTK_COMBO_BOX (dropdown), 0);

				data->list_item = g_list_last (*use_attr_list);
				data->item = attr;

				g_signal_connect (
					dropdown, "changed",
					G_CALLBACK (attr_dropdown_changed), data);
				g_object_set_data_full (G_OBJECT (dropdown), "eab-contact-merging::dropdown-data", data, g_free);

				gtk_grid_attach (grid, dropdown, 1, *row, 1, 1);
			}
		}
		g_free (value);
	}
	g_hash_table_destroy (match_attrs);
}

static void
create_dropdowns_for_multival_str (GList *match_list, /* gchar * */
				   GList *contact_list, /* gchar * */
				   GList **use_list, /* (element-type utf8) */
				   gint *row,
				   GtkGrid *grid,
				   const gchar * (*label_str) (const gchar *value))
{
	GtkWidget *label, *dropdown;
	GList *miter, *citer;
	GHashTable *match_values; /* values in the 'match' contact from address book */

	match_values = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);

	for (miter = match_list; miter; miter = g_list_next (miter)) {
		const gchar *value = miter->data;

		if (value && *value) {
			g_hash_table_add (match_values, g_strdup (value));
			*use_list = g_list_prepend (*use_list, (gpointer) (gpointer) value);
		}
	}

	*use_list = g_list_reverse (*use_list);

	for (citer = contact_list; citer; citer = g_list_next (citer)) {
		const gchar *value = citer->data;

		if (value && *value && !g_hash_table_lookup (match_values, value)) {
			dropdown_data *data;

			/* the value is not set in both contacts */
			*use_list = g_list_append (*use_list, (gpointer) value);

			(*row)++;

			label = gtk_label_new (label_str (value));
			gtk_grid_attach (grid, label, 0, *row, 1, 1);

			dropdown = gtk_combo_box_text_new ();
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), value);

			data = g_new0 (dropdown_data, 1);

			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), "");

			gtk_combo_box_set_active (GTK_COMBO_BOX (dropdown), 0);

			data->list_item = g_list_last (*use_list);
			data->item = (gpointer) value;

			g_signal_connect (
				dropdown, "changed",
				G_CALLBACK (attr_dropdown_changed), data);
			g_object_set_data_full (G_OBJECT (dropdown), "eab-contact-merging::dropdown-data", data, g_free);

			gtk_grid_attach (grid, dropdown, 1, *row, 1, 1);
		}
	}
	g_hash_table_destroy (match_values);
}

static const gchar *
get_impp_label_text (const gchar *value)
{
	return eab_get_impp_label_text (value, NULL, NULL);
}

static void
set_attributes (EContact *contact,
		const gchar *vcard_field,
		GList *use_attr_list) /* EVCardAttribute * */
{
	GList *miter, *citer;

	citer = NULL;
	for (miter = use_attr_list; miter; miter = g_list_next (miter)) {
		if (miter->data)
			citer = g_list_prepend (citer, e_vcard_attribute_copy (miter->data));
	}
	citer = g_list_reverse (citer);

	e_vcard_remove_attributes (E_VCARD (contact), NULL, vcard_field);
	e_vcard_append_attributes_take (E_VCARD (contact), citer);
}

static MergeDialogData *
merge_dialog_data_create (EContactMergingLookup *lookup,
			  GtkWidget *parent)
{
	GtkWidget *scrolled_window, *label, *dropdown;
	GtkWidget *content_area;
	GtkGrid *grid;
	EContactField field;
	EVCard *vcard_contact, *vcard_match;
	gchar *string = NULL, *string1 = NULL;
	MergeDialogData *mdd;

	mdd = g_slice_new0 (MergeDialogData);
	mdd->row = -1;

	mdd->dialog = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (mdd->dialog), _("Merge Contact"));
	gtk_container_set_border_width (GTK_CONTAINER (mdd->dialog), 5);
	if (GTK_IS_WINDOW (parent))
		gtk_window_set_transient_for (GTK_WINDOW (mdd->dialog), GTK_WINDOW (parent));

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (mdd->dialog));

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (G_OBJECT (grid),
		"border-width", 12,
		"row-spacing", 6,
		"column-spacing", 2,
		NULL);

	gtk_dialog_add_buttons (
		GTK_DIALOG (mdd->dialog),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Merge"), GTK_RESPONSE_OK,
		NULL);

	/*we match all the string fields of the already existing contact and the new contact.*/
	for (field = E_CONTACT_FULL_NAME; field != (E_CONTACT_LAST_SIMPLE_STRING -1); field++) {
		dropdown_data *data = NULL;
		string = (gchar *) e_contact_get_const (lookup->contact, field);
		string1 = (gchar *) e_contact_get_const (lookup->match, field);

		/*the field must exist in the new as well as the duplicate contact*/
		if (string && *string) {
			if ((field >= E_CONTACT_FIRST_EMAIL_ID && field <= E_CONTACT_LAST_EMAIL_ID) ||
			    (field >= E_CONTACT_FIRST_PHONE_ID && field <= E_CONTACT_LAST_PHONE_ID) ||
			    (field >= E_CONTACT_IM_AIM_HOME_1 && field <= E_CONTACT_IM_ICQ_WORK_3) ||
			    (field >= E_CONTACT_IM_GADUGADU_HOME_1 && field <= E_CONTACT_IM_GADUGADU_WORK_3) ||
			    (field >= E_CONTACT_IM_SKYPE_HOME_1 && field <= E_CONTACT_IM_SKYPE_WORK_3) ||
			    (field >= E_CONTACT_IM_GOOGLE_TALK_HOME_1 && field <= E_CONTACT_IM_GOOGLE_TALK_WORK_3) ||
			    (field >= E_CONTACT_IM_MATRIX_HOME_1 && field <= E_CONTACT_IM_MATRIX_WORK_3) ||
			    field == E_CONTACT_IMPP) {
				/* ignore multival attributes, they are compared after this for-loop */
				continue;
			}

			if (!(string1 && *string1) || (g_ascii_strcasecmp (string, string1))) {
				mdd->row++;
				label = gtk_label_new (e_contact_pretty_name (field));
				gtk_grid_attach (grid, label, 0, mdd->row, 1, 1);
				data = g_new0 (dropdown_data, 1);
				dropdown = gtk_combo_box_text_new ();
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), string);

				if (string1 && *string1)
					gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), string1);
				else
					gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dropdown), "");

				data->field = field;
				data->match = lookup->match;

				g_signal_connect (
					dropdown, "changed",
					G_CALLBACK (dropdown_changed), data);
				g_object_set_data_full (G_OBJECT (dropdown), "eab-contact-merging::dropdown-data", data, g_free);

				/* Only prefer the original value when it's filled */
				if (string1 && *string1 && (
				    field == E_CONTACT_NICKNAME ||
				    field == E_CONTACT_GIVEN_NAME ||
				    field == E_CONTACT_FAMILY_NAME ||
				    field == E_CONTACT_FULL_NAME))
					gtk_combo_box_set_active (GTK_COMBO_BOX (dropdown), 1);
				else
					gtk_combo_box_set_active (GTK_COMBO_BOX (dropdown), 0);

				gtk_grid_attach (grid, dropdown, 1, mdd->row, 1, 1);
			}
		}
	}

	vcard_contact = E_VCARD (lookup->contact);
	vcard_match = E_VCARD (lookup->match);

	mdd->match_email_attr_list = e_vcard_get_attributes_by_name (vcard_match, EVC_EMAIL);
	mdd->contact_email_attr_list = e_vcard_get_attributes_by_name (vcard_contact, EVC_EMAIL);
	mdd->use_email_attr_list = NULL;
	create_dropdowns_for_multival_attr (mdd->match_email_attr_list, mdd->contact_email_attr_list,
	                                   &(mdd->use_email_attr_list), &(mdd->row), grid, eab_get_email_label_text);

	mdd->match_tel_attr_list = e_vcard_get_attributes_by_name (vcard_match, EVC_TEL);
	mdd->contact_tel_attr_list = e_vcard_get_attributes_by_name (vcard_contact, EVC_TEL);
	mdd->use_tel_attr_list = NULL;
	create_dropdowns_for_multival_attr (mdd->match_tel_attr_list, mdd->contact_tel_attr_list,
	                                   &(mdd->use_tel_attr_list), &(mdd->row), grid, eab_get_phone_label_text);

	mdd->match_sip_attr_list = e_vcard_get_attributes_by_name (vcard_match, EVC_X_SIP);
	mdd->contact_sip_attr_list = e_vcard_get_attributes_by_name (vcard_contact, EVC_X_SIP);
	mdd->use_sip_attr_list = NULL;
	create_dropdowns_for_multival_attr (mdd->match_sip_attr_list, mdd->contact_sip_attr_list,
	                                   &(mdd->use_sip_attr_list), &(mdd->row), grid, eab_get_sip_label_text);

	mdd->match_im_list = e_contact_get (lookup->match, E_CONTACT_IMPP);
	mdd->contact_im_list = e_contact_get (lookup->contact, E_CONTACT_IMPP);
	mdd->use_im_list = NULL;
	create_dropdowns_for_multival_str (mdd->match_im_list, mdd->contact_im_list,
					   &(mdd->use_im_list), &(mdd->row), grid, get_impp_label_text);

	gtk_window_set_default_size (GTK_WINDOW (mdd->dialog), 420, 300);
	gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (grid));
	gtk_box_pack_start (GTK_BOX (content_area), GTK_WIDGET (scrolled_window), TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);
	g_signal_connect (
		mdd->dialog, "map-event",
		G_CALLBACK (dialog_map), grid);
	gtk_widget_show_all (GTK_WIDGET (grid));

	return mdd;
}

static void
set_impp (EContact *match,
	  GList *im_list)
{
	GList *link, *set_impp_list = NULL;

	for (link = im_list; link; link = g_list_next (link)) {
		const gchar *impp = link->data;

		if (impp && *impp)
			set_impp_list = g_list_prepend (set_impp_list, (gpointer) impp);
	}

	set_impp_list = g_list_reverse (set_impp_list);

	e_contact_set (match, E_CONTACT_IMPP, set_impp_list);

	g_list_free (set_impp_list);
}

static gint
mergeit (EContactMergingLookup *lookup,
	 GtkWidget *parent)
{
	GList *ll;
	gint value = 0, result;

	if (!lookup->merge_dialog_data)
		lookup->merge_dialog_data = merge_dialog_data_create (lookup, parent);

	if (lookup->merge_dialog_data->row == -1)
		result = GTK_RESPONSE_OK;
	else
		result = gtk_dialog_run (GTK_DIALOG (lookup->merge_dialog_data->dialog));

	switch (result) {
	case GTK_RESPONSE_OK:
		set_attributes (lookup->match, EVC_EMAIL, lookup->merge_dialog_data->use_email_attr_list);
		set_attributes (lookup->match, EVC_TEL, lookup->merge_dialog_data->use_tel_attr_list);
		set_attributes (lookup->match, EVC_X_SIP, lookup->merge_dialog_data->use_sip_attr_list);
		set_impp (lookup->match, lookup->merge_dialog_data->use_im_list);

		for (ll = lookup->merge_dialog_data->use_im_list; ll; ll = ll->next) {
			EVCard *vcard;
			vcard = E_VCARD (lookup->match);
			e_vcard_append_attribute (vcard, e_vcard_attribute_copy ((EVCardAttribute *) ll->data));
		}

		g_object_unref (lookup->contact);
		lookup->contact = g_object_ref (lookup->match);
		e_book_client_remove_contact (
			lookup->book_client,
			lookup->match, E_BOOK_OPERATION_FLAG_NONE, NULL,
			remove_contact_ready_cb, lookup);
		value = 1;
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		value = 0;
		break;
	}
	gtk_widget_hide (lookup->merge_dialog_data->dialog);

	return value;
}

static gboolean
check_if_same (EContact *contact,
               EContact *match)
{
	EVCard *vcard_contact = E_VCARD (contact);
	EVCard *vcard_match = E_VCARD (match);
	EContactField field;
	gchar *string = NULL, *string1 = NULL;
	gboolean res = TRUE;


	for (field = E_CONTACT_FULL_NAME; res && field != (E_CONTACT_LAST_SIMPLE_STRING -1); field++) {

		if (field == E_CONTACT_EMAIL_1) {
			GList *email_attr_list1, *email_attr_list2, *iter1, *iter2;
			gint num_of_email1, num_of_email2;

			email_attr_list1 = e_vcard_get_attributes_by_name (vcard_contact, EVC_EMAIL);
			num_of_email1 = g_list_length (email_attr_list1);

			email_attr_list2 = e_vcard_get_attributes_by_name (vcard_match, EVC_EMAIL);
			num_of_email2 = g_list_length (email_attr_list2);

			if (num_of_email1 != num_of_email2) {
				res = FALSE;
			} else { /* Do pairwise-comparisons on all of the e-mail addresses. */
				iter1 = email_attr_list1;
				while (iter1) {
					gboolean         matches = FALSE;
					EVCardAttribute *attr;
					gchar           *email_address1;

					attr = iter1->data;
					email_address1 = e_vcard_attribute_get_value (attr);

					iter2 = email_attr_list2;
					while ( iter2 && matches == FALSE) {
						gchar *email_address2;

						attr = iter2->data;
						email_address2 = e_vcard_attribute_get_value (attr);

						if (g_ascii_strcasecmp (email_address1, email_address2) == 0) {
							matches = TRUE;
						}

						g_free (email_address2);
						iter2 = g_list_next (iter2);
					}

					g_free (email_address1);
					iter1 = g_list_next (iter1);

					if (matches == FALSE) {
						res = FALSE;
						break;
					}
				}
			}

			g_list_free (email_attr_list1);
			g_list_free (email_attr_list2);
		} else if (field > E_CONTACT_FIRST_EMAIL_ID && field <= E_CONTACT_LAST_EMAIL_ID) {
			/* nothing to do, all emails are checked above */
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

	return res;
}

static GtkWidget *
create_duplicate_contact_detected_dialog (EContact *old_contact,
                                          EContact *new_contact,
                                          gboolean disable_merge,
                                          gboolean is_for_commit)
{
	GtkWidget *widget, *scrolled;
	GtkContainer *container;
	GtkDialog *dialog;
	const gchar *text;

	widget = gtk_dialog_new ();
	dialog = GTK_DIALOG (widget);

	g_object_set (
		G_OBJECT (dialog),
		"title", _("Duplicate Contact Detected"),
		"default-width", 500,
		"default-height", 400,
		NULL);

	gtk_dialog_add_action_widget (dialog, e_dialog_button_new_with_icon ("process-stop", _("_Cancel")), GTK_RESPONSE_CANCEL);

	if (is_for_commit) {
		gtk_dialog_add_action_widget (dialog, e_dialog_button_new_with_icon ("document-save", _("_Save")), GTK_RESPONSE_OK);
	} else {
		gtk_dialog_add_action_widget (dialog, e_dialog_button_new_with_icon ("list-add", _("_Add")), GTK_RESPONSE_OK);
		gtk_dialog_add_action_widget (dialog, e_dialog_button_new_with_icon (NULL, _("_Merge")), GTK_RESPONSE_APPLY);
	}

	if (disable_merge)
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_APPLY, FALSE);

	container = GTK_CONTAINER (gtk_dialog_get_content_area (dialog));

	widget = gtk_grid_new ();
	g_object_set (
		G_OBJECT (widget),
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"margin", 12,
		NULL);

	gtk_container_add (container, widget);
	container = GTK_CONTAINER (widget);

	widget = gtk_image_new_from_icon_name ("avatar-default", GTK_ICON_SIZE_BUTTON);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-end", 12,
		NULL);
	gtk_container_add (container, widget);

	widget = gtk_grid_new ();
	g_object_set (
		G_OBJECT (widget),
		"orientation", GTK_ORIENTATION_VERTICAL,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);

	gtk_container_add (container, widget);
	container = GTK_CONTAINER (widget);

	if (is_for_commit)
		text = _("The name or email address of this contact already exists\n"
			 "in this folder. Would you like to save the changes anyway?");
	else
		text = _("The name or email address of this contact already exists\n"
			 "in this folder. Would you like to add it anyway?");

	widget = gtk_label_new (text);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-bottom", 6,
		NULL);
	gtk_container_add (container, widget);

	if (is_for_commit)
		text = _("Changed Contact:");
	else
		text = _("New Contact:");

	widget = gtk_label_new (text);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-bottom", 6,
		NULL);
	gtk_container_add (container, widget);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (
		G_OBJECT (scrolled),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"margin-bottom", 6,
		NULL);
	gtk_container_add (container, scrolled);

	widget = eab_contact_display_new ();
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"contact", new_contact,
		"mode", EAB_CONTACT_DISPLAY_RENDER_COMPACT,
		NULL);
	gtk_container_add (GTK_CONTAINER (scrolled), widget);

	if (is_for_commit)
		text = _("Conflicting Contact:");
	else
		text = _("Old Contact:");

	widget = gtk_label_new (text);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-bottom", 6,
		NULL);
	gtk_container_add (container, widget);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (
		G_OBJECT (scrolled),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);
	gtk_container_add (container, scrolled);

	widget = eab_contact_display_new ();
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"contact", old_contact,
		"mode", EAB_CONTACT_DISPLAY_RENDER_COMPACT,
		NULL);
	gtk_container_add (GTK_CONTAINER (scrolled), widget);

	gtk_widget_show_all (gtk_dialog_get_content_area (dialog));

	return GTK_WIDGET (dialog);
}

static void
response (GtkWidget *dialog,
          gint response,
          EContactMergingLookup *lookup)
{
	switch (response) {
	case GTK_RESPONSE_OK:
		doit (lookup, FALSE);
		break;
	case GTK_RESPONSE_CANCEL:
		cancelit (lookup);
		break;
	case GTK_RESPONSE_APPLY:
		if (mergeit (lookup, dialog))
			break;
		return;
	case GTK_RESPONSE_DELETE_EVENT:
		cancelit (lookup);
		break;
	default:
		g_warn_if_reached ();
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
	gboolean flag;
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
		doit (lookup, same_uids && !lookup->can_add_copy);
	} else {
		GtkWidget *dialog;

		lookup->match = g_object_ref (match);
		if (lookup->op == E_CONTACT_MERGING_ADD) {
			/* Compares all the values of contacts and return true, if they match */
			flag = check_if_same (contact, match);
			dialog = create_duplicate_contact_detected_dialog (match, contact, flag, FALSE);
		} else if (lookup->op == E_CONTACT_MERGING_COMMIT) {
			dialog = create_duplicate_contact_detected_dialog (match, contact, FALSE, TRUE);
		} else {
			doit (lookup, FALSE);
			return;
		}

		g_signal_connect (
			dialog, "response",
			G_CALLBACK (response), lookup);

		gtk_widget_show_all (dialog);
	}
}

gboolean
eab_merging_book_add_contact (ESourceRegistry *registry,
                              EBookClient *book_client,
                              EContact *contact,
                              EABMergingIdAsyncCallback cb,
                              gpointer closure,
			      gboolean can_add_copy)
{
	EContactMergingLookup *lookup;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	lookup = new_lookup ();

	lookup->op = E_CONTACT_MERGING_ADD;
	lookup->registry = g_object_ref (registry);
	lookup->book_client = g_object_ref (book_client);
	lookup->contact = g_object_ref (contact);
	lookup->id_cb = cb;
	lookup->closure = closure;
	lookup->avoid = NULL;
	lookup->match = NULL;
	lookup->can_add_copy = TRUE;

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

	lookup = new_lookup ();

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

	lookup = new_lookup ();

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
