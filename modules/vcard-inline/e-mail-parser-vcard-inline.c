/*
 * e-mail-parser-vcard-inline.c
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "e-mail-parser-vcard-inline.h"
#include "e-mail-part-vcard-inline.h"

#include <camel/camel.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-utils.h>
#include <em-format/e-mail-formatter-utils.h>

#include <libebook/libebook.h>
#include <libedataserver/libedataserver.h>

#include <shell/e-shell.h>
#include <addressbook/gui/merging/eab-contact-merging.h>
#include <addressbook/util/eab-book-util.h>

#include <libebackend/libebackend.h>

#define d(x)

typedef EMailParserExtension EMailParserVCardInline;
typedef EMailParserExtensionClass EMailParserVCardInlineClass;

typedef EExtension EMailParserVCardInlineLoader;
typedef EExtensionClass EMailParserVCardInlineLoaderClass;

GType e_mail_parser_vcard_inline_get_type (void);
GType e_mail_parser_vcard_inline_loader_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailParserVCardInline,
	e_mail_parser_vcard_inline,
	E_TYPE_MAIL_PARSER_EXTENSION)

G_DEFINE_DYNAMIC_TYPE (
	EMailParserVCardInlineLoader,
	e_mail_parser_vcard_inline_loader,
	E_TYPE_EXTENSION)

static const gchar *parser_mime_types[] = {
	"text/vcard",
	"text/x-vcard",
	"text/directory",
	NULL
};

static void
mail_part_vcard_inline_free (EMailPart *mail_part)
{
	EMailPartVCardInline *vi_part = (EMailPartVCardInline *) mail_part;

	g_clear_object (&vi_part->contact_display);
	g_clear_object (&vi_part->message_label);
	g_clear_object (&vi_part->formatter);
	g_clear_object (&vi_part->iframe);
	g_clear_object (&vi_part->save_button);
	g_clear_object (&vi_part->toggle_button);
	g_clear_object (&vi_part->folder);

	if (vi_part->message_uid) {
		g_free (vi_part->message_uid);
		vi_part->message_uid = NULL;
	}
}

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
		g_warning (
			"%s: Failed to open book client: %s",
			G_STRFUNC, error->message);
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
save_vcard_cb (WebKitDOMEventTarget *button,
               WebKitDOMEvent *event,
               EMailPartVCardInline *vcard_part)
{
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	GSList *contact_list;
	const gchar *extension_name;
	GtkWidget *dialog;

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
		source, NULL, client_connect_cb, contact_list);
}

static void
display_mode_toggle_cb (WebKitDOMEventTarget *button,
                        WebKitDOMEvent *event,
                        EMailPartVCardInline *vcard_part)
{
	EABContactDisplayMode mode;
	gchar *uri;
	gchar *html_label, *access_key;

	mode = eab_contact_formatter_get_display_mode (vcard_part->formatter);
	if (mode == EAB_CONTACT_DISPLAY_RENDER_NORMAL) {
		mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;

		html_label = e_mail_formatter_parse_html_mnemonics (
				_("Show F_ull vCard"), &access_key);

		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (button),
			html_label, NULL);
		if (access_key) {
			webkit_dom_html_element_set_access_key (
				WEBKIT_DOM_HTML_ELEMENT (button),
				access_key);
			g_free (access_key);
		}

		g_free (html_label);

	} else {
		mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;

		html_label = e_mail_formatter_parse_html_mnemonics (
				_("Show Com_pact vCard"), &access_key);

		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (button),
			html_label, NULL);
		if (access_key) {
			webkit_dom_html_element_set_access_key (
				WEBKIT_DOM_HTML_ELEMENT (button),
				access_key);
			g_free (access_key);
		}

		g_free (html_label);
	}

	eab_contact_formatter_set_display_mode (vcard_part->formatter, mode);

	uri = e_mail_part_build_uri (
		vcard_part->folder, vcard_part->message_uid,
		"part_id", G_TYPE_STRING, vcard_part->parent.id,
		"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW, NULL);

	webkit_dom_html_iframe_element_set_src (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (vcard_part->iframe), uri);

	g_free (uri);
}

static void
bind_dom (EMailPartVCardInline *vcard_part,
          WebKitDOMElement *attachment)
{
	WebKitDOMNodeList *list;
	WebKitDOMElement *iframe, *toggle_button, *save_button;

        /* IFRAME */
	list = webkit_dom_element_get_elements_by_tag_name (attachment, "iframe");
	if (webkit_dom_node_list_get_length (list) != 1)
		return;
	iframe = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (list, 0));
	if (vcard_part->iframe)
		g_object_unref (vcard_part->iframe);
	vcard_part->iframe = g_object_ref (iframe);

	/* TOGGLE DISPLAY MODE BUTTON */
	list = webkit_dom_element_get_elements_by_class_name (
		attachment, "org-gnome-vcard-inline-display-mode-button");
	if (webkit_dom_node_list_get_length (list) != 1)
		return;
	toggle_button = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (list, 0));
	if (vcard_part->toggle_button)
		g_object_unref (vcard_part->toggle_button);
	vcard_part->toggle_button = g_object_ref (toggle_button);

	/* SAVE TO ADDRESSBOOK BUTTON */
	list = webkit_dom_element_get_elements_by_class_name (
		attachment, "org-gnome-vcard-inline-save-button");
	if (webkit_dom_node_list_get_length (list) != 1)
		return;
	save_button = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (list, 0));
	if (vcard_part->save_button)
		g_object_unref (vcard_part->save_button);
	vcard_part->save_button = g_object_ref (save_button);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (toggle_button),
		"click", G_CALLBACK (display_mode_toggle_cb),
		FALSE, vcard_part);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (save_button),
		"click", G_CALLBACK (save_vcard_cb),
		FALSE, vcard_part);

	/* Bind collapse buttons for contact lists. */
	eab_contact_formatter_bind_dom (
		webkit_dom_html_iframe_element_get_content_document (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe)));
}

static void
decode_vcard (EMailPartVCardInline *vcard_part,
              CamelMimePart *mime_part)
{
	CamelDataWrapper *data_wrapper;
	CamelMedium *medium;
	CamelStream *stream;
	GSList *contact_list;
	GByteArray *array;
	const gchar *string;
	const guint8 padding[2] = {0};

	array = g_byte_array_new ();
	medium = CAMEL_MEDIUM (mime_part);

	/* Stream takes ownership of the byte array. */
	stream = camel_stream_mem_new_with_byte_array (array);
	data_wrapper = camel_medium_get_content (medium);
	camel_data_wrapper_decode_to_stream_sync (
		data_wrapper, stream, NULL, NULL);

	/* because the result is not NULL-terminated */
	g_byte_array_append (array, padding, 2);

	string = (gchar *) array->data;
	contact_list = eab_contact_list_from_string (string);
	vcard_part->contact_list = contact_list;

	g_object_unref (mime_part);
	g_object_unref (stream);
}

static gboolean
empe_vcard_inline_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable,
                         GQueue *out_mail_parts)
{
	EMailPartVCardInline *vcard_part;
	GQueue work_queue = G_QUEUE_INIT;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".org-gnome-vcard-inline-display");

	vcard_part = (EMailPartVCardInline *) e_mail_part_subclass_new (
		part, part_id->str, sizeof (EMailPartVCardInline),
		(GFreeFunc) mail_part_vcard_inline_free);
	vcard_part->parent.mime_type = camel_content_type_simple (
		camel_mime_part_get_content_type (part));
	vcard_part->parent.bind_func = (EMailPartDOMBindFunc) bind_dom;
	vcard_part->parent.is_attachment = TRUE;
	vcard_part->formatter = g_object_new (
		EAB_TYPE_CONTACT_FORMATTER,
		"display-mode", EAB_CONTACT_DISPLAY_RENDER_COMPACT,
		"render-maps", FALSE, NULL);
	g_object_ref (part);

	decode_vcard (vcard_part, part);

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, vcard_part);

	e_mail_parser_wrap_as_attachment (
		parser, part, part_id, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static void
e_mail_parser_vcard_inline_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->flags = E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION;
	class->parse = empe_vcard_inline_parse;
}

static void
e_mail_parser_vcard_inline_class_finalize (EMailParserExtensionClass *class)
{
}

static void
e_mail_parser_vcard_inline_init (EMailParserExtension *extension)
{
}

static void
mail_parser_vcard_inline_loader_constructed (GObject *object)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	e_mail_extension_registry_add_extension (
		E_MAIL_EXTENSION_REGISTRY (extensible),
		parser_mime_types,
		e_mail_parser_vcard_inline_get_type ());
}

static void
e_mail_parser_vcard_inline_loader_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_parser_vcard_inline_loader_constructed;

	class->extensible_type = E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY;
}

static void
e_mail_parser_vcard_inline_loader_class_finalize (EExtensionClass *class)
{
}

static void
e_mail_parser_vcard_inline_loader_init (EExtension *extension)
{
}

void
e_mail_parser_vcard_inline_type_register (GTypeModule *type_module)
{
	e_mail_parser_vcard_inline_register_type (type_module);
	e_mail_parser_vcard_inline_loader_register_type (type_module);
}

