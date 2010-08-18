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
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <libedataserverui/e-book-auth-util.h>

#include "addressbook/gui/merging/eab-contact-merging.h"
#include "addressbook/gui/widgets/eab-contact-display.h"
#include "addressbook/util/eab-book-util.h"
#include "mail/em-format-hook.h"
#include "mail/em-format-html.h"

#define d(x)

typedef struct _VCardInlinePObject VCardInlinePObject;

struct _VCardInlinePObject {
	EMFormatHTMLPObject object;

	GList *contact_list;
	ESourceList *source_list;
	GtkWidget *contact_display;
	GtkWidget *message_label;
};

static gint org_gnome_vcard_inline_classid;

/* Forward Declarations */
void org_gnome_vcard_inline_format (gpointer ep, EMFormatHookTarget *target);
gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

static void
org_gnome_vcard_inline_pobject_free (EMFormatHTMLPObject *object)
{
	VCardInlinePObject *vcard_object;

	vcard_object = (VCardInlinePObject *) object;

	g_list_foreach (
		vcard_object->contact_list,
		(GFunc) g_object_unref, NULL);
	g_list_free (vcard_object->contact_list);
	vcard_object->contact_list = NULL;

	if (vcard_object->source_list != NULL) {
		g_object_unref (vcard_object->source_list);
		vcard_object->source_list = NULL;
	}

	if (vcard_object->contact_display != NULL) {
		g_object_unref (vcard_object->contact_display);
		vcard_object->contact_display = NULL;
	}

	if (vcard_object->message_label != NULL) {
		g_object_unref (vcard_object->message_label);
		vcard_object->message_label = NULL;
	}
}

static void
org_gnome_vcard_inline_decode (VCardInlinePObject *vcard_object,
                               CamelMimePart *mime_part)
{
	CamelDataWrapper *data_wrapper;
	CamelMedium *medium;
	CamelStream *stream;
	GList *contact_list;
	GByteArray *array;
	const gchar *string;
	const guint8 padding[2] = {0};

	array = g_byte_array_new ();
	medium = CAMEL_MEDIUM (mime_part);

	/* Stream takes ownership of the byte array. */
	stream = camel_stream_mem_new_with_byte_array (array);
	data_wrapper = camel_medium_get_content (medium);
	camel_data_wrapper_decode_to_stream (data_wrapper, stream, NULL);

	/* because the result is not NULL-terminated */
	g_byte_array_append (array, padding, 2);

	string = (gchar *) array->data;
	contact_list = eab_contact_list_from_string (string);
	vcard_object->contact_list = contact_list;

	g_object_unref (mime_part);
	g_object_unref (stream);
}

static void
org_gnome_vcard_inline_book_loaded_cb (ESource *source,
                                       GAsyncResult *result,
                                       GList *contact_list)
{
	EBook *book;
	GList *iter;

	book = e_load_book_source_finish (source, result, NULL);

	if (book == NULL)
		goto exit;

	for (iter = contact_list; iter != NULL; iter = iter->next) {
		EContact *contact;

		contact = E_CONTACT (iter->data);
		eab_merging_book_add_contact (book, contact, NULL, NULL);
	}

	g_object_unref (book);

exit:
	g_list_foreach (contact_list, (GFunc) g_object_unref, NULL);
	g_list_free (contact_list);
}

static void
org_gnome_vcard_inline_save_cb (VCardInlinePObject *vcard_object)
{
	ESource *source;
	GList *contact_list;

	g_return_if_fail (vcard_object->source_list != NULL);

	source = e_source_list_peek_default_source (vcard_object->source_list);
	g_return_if_fail (source != NULL);

	contact_list = g_list_copy (vcard_object->contact_list);
	g_list_foreach (contact_list, (GFunc) g_object_ref, NULL);

	e_load_book_source_async (
		source, NULL, NULL, (GAsyncReadyCallback)
		org_gnome_vcard_inline_book_loaded_cb, contact_list);
}

static void
org_gnome_vcard_inline_toggle_cb (VCardInlinePObject *vcard_object,
                                  GtkButton *button)
{
	EABContactDisplay *contact_display;
	EABContactDisplayMode mode;
	const gchar *label;

	contact_display = EAB_CONTACT_DISPLAY (vcard_object->contact_display);
	mode = eab_contact_display_get_mode (contact_display);

	/* Toggle between "full" and "compact" modes. */
	if (mode == EAB_CONTACT_DISPLAY_RENDER_NORMAL) {
		mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;
		label = _("Show Full vCard");
	} else {
		mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;
		label = _("Show Compact vCard");
	}

	eab_contact_display_set_mode (contact_display, mode);
	gtk_button_set_label (button, label);
}

static gboolean
org_gnome_vcard_inline_embed (EMFormatHTML *format,
                              GtkHTMLEmbedded *embedded,
                              EMFormatHTMLPObject *object)
{
	VCardInlinePObject *vcard_object;
	GtkWidget *button_box;
	GtkWidget *container;
	GtkWidget *widget;
	EContact *contact;
	guint length;

	vcard_object = (VCardInlinePObject *) object;
	length = g_list_length (vcard_object->contact_list);

	if (vcard_object->contact_list != NULL)
		contact = E_CONTACT (vcard_object->contact_list->data);
	else
		contact = NULL;

	container = GTK_WIDGET (embedded);

	widget = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	button_box = widget;

	widget = eab_contact_display_new ();
	eab_contact_display_set_contact (
		EAB_CONTACT_DISPLAY (widget), contact);
	eab_contact_display_set_mode (
		EAB_CONTACT_DISPLAY (widget),
		EAB_CONTACT_DISPLAY_RENDER_COMPACT);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	vcard_object->contact_display = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	vcard_object->message_label = g_object_ref (widget);

	if (length == 2) {
		const gchar *text;

		text = _("There is one other contact.");
		gtk_label_set_text (GTK_LABEL (widget), text);
		gtk_widget_show (widget);

	} else if (length > 2) {
		gchar *text;

		/* Translators: This will always be two or more. */
		text = g_strdup_printf (ngettext (
			"There is %d other contact.",
			"There are %d other contacts.",
			length - 1), length - 1);
		gtk_label_set_text (GTK_LABEL (widget), text);
		gtk_widget_show (widget);
		g_free (text);

	} else
		gtk_widget_hide (widget);

	container = button_box;

	widget = gtk_button_new_with_label (_("Show Full vCard"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (org_gnome_vcard_inline_toggle_cb),
		vcard_object);

	widget = gtk_button_new_with_label (_("Save in Address Book"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	/* This depends on having a source list. */
	if (vcard_object->source_list != NULL)
		gtk_widget_show (widget);
	else
		gtk_widget_hide (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (org_gnome_vcard_inline_save_cb),
		vcard_object);

	return TRUE;
}

void
org_gnome_vcard_inline_format (gpointer ep, EMFormatHookTarget *target)
{
	VCardInlinePObject *vcard_object;
	gchar *classid;

	classid = g_strdup_printf (
		"org-gnome-vcard-inline-display-%d",
		org_gnome_vcard_inline_classid++);

	vcard_object = (VCardInlinePObject *)
		em_format_html_add_pobject (
			EM_FORMAT_HTML (target->format),
			sizeof (VCardInlinePObject),
			classid, target->part,
			org_gnome_vcard_inline_embed);

	g_object_ref (target->part);

	vcard_object->object.free = org_gnome_vcard_inline_pobject_free;
	org_gnome_vcard_inline_decode (vcard_object, target->part);

	e_book_get_addressbooks (&vcard_object->source_list, NULL);

	camel_stream_printf (
		target->stream, "<object classid=%s></object>", classid);

	g_free (classid);
}
