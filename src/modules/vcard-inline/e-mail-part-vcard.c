/*
 * e-mail-part-vcard.c
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
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include <em-format/e-mail-formatter-utils.h>
#include <em-format/e-mail-part-utils.h>

#include <shell/e-shell.h>
#include <addressbook/gui/widgets/eab-contact-merging.h>

#include "e-mail-part-vcard.h"

struct _EMailPartVCardPrivate {
	GSList *contacts;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailPartVCard, e_mail_part_vcard, E_TYPE_MAIL_PART, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailPartVCard))

static void
client_connect_cb (GObject *source_object,
                   GAsyncResult *result,
                   gpointer user_data)
{
	GSList *contact_list = user_data;
	EShell *shell;
	EClient *client;
	EBookClient *book_client;
	ESourceRegistry *registry;
	GSList *iter;
	GError *error = NULL;

	client = e_book_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	book_client = E_BOOK_CLIENT (client);

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	for (iter = contact_list; iter != NULL; iter = iter->next) {
		EContact *contact;

		contact = E_CONTACT (iter->data);
		eab_merging_book_add_contact (
			registry, book_client, contact, NULL, NULL, FALSE);
	}

	g_object_unref (client);

 exit:
	g_slist_free_full (contact_list, (GDestroyNotify) g_object_unref);
}

static void
mail_part_vcard_save_clicked_cb (EWebView *web_view,
				 const gchar *iframe_id,
				 const gchar *element_id,
				 const gchar *element_class,
				 const gchar *element_value,
				 const GtkAllocation *element_position,
				 gpointer user_data)
{
	EMailPartVCard *vcard_part = user_data;
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	GSList *contact_list;
	const gchar *extension_name, *part_id;
	GtkWidget *dialog;

	g_return_if_fail (E_IS_MAIL_PART_VCARD (vcard_part));

	part_id = e_mail_part_get_id (E_MAIL_PART (vcard_part));

	if (!strstr (part_id, element_value))
		return;

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;

	dialog = e_source_selector_dialog_new (NULL, registry, extension_name);

	selector = e_source_selector_dialog_get_selector (
		E_SOURCE_SELECTOR_DIALOG (dialog));

	source = e_source_registry_ref_default_address_book (registry);
	e_source_selector_set_primary_selection (selector, source);
	g_object_unref (source);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	source = e_source_selector_dialog_peek_primary_selection (
		E_SOURCE_SELECTOR_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	g_return_if_fail (source != NULL);

	contact_list = g_slist_copy_deep (
		vcard_part->priv->contacts,
		(GCopyFunc) g_object_ref, NULL);

	e_book_client_connect (source, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, NULL, client_connect_cb, contact_list);
}

static void
mail_part_vcard_finalize (GObject *object)
{
	EMailPartVCard *part = E_MAIL_PART_VCARD (object);

	g_slist_free_full (part->priv->contacts, g_object_unref);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_vcard_parent_class)->finalize (object);
}

static void
mail_part_vcard_constructed (GObject *object)
{
	EMailPart *part;
	CamelMimePart *mime_part;
	CamelContentType *content_type;
	gchar *mime_type;

	part = E_MAIL_PART (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_part_vcard_parent_class)->constructed (object);

	e_mail_part_set_is_attachment (part, TRUE);

	mime_part = e_mail_part_ref_mime_part (part);

	content_type = camel_mime_part_get_content_type (mime_part);
	mime_type = camel_content_type_simple (content_type);
	e_mail_part_set_mime_type (part, mime_type);
	g_free (mime_type);

	g_object_unref (mime_part);
}

static void
mail_part_vcard_content_loaded (EMailPart *part,
				EWebView *web_view,
				const gchar *iframe_id)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (E_IS_MAIL_PART_VCARD (part));

	e_web_view_register_element_clicked (web_view, "org-gnome-vcard-save-button",
		mail_part_vcard_save_clicked_cb, part);
}

static void
e_mail_part_vcard_class_init (EMailPartVCardClass *class)
{
	GObjectClass *object_class;
	EMailPartClass *mail_part_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = mail_part_vcard_finalize;
	object_class->constructed = mail_part_vcard_constructed;

	mail_part_class = E_MAIL_PART_CLASS (class);
	mail_part_class->content_loaded = mail_part_vcard_content_loaded;
}

static void
e_mail_part_vcard_class_finalize (EMailPartVCardClass *class)
{
}

static void
e_mail_part_vcard_init (EMailPartVCard *part)
{
	part->priv = e_mail_part_vcard_get_instance_private (part);
}

void
e_mail_part_vcard_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_part_vcard_register_type (type_module);
}

EMailPartVCard *
e_mail_part_vcard_new (CamelMimePart *mime_part,
                       const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_VCARD,
		"id", id, "mime-part", mime_part, NULL);
}

void
e_mail_part_vcard_take_contacts (EMailPartVCard *vcard_part,
				 GSList *contacts)
{
	g_return_if_fail (E_IS_MAIL_PART_VCARD (vcard_part));

	g_slist_free_full (vcard_part->priv->contacts, g_object_unref);
	vcard_part->priv->contacts = contacts;
}

const GSList *
e_mail_part_vcard_get_contacts (EMailPartVCard *vcard_part)
{
	g_return_val_if_fail (E_IS_MAIL_PART_VCARD (vcard_part), NULL);

	return vcard_part->priv->contacts;
}
