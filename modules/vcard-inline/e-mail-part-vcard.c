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

#define E_MAIL_PART_VCARD_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PART_VCARD, EMailPartVCardPrivate))

struct _EMailPartVCardPrivate {
	gint placeholder;

	guint display_mode_toggled_signal_id;
	guint save_vcard_button_pressed_signal_id;

	GDBusProxy *web_extension;
	guint64 page_id;
};

G_DEFINE_DYNAMIC_TYPE (
	EMailPartVCard,
	e_mail_part_vcard,
	E_TYPE_MAIL_PART)

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
			registry, book_client, contact, NULL, NULL);
	}

	g_object_unref (client);

 exit:
	g_slist_free_full (contact_list, (GDestroyNotify) g_object_unref);
}

static void
save_vcard_cb (GDBusConnection *connection,
               const gchar *sender_name,
               const gchar *object_path,
               const gchar *interface_name,
               const gchar *signal_name,
               GVariant *parameters,
               EMailPartVCard *vcard_part)
{
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	GSList *contact_list;
	const gchar *extension_name, *button_value, *part_id;
	GtkWidget *dialog;

	if (g_strcmp0 (signal_name, "VCardInlineSaveButtonPressed") != 0)
		return;

	g_variant_get (parameters, "(&s)", &button_value);

	part_id = e_mail_part_get_id (E_MAIL_PART (vcard_part));

	if (!strstr (part_id, button_value))
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
		vcard_part->contact_list,
		(GCopyFunc) g_object_ref, NULL);

	e_book_client_connect (
		source, 30, NULL, client_connect_cb, contact_list);
}

static void
display_mode_toggle_cb (GDBusConnection *connection,
                        const gchar *sender_name,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *signal_name,
                        GVariant *parameters,
                        EMailPartVCard *vcard_part)
{
	EABContactDisplayMode mode;
	gchar *uri;
	gchar *html_label;
	gchar *access_key;
	const gchar *part_id;
	const gchar *button_id;

	if (g_strcmp0 (signal_name, "VCardInlineDisplayModeToggled") != 0)
		return;

	if (!vcard_part->priv->web_extension)
		return;

	g_variant_get (parameters, "(&s)", &button_id);

	part_id = e_mail_part_get_id (E_MAIL_PART (vcard_part));

	if (!strstr (part_id, button_id))
		return;

	mode = eab_contact_formatter_get_display_mode (vcard_part->formatter);
	if (mode == EAB_CONTACT_DISPLAY_RENDER_NORMAL) {
		mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;

		html_label = e_mail_formatter_parse_html_mnemonics (
				_("Show F_ull vCard"), &access_key);
	} else {
		mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;

		html_label = e_mail_formatter_parse_html_mnemonics (
				_("Show Com_pact vCard"), &access_key);
	}

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		vcard_part->priv->web_extension,
		"VCardInlineUpdateButton",
		g_variant_new (
			"(tsss)",
			vcard_part->priv->page_id,
			button_id,
			html_label,
			access_key),
		NULL);

	if (access_key)
		g_free (access_key);

	g_free (html_label);

	eab_contact_formatter_set_display_mode (vcard_part->formatter, mode);

	uri = e_mail_part_build_uri (
		vcard_part->folder, vcard_part->message_uid,
		"part_id", G_TYPE_STRING, part_id,
		"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW, NULL);

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		vcard_part->priv->web_extension,
		"VCardInlineSetIFrameSrc",
		g_variant_new (
			"(tss)",
			vcard_part->priv->page_id,
			button_id,
			uri),
		NULL);

	g_free (uri);
}

static void
mail_part_vcard_dispose (GObject *object)
{
	EMailPartVCard *part = E_MAIL_PART_VCARD (object);

	g_clear_object (&part->contact_display);
	g_clear_object (&part->message_label);
	g_clear_object (&part->formatter);
	g_clear_object (&part->folder);

	if (part->priv->display_mode_toggled_signal_id > 0) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (part->priv->web_extension),
			part->priv->display_mode_toggled_signal_id);
		part->priv->display_mode_toggled_signal_id = 0;
	}

	if (part->priv->save_vcard_button_pressed_signal_id > 0) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (part->priv->web_extension),
			part->priv->save_vcard_button_pressed_signal_id);
		part->priv->save_vcard_button_pressed_signal_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_part_vcard_parent_class)->dispose (object);
}

static void
mail_part_vcard_finalize (GObject *object)
{
	EMailPartVCard *part = E_MAIL_PART_VCARD (object);

	g_free (part->message_uid);

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
mail_part_vcard_bind_dom_element (EMailPart *part,
                                  GDBusProxy *evolution_web_extension,
                                  guint64 page_id,
                                  const gchar *element_id)
{
	EMailPartVCard *vcard_part;

	vcard_part = E_MAIL_PART_VCARD (part);

	vcard_part->priv->web_extension = evolution_web_extension;
	vcard_part->priv->page_id = page_id;

	vcard_part->priv->display_mode_toggled_signal_id =
		g_dbus_connection_signal_subscribe (
			g_dbus_proxy_get_connection (evolution_web_extension),
			g_dbus_proxy_get_name (evolution_web_extension),
			g_dbus_proxy_get_interface_name (evolution_web_extension),
			"VCardInlineDisplayModeToggled",
			g_dbus_proxy_get_object_path (evolution_web_extension),
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			(GDBusSignalCallback) display_mode_toggle_cb,
			vcard_part,
			NULL);

	vcard_part->priv->save_vcard_button_pressed_signal_id =
		g_dbus_connection_signal_subscribe (
			g_dbus_proxy_get_connection (evolution_web_extension),
			g_dbus_proxy_get_name (evolution_web_extension),
			g_dbus_proxy_get_interface_name (evolution_web_extension),
			"VCardInlineSaveButtonPressed",
			g_dbus_proxy_get_object_path (evolution_web_extension),
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			(GDBusSignalCallback) save_vcard_cb,
			vcard_part,
			NULL);

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		vcard_part->priv->web_extension,
		"VCardInlineBindDOM",
		g_variant_new (
			"(ts)",
			vcard_part->priv->page_id,
			element_id),
		NULL);
}

static void
e_mail_part_vcard_class_init (EMailPartVCardClass *class)
{
	GObjectClass *object_class;
	EMailPartClass *mail_part_class;

	g_type_class_add_private (class, sizeof (EMailPartVCardPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_part_vcard_dispose;
	object_class->finalize = mail_part_vcard_finalize;
	object_class->constructed = mail_part_vcard_constructed;

	mail_part_class = E_MAIL_PART_CLASS (class);
	mail_part_class->bind_dom_element = mail_part_vcard_bind_dom_element;
}

static void
e_mail_part_vcard_class_finalize (EMailPartVCardClass *class)
{
}

static void
e_mail_part_vcard_init (EMailPartVCard *part)
{
	part->priv = E_MAIL_PART_VCARD_GET_PRIVATE (part);
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

