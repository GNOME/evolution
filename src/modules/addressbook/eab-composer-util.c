/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "eab-composer-util.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libebook/libebook.h>

#include "composer/e-msg-composer.h"
#include "addressbook/util/eab-book-util.h"
#include "addressbook/gui/widgets/eab-gui-util.h"

static const gchar *
get_email (EContact *contact,
           EContactField field_id,
           gchar **to_free)
{
	gchar *name = NULL, *mail = NULL;
	const gchar *value = e_contact_get_const (contact, field_id);

	*to_free = NULL;

	if (eab_parse_qp_email (value, &name, &mail)) {
		*to_free = g_strdup_printf ("%s <%s>", name, mail);
		value = *to_free;
	}

	g_free (name);
	g_free (mail);

	return value;
}

typedef struct _CreateComposerData {
	EDestination **to_destinations;
	EDestination **bcc_destinations;
	GSList *attachment_destinations;
} CreateComposerData;

static void
eab_composer_created_cb (GObject *source_object,
			 GAsyncResult *result,
			 gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EComposerHeaderTable *table;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		table = e_msg_composer_get_header_table (composer);

		if (ccd->to_destinations)
			e_composer_header_table_set_destinations_to (table, ccd->to_destinations);

		if (ccd->bcc_destinations)
			e_composer_header_table_set_destinations_bcc (table, ccd->bcc_destinations);

		if (ccd->attachment_destinations) {
			CamelMimePart *attachment;
			GSList *contacts, *iter;
			gchar *data;

			attachment = camel_mime_part_new ();

			contacts = g_slist_copy (ccd->attachment_destinations);
			for (iter = contacts; iter != NULL; iter = iter->next)
				iter->data = e_destination_get_contact (iter->data);
			data = eab_contact_list_to_string (contacts);
			g_slist_free (contacts);

			camel_mime_part_set_content (attachment, data, strlen (data), "text/x-vcard");

			if (ccd->attachment_destinations->next != NULL) {
				camel_mime_part_set_description (attachment, _("Multiple vCards"));
			} else {
				EContact *contact;
				const gchar *file_as;
				gchar *description;

				contact = e_destination_get_contact (ccd->attachment_destinations->data);
				file_as = e_contact_get_const (contact, E_CONTACT_FILE_AS);
				description = g_strdup_printf (_("vCard for %s"), file_as);
				camel_mime_part_set_description (attachment, description);
				g_free (description);
			}

			camel_mime_part_set_disposition (attachment, "attachment");

			e_msg_composer_attach (composer, attachment);
			g_object_unref (attachment);

			if (ccd->attachment_destinations->next != NULL) {
				e_composer_header_table_set_subject (table, _("Contact information"));
			} else {
				EContact *contact;
				gchar *tempstr;
				const gchar *tempstr2;
				gchar *tempfree = NULL;

				contact = e_destination_get_contact (ccd->attachment_destinations->data);
				tempstr2 = e_contact_get_const (contact, E_CONTACT_FILE_AS);
				if (!tempstr2 || !*tempstr2)
					tempstr2 = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
				if (!tempstr2 || !*tempstr2)
					tempstr2 = e_contact_get_const (contact, E_CONTACT_ORG);
				if (!tempstr2 || !*tempstr2) {
					g_free (tempfree);
					tempstr2 = get_email (contact, E_CONTACT_EMAIL_1, &tempfree);
				}
				if (!tempstr2 || !*tempstr2) {
					g_free (tempfree);
					tempstr2 = get_email (contact, E_CONTACT_EMAIL_2, &tempfree);
				}
				if (!tempstr2 || !*tempstr2) {
					g_free (tempfree);
					tempstr2 = get_email (contact, E_CONTACT_EMAIL_3, &tempfree);
				}

				if (!tempstr2 || !*tempstr2)
					tempstr = g_strdup_printf (_("Contact information"));
				else
					tempstr = g_strdup_printf (_("Contact information for %s"), tempstr2);

				e_composer_header_table_set_subject (table, tempstr);

				g_free (tempstr);
				g_free (tempfree);
			}
		}

		gtk_widget_show (GTK_WIDGET (composer));
	}

	if (ccd->to_destinations)
		e_destination_freev (ccd->to_destinations);
	if (ccd->bcc_destinations)
		e_destination_freev (ccd->bcc_destinations);
	g_slist_free_full (ccd->attachment_destinations, g_object_unref);

	g_slice_free (CreateComposerData, ccd);
}

void
eab_send_as_to (EShell *shell,
                GSList *destinations)
{
	GPtrArray *to_array;
	GPtrArray *bcc_array;
	CreateComposerData *ccd;

	g_return_if_fail (E_IS_SHELL (shell));

	if (destinations == NULL)
		return;

	to_array = g_ptr_array_new ();
	bcc_array = g_ptr_array_new ();

	/* Sort contacts into "To" and "Bcc" destinations. */
	while (destinations != NULL) {
		EDestination *destination = destinations->data;

		if (e_destination_is_evolution_list (destination)) {
			if (e_destination_list_show_addresses (destination))
				g_ptr_array_add (to_array, e_destination_copy (destination));
			else
				g_ptr_array_add (bcc_array, e_destination_copy (destination));
		} else
			g_ptr_array_add (to_array, e_destination_copy (destination));

		destinations = g_slist_next (destinations);
	}

	/* Add sentinels to each array. */
	g_ptr_array_add (to_array, NULL);
	g_ptr_array_add (bcc_array, NULL);

	ccd = g_slice_new0 (CreateComposerData);
	ccd->to_destinations = (EDestination **) g_ptr_array_free (to_array, FALSE);
	ccd->bcc_destinations = (EDestination **) g_ptr_array_free (bcc_array, FALSE);
	ccd->attachment_destinations = NULL;

	e_msg_composer_new (shell, eab_composer_created_cb, ccd);
}

void
eab_send_as_attachment (EShell *shell,
                        GSList *destinations)
{
	CreateComposerData *ccd;

	g_return_if_fail (E_IS_SHELL (shell));

	if (destinations == NULL)
		return;

	ccd = g_slice_new0 (CreateComposerData);
	ccd->attachment_destinations = g_slist_copy (destinations);
	g_slist_foreach (ccd->attachment_destinations, (GFunc) g_object_ref, NULL);

	e_msg_composer_new (shell, eab_composer_created_cb, ccd);
}
