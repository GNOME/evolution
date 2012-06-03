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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <libebook/libebook.h>
#include <libedataserverui/libedataserverui.h>

#include <shell/e-shell.h>
#include <addressbook/gui/merging/eab-contact-merging.h>
#include <addressbook/gui/widgets/eab-contact-display.h>
#include <addressbook/gui/widgets/eab-contact-formatter.h>
#include <addressbook/util/eab-book-util.h>
#include <mail/em-format-hook.h>
#include <mail/em-format-html.h>

#define d(x)

typedef struct _VCardInlinePURI VCardInlinePURI;

struct _VCardInlinePURI {
	EMFormatPURI puri;

	GSList *contact_list;
	GtkWidget *contact_display;
	GtkWidget *message_label;

	EABContactFormatter *formatter;
	WebKitDOMElement *iframe;
	WebKitDOMElement *toggle_button;
	WebKitDOMElement *save_button;
};

/* Forward Declarations */
void org_gnome_vcard_inline_format (gpointer ep, EMFormatHookTarget *target);
gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

static void
org_gnome_vcard_inline_pobject_free (EMFormatPURI *object)
{
	VCardInlinePURI *vcard_object;

	vcard_object = (VCardInlinePURI *) object;

	e_client_util_free_object_slist (vcard_object->contact_list);
	vcard_object->contact_list = NULL;

	if (vcard_object->contact_display != NULL) {
		g_object_unref (vcard_object->contact_display);
		vcard_object->contact_display = NULL;
	}

	if (vcard_object->message_label != NULL) {
		g_object_unref (vcard_object->message_label);
		vcard_object->message_label = NULL;
	}

	if (vcard_object->formatter != NULL) {
		g_object_unref (vcard_object->formatter);
		vcard_object->formatter = NULL;
	}

	if (vcard_object->iframe != NULL) {
		g_object_unref (vcard_object->iframe);
		vcard_object->iframe = NULL;
	}

	if (vcard_object->toggle_button != NULL) {
		g_object_unref (vcard_object->toggle_button);
		vcard_object->toggle_button = NULL;
	}

	if (vcard_object->save_button != NULL) {
		g_object_unref (vcard_object->save_button);
		vcard_object->save_button = NULL;
	}
}

static void
org_gnome_vcard_inline_decode (VCardInlinePURI *vcard_object,
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
	vcard_object->contact_list = contact_list;

	g_object_unref (mime_part);
	g_object_unref (stream);
}

static void
org_gnome_vcard_inline_client_loaded_cb (ESource *source,
                                         GAsyncResult *result,
                                         GSList *contact_list)
{
	EShell *shell;
	EClient *client = NULL;
	EBookClient *book_client;
	ESourceRegistry *registry;
	GSList *iter;
	GError *error = NULL;

	e_client_utils_open_new_finish (source, result, &client, &error);

	if (error != NULL) {
		g_warn_if_fail (client == NULL);
		g_warning (
			"%s: Failed to open book client: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	g_return_if_fail (E_IS_BOOK_CLIENT (client));

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
	e_client_util_free_object_slist (contact_list);
}

static void
org_gnome_vcard_inline_save_cb (WebKitDOMEventTarget *button,
                                WebKitDOMEvent *event,
                                VCardInlinePURI *vcard_object)
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

	contact_list = e_client_util_copy_object_slist (NULL, vcard_object->contact_list);

	e_client_utils_open_new (
		source, E_CLIENT_SOURCE_TYPE_CONTACTS,
		FALSE, NULL, (GAsyncReadyCallback)
		org_gnome_vcard_inline_client_loaded_cb,
		contact_list);
}

static void
org_gnome_vcard_inline_toggle_cb (WebKitDOMEventTarget *button,
                                  WebKitDOMEvent *event,
                                  EMFormatPURI *puri)
{
	VCardInlinePURI *vcard_object;
	EABContactDisplayMode mode;
	gchar *uri;

	vcard_object = (VCardInlinePURI *) puri;

	mode = eab_contact_formatter_get_display_mode (vcard_object->formatter);
	if (mode == EAB_CONTACT_DISPLAY_RENDER_NORMAL) {
		mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;

		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (button),
			_("Show Full vCard"), NULL);

	} else {
		mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;

		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (button),
			_("Show Compact vCard"), NULL);
	}

	eab_contact_formatter_set_display_mode (vcard_object->formatter, mode);

	uri = em_format_build_mail_uri (
		puri->emf->folder, puri->emf->message_uid,
		"part_id", G_TYPE_STRING, puri->uri,
		"mode", G_TYPE_INT, EM_FORMAT_WRITE_MODE_RAW, NULL);

	webkit_dom_html_iframe_element_set_src (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (vcard_object->iframe), uri);

	g_free (uri);
}

static void
org_gnome_vcard_inline_bind_dom (WebKitDOMElement *attachment,
                                 EMFormatPURI *puri)
{
	WebKitDOMNodeList *list;
	WebKitDOMElement *iframe, *toggle_button, *save_button;
	VCardInlinePURI *vcard_object;

	vcard_object = (VCardInlinePURI *) puri;

        /* IFRAME */
	list = webkit_dom_element_get_elements_by_tag_name (attachment, "iframe");
	if (webkit_dom_node_list_get_length (list) != 1)
		return;
	iframe = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (list, 0));
	if (vcard_object->iframe)
		g_object_unref (vcard_object->iframe);
	vcard_object->iframe = g_object_ref (iframe);

	/* TOGGLE DISPLAY MODE BUTTON */
	list = webkit_dom_element_get_elements_by_class_name (
		attachment, "org-gnome-vcard-inline-display-mode-button");
	if (webkit_dom_node_list_get_length (list) != 1)
		return;
	toggle_button = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (list, 0));
	if (vcard_object->toggle_button)
		g_object_unref (vcard_object->toggle_button);
	vcard_object->toggle_button = g_object_ref (toggle_button);

	/* SAVE TO ADDRESSBOOK BUTTON */
	list = webkit_dom_element_get_elements_by_class_name (
		attachment, "org-gnome-vcard-inline-save-button");
	if (webkit_dom_node_list_get_length (list) != 1)
		return;
	save_button = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (list, 0));
	if (vcard_object->save_button)
		g_object_unref (vcard_object->save_button);
	vcard_object->save_button = g_object_ref (save_button);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (toggle_button),
		"click", G_CALLBACK (org_gnome_vcard_inline_toggle_cb),
		FALSE, puri);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (save_button),
		"click", G_CALLBACK (org_gnome_vcard_inline_save_cb),
		FALSE, puri);

	/* Bind collapse buttons for contact lists. */
	eab_contact_formatter_bind_dom (
		webkit_dom_html_iframe_element_get_content_document (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe)));
}

static void
org_gnome_vcard_inline_write (EMFormat *emf,
                              EMFormatPURI *puri,
                              CamelStream *stream,
                              EMFormatWriterInfo *info,
                              GCancellable *cancellable)
{
	VCardInlinePURI *vpuri;

	vpuri = (VCardInlinePURI *) puri;

	if (info->mode == EM_FORMAT_WRITE_MODE_RAW)  {

		EContact *contact;

		if (vpuri->contact_list != NULL)
			contact = E_CONTACT (vpuri->contact_list->data);
		else
			contact = NULL;

		eab_contact_formatter_format_contact_sync (
			vpuri->formatter, contact, stream, cancellable);

	} else {
		gchar *str, *uri;
		gint length;
		const gchar *label = NULL;
		EABContactDisplayMode mode;
		const gchar *info = NULL;

		length = g_slist_length (vpuri->contact_list);
		if (length < 1)
			return;

		uri = em_format_build_mail_uri (
			emf->folder, emf->message_uid,
			"part_id", G_TYPE_STRING, puri->uri,
			"mode", G_TYPE_INT, EM_FORMAT_WRITE_MODE_RAW, NULL);

		mode = eab_contact_formatter_get_display_mode (vpuri->formatter);
		if (mode == EAB_CONTACT_DISPLAY_RENDER_COMPACT) {
			mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;
			label =_("Show Full vCard");
		} else {
			mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;
			label = _("Show Compact vCard");
		}

		str = g_strdup_printf (
			"<div id=\"%s\">"
			"<button type=\"button\" "
				"name=\"set-display-mode\" "
				"class=\"org-gnome-vcard-inline-display-mode-button\" "
				"value=\"%d\">%s</button>"
			"<button type=\"button\" "
				"name=\"save-to-addressbook\" "
				"class=\"org-gnome-vcard-inline-save-button\" "
				"value=\"%s\">%s</button><br/>"
			"<iframe width=\"100%%\" height=\"auto\" frameborder=\"0\""
				"src=\"%s\" name=\"%s\"></iframe>"
			"</div>",
			 puri->uri,
			 mode, label,
			 puri->uri, _("Save To Addressbook"),
			 uri, puri->uri);

		camel_stream_write_string (stream, str, cancellable, NULL);

		g_free (str);

		if (length == 2) {

			info = _("There is one other contact.");

		} else if (length > 2) {

			/* Translators: This will always be two or more. */
			info = g_strdup_printf (ngettext (
				"There is %d other contact.",
				"There are %d other contacts.",
				length - 1), length - 1);
		}

		if (info) {

			str = g_strdup_printf (
				"<div class=\"attachment-info\">%s</div>",
				info);

			camel_stream_write_string (stream, str, cancellable, NULL);

			g_free (str);
		}

		g_free (uri);
	}
}

void
org_gnome_vcard_inline_format (gpointer ep,
                               EMFormatHookTarget *target)
{
	VCardInlinePURI *vcard_object;
	gint len;

	len = target->part_id->len;
	g_string_append (target->part_id, ".org-gnome-vcard-inline-display");

	vcard_object = (VCardInlinePURI *) em_format_puri_new (
			target->format, sizeof (VCardInlinePURI),
			target->part, target->part_id->str);
	vcard_object->puri.mime_type = g_strdup("text/html");
	vcard_object->puri.write_func = org_gnome_vcard_inline_write;
	vcard_object->puri.bind_func = org_gnome_vcard_inline_bind_dom;
	vcard_object->puri.free = org_gnome_vcard_inline_pobject_free;
	vcard_object->puri.is_attachment = true;
	vcard_object->formatter
		= g_object_new (
			EAB_TYPE_CONTACT_FORMATTER,
			"display-mode", EAB_CONTACT_DISPLAY_RENDER_COMPACT,
			"render-maps", FALSE, NULL);

	em_format_add_puri (target->format, (EMFormatPURI *) vcard_object);

	g_object_ref (target->part);

	org_gnome_vcard_inline_decode (vcard_object, target->part);

	g_string_truncate (target->part_id, len);
}
