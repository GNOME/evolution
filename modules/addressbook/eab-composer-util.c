/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "eab-composer-util.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libebook/e-contact.h>
#include <libebook/e-destination.h>

#include "composer/e-msg-composer.h"
#include "addressbook/util/eab-book-util.h"
#include "addressbook/gui/widgets/eab-gui-util.h"

void
eab_send_as_to (EShell *shell,
                GList *destinations)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	GPtrArray *to_array;
	GPtrArray *bcc_array;

	union {
		gpointer *pdata;
		EDestination **destinations;
	} convert;

	g_return_if_fail (E_IS_SHELL (shell));

	if (destinations == NULL)
		return;

	composer = e_msg_composer_new (shell);
	table = e_msg_composer_get_header_table (composer);

	to_array = g_ptr_array_new ();
	bcc_array = g_ptr_array_new ();

	/* Sort contacts into "To" and "Bcc" destinations. */
	while (destinations != NULL) {
		EDestination *destination = destinations->data;

		if (e_destination_is_evolution_list (destination)) {
			if (e_destination_list_show_addresses (destination))
				g_ptr_array_add (to_array, destination);
			else
				g_ptr_array_add (bcc_array, destination);
		} else
			g_ptr_array_add (to_array, destination);

		destinations = g_list_next (destinations);
	}

	/* Add sentinels to each array. */
	g_ptr_array_add (to_array, NULL);
	g_ptr_array_add (bcc_array, NULL);

	/* XXX Acrobatics like this make me question whether NULL-terminated
	 *     arrays are really the best argument type for passing a list of
	 *     destinations to the header table. */

	/* Set "To" destinations. */
	convert.pdata = to_array->pdata;
	e_composer_header_table_set_destinations_to (
		table, convert.destinations);
	g_ptr_array_free (to_array, FALSE);

	/* Add "Bcc" destinations. */
	convert.pdata = bcc_array->pdata;
	e_composer_header_table_add_destinations_bcc (
		table, convert.destinations);
	g_ptr_array_free (bcc_array, FALSE);

	gtk_widget_show (GTK_WIDGET (composer));
}

static const gchar *
get_email (EContact *contact, EContactField field_id, gchar **to_free)
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

void
eab_send_as_attachment (EShell *shell,
                        GList *destinations)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	CamelMimePart *attachment;
	GList *contacts, *iter;
	gchar *data;

	g_return_if_fail (E_IS_SHELL (shell));

	if (destinations == NULL)
		return;

	composer = e_msg_composer_new (shell);
	table = e_msg_composer_get_header_table (composer);

	attachment = camel_mime_part_new ();

	contacts = g_list_copy (destinations);
	for (iter = contacts; iter != NULL; iter = iter->next)
		iter->data = e_destination_get_contact (iter->data);
	data = eab_contact_list_to_string (contacts);
	g_list_free (contacts);

	camel_mime_part_set_content (
		attachment, data, strlen (data), "text/x-vcard");

	if (destinations->next != NULL)
		camel_mime_part_set_description (
			attachment, _("Multiple vCards"));
	else {
		EContact *contact;
		const gchar *file_as;
		gchar *description;

		contact = e_destination_get_contact (destinations->data);
		file_as = e_contact_get_const (contact, E_CONTACT_FILE_AS);
		description = g_strdup_printf (_("vCard for %s"), file_as);
		camel_mime_part_set_description (attachment, description);
		g_free (description);
	}

	camel_mime_part_set_disposition (attachment, "attachment");

	e_msg_composer_attach (composer, attachment);
	g_object_unref (attachment);

	if (destinations->next != NULL)
		e_composer_header_table_set_subject (
			table, _("Contact information"));
	else {
		EContact *contact;
		gchar *tempstr;
		const gchar *tempstr2;
		gchar *tempfree = NULL;

		contact = e_destination_get_contact (destinations->data);
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

	gtk_widget_show (GTK_WIDGET (composer));
}
