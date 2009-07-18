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
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* ExchangeDelegatesUser: A single delegate */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exchange-delegates.h"
#include "exchange-delegates-user.h"

#include <mail/mail-ops.h>
#include <mail/mail-component.h>
#include <mail/mail-send-recv.h>
#include <camel/camel-multipart.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-stream-mem.h>
#include <exchange-account.h>
#include "exchange-delegates.h"
#include <exchange/exchange-account.h>
#include <e2k-global-catalog.h>
#include <e2k-marshal.h>
#include <e2k-sid.h>
#include <e2k-utils.h>

#include <e-util/e-dialog-utils.h>
#include <e-util/e-dialog-widgets.h>
#include <glade/glade.h>

#include <string.h>

#define EXCHANGE_DELEGATES_USER_CUSTOM    -3
/* Can't use E2K_PERMISSIONS_ROLE_CUSTOM, because it's -1, which
 * means "end of list" to e_dialog_combo_box_get/set
 */

static const gint exchange_perm_map[] = {
	E2K_PERMISSIONS_ROLE_NONE,
	E2K_PERMISSIONS_ROLE_REVIEWER,
	E2K_PERMISSIONS_ROLE_AUTHOR,
	E2K_PERMISSIONS_ROLE_EDITOR,

	EXCHANGE_DELEGATES_USER_CUSTOM,

	-1
};

const gchar *exchange_delegates_user_folder_names[] = {
	"calendar", "tasks", "inbox", "contacts"
};

/* To translators: The folder names to be displayed in the message being
   sent to the delegatee.
*/
static const gchar *folder_names_for_display[] = {
	N_("Calendar"), N_("Tasks"), N_("Inbox"), N_("Contacts")
};

static const gchar *widget_names[] = {
	"calendar_perms_combobox", "task_perms_combobox", "inbox_perms_combobox", "contact_perms_combobox",
};

enum {
	EDITED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

static void
finalize (GObject *object)
{
	ExchangeDelegatesUser *user = EXCHANGE_DELEGATES_USER (object);

	if (user->display_name)
		g_free (user->display_name);
	if (user->dn)
		g_free (user->dn);
	if (user->entryid)
		g_byte_array_free (user->entryid, TRUE);
	if (user->sid)
		g_object_unref (user->sid);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
class_init (GObjectClass *object_class)
{
	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->finalize = finalize;

	/* signals */
	signals[EDITED] =
		g_signal_new ("edited",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ExchangeDelegatesUserClass, edited),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

E2K_MAKE_TYPE (exchange_delegates_user, ExchangeDelegatesUser, class_init, NULL, PARENT_TYPE)

static inline gboolean
is_delegate_role (E2kPermissionsRole role)
{
	return (role == E2K_PERMISSIONS_ROLE_NONE ||
		role == E2K_PERMISSIONS_ROLE_REVIEWER ||
		role == E2K_PERMISSIONS_ROLE_AUTHOR ||
		role == E2K_PERMISSIONS_ROLE_EDITOR);
}

static void
set_perms (GtkWidget *combobox, E2kPermissionsRole role)
{
	if (!is_delegate_role (role)) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("Custom"));
		role = EXCHANGE_DELEGATES_USER_CUSTOM;
	}

	e_dialog_combo_box_set (combobox, role, exchange_perm_map);
}

static void
parent_window_destroyed (gpointer dialog, GObject *where_parent_window_was)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
}

/* Maps the role_nam parameter to their corresponding Full role name
*/
static const gchar *
map_to_full_role_name (E2kPermissionsRole role_nam)
{
	const gchar *role_name;

	switch (role_nam)
	{
	/* To translators: The following are the various types of permissions that can
	   assigned by an user to his folders.
	*/
		case E2K_PERMISSIONS_ROLE_EDITOR: role_name = g_strdup (
							_("Editor (read, create, edit)"));
						  break;

		case E2K_PERMISSIONS_ROLE_AUTHOR: role_name = g_strdup (
							_("Author (read, create)"));
						  break;

		case E2K_PERMISSIONS_ROLE_REVIEWER: role_name = g_strdup (
							_("Reviewer (read-only)"));
						    break;

		default: role_name = g_strdup (_("None"));
			 break;
	}

	return role_name;
}

static void
em_utils_delegates_done (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info,
		       gint queued, const gchar *appended_uid, gpointer data)
{
	camel_message_info_free (info);
	mail_send ();
}

/**
 * exchange_delegates_user_edit:
 * @user: a delegate
 * @parent_window: parent window for the editor dialog
 *
 * Brings up a dialog to edit @user's permissions as a delegate.
 * An %edited signal will be emitted if anything changed.
 *
 * Return value: %TRUE for "OK", %FALSE for "Cancel".
 **/
gboolean
exchange_delegates_user_edit (ExchangeAccount *account,
			     ExchangeDelegatesUser *user,
			     GtkWidget *parent_window)
{
	GladeXML *xml;
	GtkWidget *dialog, *table, *label, *combobox, *check, *check_delegate;
	gchar *title;
	gint button, i;
	E2kPermissionsRole role;
	gboolean modified;

	g_return_val_if_fail (EXCHANGE_IS_DELEGATES_USER (user), FALSE);
	g_return_val_if_fail (E2K_IS_SID (user->sid), FALSE);

	/* Grab the Glade widgets */
	xml = glade_xml_new (
		CONNECTOR_GLADEDIR "/exchange-delegates.glade",
		"delegate_permissions", PACKAGE);
	g_return_val_if_fail (xml, FALSE);

	title = g_strdup (_("Delegate Permissions"));

	dialog = glade_xml_get_widget (xml, "delegate_permissions");
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	e_dialog_set_transient_for (GTK_WINDOW (dialog), parent_window);
	g_free (title);

	table = glade_xml_get_widget (xml, "toplevel_table");
	gtk_widget_reparent (table, GTK_DIALOG (dialog)->vbox);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 6);

	title = g_strdup_printf (_("Permissions for %s"), user->display_name);
	label = glade_xml_get_widget (xml, "delegate_label");
	gtk_label_set_text (GTK_LABEL (label), title);
	g_free (title);

	/* Set up the permissions */
	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		combobox = glade_xml_get_widget (xml, widget_names[i]);
		set_perms (combobox, user->role[i]);
	}
	check = glade_xml_get_widget (xml, "see_private_checkbox");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
				      user->see_private);

	/* Run the dialog, while watching its parent. */
	g_object_weak_ref (G_OBJECT (parent_window),
			   parent_window_destroyed, dialog);
	g_object_add_weak_pointer (G_OBJECT (parent_window),
				   (gpointer*)&parent_window);
	button = gtk_dialog_run (GTK_DIALOG (dialog));
	if (parent_window) {
		g_object_remove_weak_pointer (G_OBJECT (parent_window),
					      (gpointer *)&parent_window);
		g_object_weak_unref (G_OBJECT (parent_window),
				     parent_window_destroyed, dialog);
	}

	if (button != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	/* And update */
	modified = FALSE;
	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		combobox = glade_xml_get_widget (xml, widget_names[i]);
		role = e_dialog_combo_box_get (combobox, exchange_perm_map);

		if (is_delegate_role (user->role[i]) &&
		    user->role[i] != role) {
			user->role[i] = role;
			modified = TRUE;
		}
	}

	/* The following piece of code is used to construct a mail message to be sent to a Delegate
	   summarizing all the permissions set for him on the user's various folders.
	*/
	check_delegate = glade_xml_get_widget (xml, "delegate_mail");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_delegate)) == TRUE) {
		if (button == GTK_RESPONSE_OK) {

			EAccount *eaccount;
			CamelMimeMessage *delegate_mail = camel_mime_message_new ();
			CamelMultipart *body = camel_multipart_new ();
			CamelMimePart *part;
			CamelDataWrapper *delegate_mail_text, *delegate_mail_data;
			CamelContentType *type;
			CamelInternetAddress *addr;
			CamelStream *stream;
			CamelFolder *out_folder;
			CamelMessageInfo *info;
			gchar *self_address, *delegate_mail_subject;
			gchar *role_name;
			GString *role_name_final;

			const gchar *recipient_address;
			const gchar *delegate_exchange_dn;
			const gchar *msg_part1 = NULL, *msg_part2 = NULL;

			role_name_final = g_string_new ("");

			self_address = g_strdup (exchange_account_get_email_id (account));

			/* Create toplevel container */
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body),
					"multipart/alternative;");
			camel_multipart_set_boundary (body, NULL);

			/* Create textual receipt */
			delegate_mail_text = camel_data_wrapper_new ();
			type = camel_content_type_new ("text", "html");
			camel_content_type_set_param (type, "format", "flowed");
			camel_data_wrapper_set_mime_type_field (delegate_mail_text, type);
			camel_content_type_unref (type);
			stream = camel_stream_mem_new ();

			/* To translators: This is a part of the message to be sent to the delegatee
			   summarizing the permissions assigned to him.
			*/
			msg_part1 = _("This message was sent automatically by Evolution to inform you that you have been "
					"designated as a delegate. You can now send messages on my behalf.");

			/* To translators: Another chunk of the same message.
			*/
			msg_part2 = _("You have been given the following permissions on my folders:");

			camel_stream_printf (stream,
				"<html><body><p>%s<br><br>%s</p><table border = 0 width=\"40%%\">", msg_part1, msg_part2);
			for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
				combobox = glade_xml_get_widget (xml, widget_names[i]);
				role = e_dialog_combo_box_get (combobox, exchange_perm_map);
				role_name = g_strdup (map_to_full_role_name(role));
				g_string_append_printf (
					role_name_final,
					"<tr><td>%s:</td><td>%s</td></tr>",
					folder_names_for_display[i], role_name);
			}

			camel_stream_printf (stream, "%s</table>", role_name_final->str);

			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)) == TRUE) {
				/* To translators: This message is included if the delegatee has been given access
				   to the private items.
				*/
				camel_stream_printf (stream, "<br>%s", _("You are also permitted "
							"to see my private items."));
			}
			else
				/* To translators: This message is included if the delegatee has not been given access
				   to the private items.
				*/
				camel_stream_printf (stream, "<br>%s", _("However you are not permitted "
							 "to see my private items."));
			camel_data_wrapper_construct_from_stream (delegate_mail_text, stream);
			g_free (role_name);
			g_string_free (role_name_final, TRUE);
			camel_object_unref (stream);

			part = camel_mime_part_new ();
			camel_medium_set_content_object (CAMEL_MEDIUM (part), delegate_mail_text);
			camel_object_unref (delegate_mail_text);
			camel_multipart_add_part (body, part);
			camel_object_unref (part);

			/* Create the machine-readable receipt */
			delegate_mail_data = camel_data_wrapper_new ();
			type = camel_content_type_new ("message", "disposition-notification");
			camel_data_wrapper_set_mime_type_field (delegate_mail_data, type);
			camel_content_type_unref (type);
			stream = camel_stream_mem_new ();
			part = camel_mime_part_new ();

			camel_data_wrapper_construct_from_stream (delegate_mail_data, stream);
			camel_object_unref (stream);
			camel_medium_set_content_object (CAMEL_MEDIUM (part), delegate_mail_data);
			camel_object_unref (delegate_mail_data);
			camel_multipart_add_part (body, part);
			camel_object_unref (part);

			/* Finish creating the message */
			camel_medium_set_content_object (CAMEL_MEDIUM (delegate_mail), CAMEL_DATA_WRAPPER (body));
			camel_object_unref (body);

			delegate_mail_subject = g_strdup_printf (_("You have been designated "
						"as a delegate for %s"), exchange_account_get_username (account));
			camel_mime_message_set_subject (delegate_mail, delegate_mail_subject);
			g_free (delegate_mail_subject);

			addr = camel_internet_address_new ();
			camel_address_decode (CAMEL_ADDRESS (addr), self_address);
			camel_mime_message_set_from (delegate_mail, addr);
			g_free (self_address);
			camel_object_unref (addr);

			delegate_exchange_dn = e2k_entryid_to_dn (user->entryid);
			recipient_address = email_look_up (delegate_exchange_dn,account);

			if (recipient_address) {
				addr = camel_internet_address_new ();
				camel_address_decode (CAMEL_ADDRESS (addr), recipient_address);
				camel_mime_message_set_recipients (delegate_mail, CAMEL_RECIPIENT_TYPE_TO, addr);
				camel_object_unref (addr);
			}

			eaccount = exchange_account_fetch (account);
			if (eaccount) {
				camel_medium_set_header (CAMEL_MEDIUM (delegate_mail),
							 "X-Evolution-Account", eaccount->uid);
				camel_medium_set_header (CAMEL_MEDIUM (delegate_mail),
							 "X-Evolution-Transport", eaccount->transport->url);
				camel_medium_set_header (CAMEL_MEDIUM (delegate_mail),
							 "X-Evolution-Fcc", eaccount->sent_folder_uri);
			}

			/* Send the permissions summarizing mail */
			out_folder = mail_component_get_folder (NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
			info = camel_message_info_new (NULL);
			camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
			mail_append_mail (out_folder, delegate_mail, info, em_utils_delegates_done, NULL);

		}

	}

	check = glade_xml_get_widget (xml, "see_private_checkbox");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)) !=
	    user->see_private) {
		user->see_private = !user->see_private;
		modified = TRUE;
	}

	g_object_unref (xml);
	gtk_widget_destroy (dialog);

	if (modified)
		g_signal_emit (user, signals[EDITED], 0);

	return TRUE;
}

/**
 * exchange_delegates_user_new:
 * @display_name: the delegate's (UTF8) display name
 *
 * Return value: a new delegate user with default permissions (but
 * with most of the internal data blank).
 **/
ExchangeDelegatesUser *
exchange_delegates_user_new (const gchar *display_name)
{
	ExchangeDelegatesUser *user;
	gint i;

	user = g_object_new (EXCHANGE_TYPE_DELEGATES_USER, NULL);
	user->display_name = g_strdup (display_name);

	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		if (i == EXCHANGE_DELEGATES_CALENDAR ||
		    i == EXCHANGE_DELEGATES_TASKS)
			user->role[i] = E2K_PERMISSIONS_ROLE_EDITOR;
		else
			user->role[i] = E2K_PERMISSIONS_ROLE_NONE;
	}

	return user;
}

/**
 * exchange_delegates_user_new_from_gc:
 * @gc: the global catalog object
 * @email: email address of the new delegate
 * @creator_entryid: The value of the PR_CREATOR_ENTRYID property
 * on the LocalFreebusy file.
 *
 * Return value: a new delegate user with default permissions and
 * internal data filled in from the global catalog.
 **/
ExchangeDelegatesUser *
exchange_delegates_user_new_from_gc (E2kGlobalCatalog *gc,
				    const gchar *email,
				    GByteArray *creator_entryid)
{
	E2kGlobalCatalogStatus status;
	E2kGlobalCatalogEntry *entry;
	ExchangeDelegatesUser *user;
	guint8 *p;

	status = e2k_global_catalog_lookup (
		gc, NULL, /* FIXME: cancellable */
		E2K_GLOBAL_CATALOG_LOOKUP_BY_EMAIL, email,
		(E2K_GLOBAL_CATALOG_LOOKUP_SID |
		 E2K_GLOBAL_CATALOG_LOOKUP_LEGACY_EXCHANGE_DN),
		&entry);
	if (status != E2K_GLOBAL_CATALOG_OK)
		return NULL;

	user = exchange_delegates_user_new (e2k_sid_get_display_name (entry->sid));
	user->dn = g_strdup (entry->dn);
	user->sid = entry->sid;
	g_object_ref (user->sid);

	user->entryid = g_byte_array_new ();
	p = creator_entryid->data + creator_entryid->len - 2;
	while (p > creator_entryid->data && *p)
		p--;
	g_byte_array_append (user->entryid, creator_entryid->data,
			     p - creator_entryid->data + 1);
	g_byte_array_append (user->entryid, (guint8*)entry->legacy_exchange_dn,
			     strlen (entry->legacy_exchange_dn));
	g_byte_array_append (user->entryid, (guint8*)"", 1);

	return user;
}
